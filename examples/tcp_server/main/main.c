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
#define EXAMPLE_ACCEPT_TIMEOUT_MS 10000
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

static const char *TAG = "tcp_server_example";
static const uint16_t EXAMPLE_LISTEN_PORT = 5000;

// Keep large networking buffers out of task stack.
static esp_wiz_toe_spi_config_t s_spi_cfg;
static uint8_t s_tx_buf[8];
static uint8_t s_rx_buf[8];
static uint8_t s_loopback_buf[EXAMPLE_LOOPBACK_BUF_SIZE];
static bool s_link_up = false;
static bool s_link_was_up = false;

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
    cfg->lock_timeout_ms = EXAMPLE_ACCEPT_TIMEOUT_MS;
}

int32_t loopback_tcps(uint8_t sn, uint8_t* buf, uint16_t port) {
    int32_t ret;
    uint16_t size = 0, sentsize = 0;
    uint8_t destip[4];
    uint16_t destport;

    switch (getSn_SR(sn)) {
    case SOCK_ESTABLISHED :
        if (getSn_IR(sn) & Sn_IR_CON) {
            getSn_DIPR(sn, destip);
            destport = getSn_DPORT(sn);

            ESP_LOGI(TAG,"%d:Connected - %d.%d.%d.%d : %d\r\n", sn, destip[0], destip[1], destip[2], destip[3], destport);
            setSn_IR(sn, Sn_IR_CON);
        }
        if ((size = getSn_RX_RSR(sn)) > 0) { // Don't need to check SOCKERR_BUSY because it doesn't not occur.
            if (size > EXAMPLE_LOOPBACK_BUF_SIZE) {
                size = EXAMPLE_LOOPBACK_BUF_SIZE;
            }
            ret = recv(sn, buf, size);

            if (ret <= 0) {
                return ret;    // check SOCKERR_BUSY & SOCKERR_XXX. For showing the occurrence of SOCKERR_BUSY.
            }
            size = (uint16_t) ret;
            sentsize = 0;

            while (size != sentsize) {
                ret = send(sn, buf + sentsize, size - sentsize);
                if (ret < 0) {
                    close(sn);
                    return ret;
                }
                sentsize += ret; // Don't care SOCKERR_BUSY, because it is zero.
            }
        }
        break;
    case SOCK_CLOSE_WAIT :
        if ((ret = disconnect(sn)) != SOCK_OK) {
            return ret;
        }
        ESP_LOGI(TAG,"%d:Socket Closed\r\n", sn);
        break;
    case SOCK_INIT :
        ESP_LOGI(TAG,"%d:Listen, TCP server loopback, port [%d]\r\n", sn, port);
        if ((ret = listen(sn)) != SOCK_OK) {
            return ret;
        }
        break;
    case SOCK_CLOSED:
        if ((ret = socket(sn, Sn_MR_TCP, port, 0x00)) != sn) {
            return ret;
        }
        ESP_LOGI(TAG, "%d:Socket opened\r\n",sn);
        break;
    default:
        break;
    }
    return 1;
}

static void tcp_server_task(void *arg)
{
    (void)arg;
    int32_t retval = 0;
    int32_t rc;

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

    if (wizchip_init(s_tx_buf, s_rx_buf) != 0) {
        ESP_LOGE(TAG, "wizchip_init failed");
        vTaskDelete(NULL);
        return;
    }

    wizchip_setnetinfo((wiz_NetInfo *)&s_net_info);

    while (true) {
        if (esp_wiz_toe_spi_link_is_up(&s_link_up) != ESP_OK || !s_link_up) {
            s_link_was_up = false;
            ESP_LOGI(TAG, "PHY link down, retrying...");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (!s_link_was_up) {
            ESP_LOGI(TAG, "PHY link up");
            s_link_was_up = true;
        }

        while(1)
        {
            if ((retval = loopback_tcps(EXAMPLE_SOCKET_NUM, s_loopback_buf, EXAMPLE_LISTEN_PORT)) < 0) {
                ESP_LOGI(TAG, " loopback_tcps error : %d\n", retval);
                while (1){vTaskDelay(pdMS_TO_TICKS(10));}
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void)
{
    // loopback_tcps opens the socket in blocking mode (socket flag 0x00), so
    // this demo can stay in long waits/retries; disable Task WDT to avoid
    // resets during bring-up and manual network testing.
    esp_task_wdt_delete(NULL);
    esp_task_wdt_deinit();

    xTaskCreate(tcp_server_task, "tcp_server_task", 8192, NULL, 5, NULL);
}
