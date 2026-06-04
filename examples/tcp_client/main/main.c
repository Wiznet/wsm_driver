#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_wiz_toe.h"
#include "esp_wiz_toe/Ethernet/socket.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wizchip_conf.h"

#define EXAMPLE_SOCKET_NUM 0
#define EXAMPLE_IO_TIMEOUT_MS 5000
#define EXAMPLE_LOOPBACK_BUF_SIZE (1024 * 2)

#ifdef CONFIG_ESP_WIZ_TOE_TX_BUF_KB
#define EXAMPLE_TX_BUF_KB CONFIG_ESP_WIZ_TOE_TX_BUF_KB
#else
#define EXAMPLE_TX_BUF_KB 2
#endif

#ifdef CONFIG_ESP_WIZ_TOE_RX_BUF_KB
#define EXAMPLE_RX_BUF_KB CONFIG_ESP_WIZ_TOE_RX_BUF_KB
#else
#define EXAMPLE_RX_BUF_KB 2
#endif

static const char *TAG = "tcp_client_example";
static const uint8_t EXAMPLE_SERVER_IP[4] = {192, 168, 11, 100};
static const uint16_t EXAMPLE_SERVER_PORT = 5000;

// Keep large networking buffers out of task stack.
static esp_wiz_toe_spi_config_t s_spi_cfg;
static uint8_t s_tx_buf[8];
static uint8_t s_rx_buf[8];
static uint8_t s_loopback_buf[EXAMPLE_LOOPBACK_BUF_SIZE];
static bool s_link_up = false;
static bool s_link_was_up = false;
static uint16_t s_any_port = 50000;

static const wiz_NetInfo s_net_info = {
    .mac = {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56},
    .ip = {192, 168, 11, 2},
    .sn = {255, 255, 255, 0},
    .gw = {192, 168, 11, 1},
    .dns = {8, 8, 8, 8},
    .dhcp = NETINFO_STATIC,
};

static void fill_spi_config(esp_wiz_toe_spi_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));

    cfg->host_id = (spi_host_device_t)CONFIG_ESP_WIZ_TOE_SPI_HOST;
    cfg->clock_hz = CONFIG_ESP_WIZ_TOE_SPI_CLOCK_HZ;
    cfg->pin_miso = (gpio_num_t)CONFIG_ESP_WIZ_TOE_PIN_MISO;
    cfg->pin_mosi = (gpio_num_t)CONFIG_ESP_WIZ_TOE_PIN_MOSI;
    cfg->pin_sclk = (gpio_num_t)CONFIG_ESP_WIZ_TOE_PIN_SCLK;
    cfg->pin_cs = (gpio_num_t)CONFIG_ESP_WIZ_TOE_PIN_CS;
    cfg->pin_int = (gpio_num_t)CONFIG_ESP_WIZ_TOE_PIN_INT;
    cfg->pin_rst = (gpio_num_t)CONFIG_ESP_WIZ_TOE_PIN_RST;
    cfg->lock_timeout_ms = EXAMPLE_IO_TIMEOUT_MS;
}

int32_t loopback_tcpc(uint8_t sn, uint8_t* buf, uint8_t* destip, uint16_t destport) {
    int32_t ret; // return value for SOCK_ERRORs
    uint16_t size = 0, sentsize = 0;

    // Destination (TCP Server) IP info (will be connected)
    // >> loopback_tcpc() function parameter
    // >> Ex)
    //	uint8_t destip[4] = 	{192, 168, 0, 214};
    //	uint16_t destport = 	5000;

    // Port number for TCP client (will be increased)
    static uint16_t any_port = 	50000;

    // Socket Status Transitions
    // Check the W5500 Socket n status register (Sn_SR, The 'Sn_SR' controlled by Sn_CR command or Packet send/recv status)
    switch (getSn_SR(sn)) {
    case SOCK_ESTABLISHED :
        if (getSn_IR(sn) & Sn_IR_CON) {	// Socket n interrupt register mask; TCP CON interrupt = connection with peer is successful
            ESP_LOGI(TAG, "%d:Connected to - %d.%d.%d.%d : %d\r\n", sn, destip[0], destip[1], destip[2], destip[3], destport);
            setSn_IR(sn, Sn_IR_CON);  // this interrupt should be write the bit cleared to '1'
        }

        //////////////////////////////////////////////////////////////////////////////////////////////
        // Data Transaction Parts; Handle the [data receive and send] process
        //////////////////////////////////////////////////////////////////////////////////////////////
        if ((size = getSn_RX_RSR(sn)) > 0) { // Sn_RX_RSR: Socket n Received Size Register, Receiving data length
            if (size > EXAMPLE_LOOPBACK_BUF_SIZE) {
                size = EXAMPLE_LOOPBACK_BUF_SIZE;
            }
            ret = recv(sn, buf, size); // Data Receive process (H/W Rx socket buffer -> User's buffer)

            if (ret <= 0) {
                return ret;
            }
            size = (uint16_t) ret;
            sentsize = 0;

            // Data sentsize control
            while (size != sentsize) {
                ret = send(sn, buf + sentsize, size - sentsize); // Data send process (User's buffer -> Destination through H/W Tx socket buffer)
                if (ret < 0) { // Send Error occurred (sent data length < 0)
                    close(sn); // socket close
                    return ret;
                }
                sentsize += ret; // Don't care SOCKERR_BUSY, because it is zero.
            }
        }
        //////////////////////////////////////////////////////////////////////////////////////////////
        break;

    case SOCK_CLOSE_WAIT :
        if ((ret = disconnect(sn)) != SOCK_OK) {
            return ret;
        }
        ESP_LOGI(TAG, "%d:Socket Closed\r\n", sn);
        break;

    case SOCK_INIT :
        ESP_LOGI(TAG, "%d:Try to connect to the %d.%d.%d.%d : %d\r\n", sn, destip[0], destip[1], destip[2], destip[3], destport);
        if ((ret = connect(sn, destip, destport)) != SOCK_OK) {
            return ret;    //	Try to TCP connect to the TCP server (destination)
        }
        break;

    case SOCK_CLOSED:
        close(sn);
        if ((ret = socket(sn, Sn_MR_TCP, any_port++, 0)  != sn)) {    
            if (any_port == 0xffff) {
                any_port = 50000;
            }
            return ret; // TCP socket open with 'any_port' port number
        }
        break;
    default:
        break;
    }
    return 1;
}


