/**
 * Loopback example — ported from WIZnet-PICO-C examples/loopback.
 *
 * TCP server loopback on port 5000 by default; enable the TCP client / UDP
 * variants with the defines below, same as the original example.
 *
 * Works with W5500 or W6300 — select the chip in menuconfig:
 *   Component config -> WIZnet TOE Component -> WIZnet chip
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_wiz_toe.h"
#include "esp_wiz_toe/Ethernet/socket.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wizchip_conf.h"
#include "loopback.h"
#if _WIZCHIP_ > W5500
#include "Application/Application.h"
#endif

/* Buffer */
#define ETHERNET_BUF_MAX_SIZE (1024 * 2)

/* Socket */
#define SOCKET_TCP_SERVER 0
#define SOCKET_TCP_CLIENT 1
#define SOCKET_UDP 2

/* Port */
#define PORT_TCP_SERVER 5000
#define PORT_TCP_CLIENT 5001
#define PORT_TCP_CLIENT_DEST 5002
#define PORT_UDP 5003

/* Select the loopback variants to run (same switches as WIZnet-PICO-C) */
#define TCP_SERVER
// #define TCP_CLIENT
// #define UDP

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

static uint8_t g_tcp_client_destip[] = {192, 168, 11, 100};
static uint16_t g_tcp_client_destport = PORT_TCP_CLIENT_DEST;

/* Loopback buffers — keep large networking buffers out of task stack */
static uint8_t g_tcp_server_buf[ETHERNET_BUF_MAX_SIZE];
static uint8_t g_tcp_client_buf[ETHERNET_BUF_MAX_SIZE];
static uint8_t g_udp_buf[ETHERNET_BUF_MAX_SIZE];

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
#if _WIZCHIP_ > W5500
    set_loopback_mode_W6x00(AS_IPV4);
#endif

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

static void loopback_task(void *arg)
{
    (void)arg;
    int32_t retval = 0;

    wizchip_port_initialize();

    /* Infinite loop */
    while (1) {
#ifdef TCP_SERVER
        /* TCP server loopback test */
        if ((retval = loopback_tcps(SOCKET_TCP_SERVER, g_tcp_server_buf, PORT_TCP_SERVER)) < 0) {
            printf(" loopback_tcps error : %d\n", (int)retval);
            close(SOCKET_TCP_SERVER);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
#endif
#ifdef TCP_CLIENT
        /* TCP client loopback test */
        if ((retval = loopback_tcpc(SOCKET_TCP_CLIENT, g_tcp_client_buf, g_tcp_client_destip, g_tcp_client_destport)) < 0) {
            printf(" loopback_tcpc error : %d\n", (int)retval);
            close(SOCKET_TCP_CLIENT);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
#endif
#ifdef UDP
        /* UDP loopback test */
        if ((retval = loopback_udps(SOCKET_UDP, g_udp_buf, PORT_UDP)) < 0) {
            printf(" loopback_udps error : %d\n", (int)retval);
            close(SOCKET_UDP);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
#endif
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void)
{
    xTaskCreate(loopback_task, "loopback_task", 16384, NULL, 5, NULL);
}
