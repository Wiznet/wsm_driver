/**
 * TFTP client example — ported from WIZnet-PICO-C examples/tftp.
 *
 * Reads a file from a TFTP server. Run a TFTP server (e.g. tftpd64 on
 * Windows) hosting TFTP_SERVER_FILE_NAME, then check the read result on the
 * console. The TFTP timeout tick uses esp_timer.
 *
 * Works with W5500 or W6300 — select the chip in menuconfig:
 *   Component config -> WIZnet TOE Component -> WIZnet chip
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "esp_wiz_toe.h"
#include "esp_wiz_toe/Ethernet/socket.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wizchip_conf.h"
#include "tftp.h"
#include "netutil.h"

/* Socket */
#define TFTP_SOCKET_ID 1
#define TFTP_CLIENT_SOCKET_BUFFER_SIZE 2048

/* TFTP server: change to your environment */
#define TFTP_SERVER_IP "192.168.11.4"
#define TFTP_SERVER_FILE_NAME "tftp_test_file.txt"

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

/* Network */
static const wiz_NetInfo g_net_info = {
    .mac = {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}, // MAC address
    .ip = {192, 168, 11, 2},                     // IP address
    .sn = {255, 255, 255, 0},                    // Subnet Mask
    .gw = {192, 168, 11, 1},                     // Gateway
    .dns = {8, 8, 8, 8},                         // DNS server
#if _WIZCHIP_ > W5500
    .ipmode = NETINFO_STATIC_ALL,
#endif
    .dhcp = NETINFO_STATIC,
};

static uint8_t g_tftp_socket_buf[TFTP_CLIENT_SOCKET_BUFFER_SIZE];

static esp_wiz_toe_spi_config_t g_spi_cfg;
static uint8_t g_buf_size_tx[_WIZCHIP_SOCK_NUM_];
static uint8_t g_buf_size_rx[_WIZCHIP_SOCK_NUM_];

/* WIZnet chip bring-up through the esp_wiz_toe port layer */
static void wizchip_port_initialize(void)
{
    memset(&g_spi_cfg, 0, sizeof(g_spi_cfg));
    g_spi_cfg.host_id = (spi_host_device_t)CONFIG_ESP_WIZ_TOE_SPI_HOST;
    g_spi_cfg.clock_hz = CONFIG_ESP_WIZ_TOE_SPI_CLOCK_HZ;
    g_spi_cfg.pin_miso = (gpio_num_t)CONFIG_ESP_WIZ_TOE_PIN_MISO;
    g_spi_cfg.pin_mosi = (gpio_num_t)CONFIG_ESP_WIZ_TOE_PIN_MOSI;
    g_spi_cfg.pin_sclk = (gpio_num_t)CONFIG_ESP_WIZ_TOE_PIN_SCLK;
    g_spi_cfg.pin_cs = (gpio_num_t)CONFIG_ESP_WIZ_TOE_PIN_CS;
    g_spi_cfg.pin_int = (gpio_num_t)CONFIG_ESP_WIZ_TOE_PIN_INT;
    g_spi_cfg.pin_rst = (gpio_num_t)CONFIG_ESP_WIZ_TOE_PIN_RST;
#ifdef CONFIG_ESP_WIZ_TOE_PIN_IO2
    g_spi_cfg.pin_io2 = (gpio_num_t)CONFIG_ESP_WIZ_TOE_PIN_IO2;
#else
    g_spi_cfg.pin_io2 = GPIO_NUM_NC;
#endif
#ifdef CONFIG_ESP_WIZ_TOE_PIN_IO3
    g_spi_cfg.pin_io3 = (gpio_num_t)CONFIG_ESP_WIZ_TOE_PIN_IO3;
#else
    g_spi_cfg.pin_io3 = GPIO_NUM_NC;
#endif
    g_spi_cfg.lock_timeout_ms = 5000;

    memset(g_buf_size_tx, EXAMPLE_TX_BUF_KB, sizeof(g_buf_size_tx));
    memset(g_buf_size_rx, EXAMPLE_RX_BUF_KB, sizeof(g_buf_size_rx));

    ESP_ERROR_CHECK(esp_wiz_toe_spi_init(&g_spi_cfg));
    ESP_ERROR_CHECK(esp_wiz_toe_spi_register_iolib_callbacks());
    ESP_ERROR_CHECK(esp_wiz_toe_spi_reset());
    ESP_ERROR_CHECK(esp_wiz_toe_spi_wizchip_check());

    if (wizchip_init(g_buf_size_tx, g_buf_size_rx) != 0) {
        printf("wizchip_init failed\n");
        abort();
    }

    wizchip_setnetinfo((wiz_NetInfo *)&g_net_info);

    printf("ip: %d.%d.%d.%d\n", g_net_info.ip[0], g_net_info.ip[1], g_net_info.ip[2], g_net_info.ip[3]);
#if _WIZCHIP_ > W5500
    {
        uint8_t physr;
        uint32_t waited = 0;
        do {
            physr = getPHYSR();
            if (physr & PHYSR_LNK) break;
            vTaskDelay(pdMS_TO_TICKS(100));
            waited += 100;
        } while (waited < 3000);
        if (!(physr & PHYSR_LNK)) {
            printf("PHY link down after 3 s — check cable\n");
        }
    }
#endif
}

/* 1-second tick for the TFTP retransmission timer */
static void repeating_timer_callback(void *arg)
{
    (void)arg;
    tftp_timeout_handler();
}

static void tftp_task(void *arg)
{
    (void)arg;
    int tftp_state;
    uint8_t tftp_read_flag = 0;
    uint32_t tftp_server_ip = inet_addr((uint8_t *)TFTP_SERVER_IP);

    wizchip_port_initialize();

    const esp_timer_create_args_t timer_args = {
        .callback = repeating_timer_callback,
        .name = "tftp_1s",
    };
    esp_timer_handle_t timer;
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, 1000 * 1000)); // 1 second

    TFTP_init(TFTP_SOCKET_ID, g_tftp_socket_buf);

    /* Infinite loop */
    while (1) {
        if (tftp_read_flag == 0) {
            printf("tftp server ip: %s, file name: %s\r\n", TFTP_SERVER_IP, TFTP_SERVER_FILE_NAME);
            printf("send request\r\n");
            TFTP_read_request(tftp_server_ip, (uint8_t *)TFTP_SERVER_FILE_NAME);
            tftp_read_flag = 1;
        } else {
            tftp_state = TFTP_run();

            if (tftp_state == TFTP_SUCCESS) {
                printf("tftp read success, file name: %s\r\n", TFTP_SERVER_FILE_NAME);
                while (1) {
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
            } else if (tftp_state == TFTP_FAIL) {
                printf("tftp read fail, file name: %s\r\n", TFTP_SERVER_FILE_NAME);
                while (1) {
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void)
{
    // Sockets are opened in blocking mode; disable Task WDT to avoid resets
    // during long waits and manual network testing.
    esp_task_wdt_delete(NULL);
    esp_task_wdt_deinit();

    xTaskCreate(tftp_task, "tftp_task", 8192, NULL, 5, NULL);
}
