/**
 * HTTP server example — ported from WIZnet-PICO-C examples/http/server.
 *
 * Serves index.html (web_page.h) on port 80 using the official ioLibrary
 * httpServer across 4 hardware sockets. Open http://192.168.11.2 in a
 * browser. The 1-second HTTP session timeout tick uses esp_timer.
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
#include "httpServer.h"
#include "web_page.h"

/* Buffer */
#define ETHERNET_BUF_MAX_SIZE (1024 * 2)

/* Socket */
#define HTTP_SOCKET_MAX_NUM 4

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

/* HTTP */
static uint8_t g_http_send_buf[ETHERNET_BUF_MAX_SIZE];
static uint8_t g_http_recv_buf[ETHERNET_BUF_MAX_SIZE];
static uint8_t g_http_socket_num_list[HTTP_SOCKET_MAX_NUM] = {0, 1, 2, 3};

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

    printf("HTTP server: http://%d.%d.%d.%d\n",
           g_net_info.ip[0], g_net_info.ip[1], g_net_info.ip[2], g_net_info.ip[3]);
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

/* 1-second tick for the HTTP session timeout */
static void repeating_timer_callback(void *arg)
{
    (void)arg;
    httpServer_time_handler();
}

static void http_server_task(void *arg)
{
    (void)arg;
    uint8_t i;

    wizchip_port_initialize();

    const esp_timer_create_args_t timer_args = {
        .callback = repeating_timer_callback,
        .name = "http_1s",
    };
    esp_timer_handle_t timer;
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, 1000 * 1000)); // 1 second

    httpServer_init(g_http_send_buf, g_http_recv_buf, HTTP_SOCKET_MAX_NUM, g_http_socket_num_list);

    /* Register web page */
    reg_httpServer_webContent((uint8_t *)"index.html", (uint8_t *)index_page);

    /* Infinite loop */
    while (1) {
        /* Run HTTP server */
        for (i = 0; i < HTTP_SOCKET_MAX_NUM; i++) {
            httpServer_run(i);
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

void app_main(void)
{
    // Sockets are opened in blocking mode; disable Task WDT to avoid resets
    // during long waits and manual network testing.
    esp_task_wdt_delete(NULL);
    esp_task_wdt_deinit();

    xTaskCreate(http_server_task, "http_server_task", 8192, NULL, 5, NULL);
}