static void tcp_client_task(void *arg)
{
    (void)arg;
    int32_t retval = 0;
    int32_t rc = 0;
    uint32_t connect_busy_count = 0;

    s_tx_buf[0] = EXAMPLE_TX_BUF_KB;
    s_tx_buf[1] = EXAMPLE_TX_BUF_KB;
    s_tx_buf[2] = EXAMPLE_TX_BUF_KB;
    s_tx_buf[3] = EXAMPLE_TX_BUF_KB;
    s_tx_buf[4] = EXAMPLE_TX_BUF_KB;
    s_tx_buf[5] = EXAMPLE_TX_BUF_KB;
    s_tx_buf[6] = EXAMPLE_TX_BUF_KB;
    s_tx_buf[7] = EXAMPLE_TX_BUF_KB;

    s_rx_buf[0] = EXAMPLE_RX_BUF_KB;
    s_rx_buf[1] = EXAMPLE_RX_BUF_KB;
    s_rx_buf[2] = EXAMPLE_RX_BUF_KB;
    s_rx_buf[3] = EXAMPLE_RX_BUF_KB;
    s_rx_buf[4] = EXAMPLE_RX_BUF_KB;
    s_rx_buf[5] = EXAMPLE_RX_BUF_KB;
    s_rx_buf[6] = EXAMPLE_RX_BUF_KB;
    s_rx_buf[7] = EXAMPLE_RX_BUF_KB;

    fill_spi_config(&s_spi_cfg);

    ESP_ERROR_CHECK(esp_wiz_toe_spi_init(&s_spi_cfg));
    ESP_ERROR_CHECK(esp_wiz_toe_spi_register_iolib_callbacks());
    ESP_ERROR_CHECK(esp_wiz_toe_spi_reset());
    ESP_ERROR_CHECK(esp_wiz_toe_spi_wizchip_check());

    if (wizchip_init(s_tx_buf, s_rx_buf) != 0) {
        ESP_LOGE(TAG, "wizchip_init failed");
        vTaskDelete(NULL);
        return;
    }

    wizchip_setnetinfo((wiz_NetInfo *)&s_net_info);

    do {
        if (esp_wiz_toe_spi_link_is_up(&s_link_up) != ESP_OK) {
            ESP_LOGW(TAG, "PHY link state read failed, retrying...");
            s_link_up = false;
        } else if (!s_link_up) {
            ESP_LOGI(TAG, "PHY link down, waiting...");
        }

        if (!s_link_up) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    } while (!s_link_up);
    ESP_LOGI(TAG, "PHY link up");
    s_link_was_up = true;

    while (true) {
        if (esp_wiz_toe_spi_link_is_up(&s_link_up) != ESP_OK) {
            ESP_LOGW(TAG, "PHY link state read failed, retrying...");
            s_link_up = false;
        }

        if (!s_link_up) {
            s_link_was_up = false;
            ESP_LOGI(TAG, "PHY link down, waiting...");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (!s_link_was_up) {
            ESP_LOGI(TAG, "PHY link up");
            s_link_was_up = true;
        }

        while(1)
        {
            if ((retval = loopback_tcpc(EXAMPLE_SOCKET_NUM, s_loopback_buf, (uint8_t *)EXAMPLE_SERVER_IP, EXAMPLE_SERVER_PORT)) < 0) {
                ESP_LOGI(TAG, " loopback_tcpc error : %d\n", retval);
                while (1){vTaskDelay(pdMS_TO_TICKS(10));}
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void)
{
    // loopback_tcpc opens the socket in blocking mode (socket flag 0), so this
    // demo can stay in long waits/retries; disable Task WDT to avoid resets
    // during bring-up and manual network testing.
    esp_task_wdt_delete(NULL);
    esp_task_wdt_deinit();

    xTaskCreate(tcp_client_task, "tcp_client_task", 8192, NULL, 5, NULL);
}
