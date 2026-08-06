/**
 * PPPoE example — ported from WIZnet-PICO-C examples/pppoe.
 *
 * Establishes a PPPoE session (e.g. against an ISP modem or a test PPPoE
 * server) and prints the assigned IP. PPPoE.c/md5.c are carried with the
 * example, same as in the original; set pppoe_id / pppoe_pw below.
 *
 * Works with W5500 or W6300 — select the chip in menuconfig:
 *   Component config -> WIZnet TOE Component -> WIZnet chip
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_task_wdt.h"
#include "esp_wiz_toe.h"
#include "esp_wiz_toe/Ethernet/socket.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wizchip_conf.h"

/* The vendored PPPoE.c uses W5100S/W5500 PPPoE registers (getMR/getPSID/...),
 * which do not exist on W6300 (it exposes PPPoE through a different register
 * map: PSIDR/PHAR/NETMR2). This example is therefore W5500-only. */
#if (_WIZCHIP_ != W5500)
#error "The pppoe example supports W5500 only (W6300 uses a different PPPoE register map)."
#endif

#include "PPPoE.h"

#ifndef DATA_BUF_SIZE
#define DATA_BUF_SIZE 2048
#endif

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

/* PPPoE — referenced by PPPoE.c (same globals as the original example) */
uint8_t gDATABUF[DATA_BUF_SIZE];
uint8_t pppoe_id[6] = "W5100S";
uint8_t pppoe_id_len = 6;
uint8_t pppoe_pw[6] = "WIZnet";
uint8_t pppoe_pw_len = 6;
uint8_t pppoe_ip[4];
uint16_t pppoe_retry_count = 0;

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

static void pppoe_task(void *arg)
{
    (void)arg;
    int32_t ret = 0;
    uint8_t str[15];

    wizchip_port_initialize();

    printf("wiznet chip PPPOE example.\r\n");

    while (1) {
        ret = ppp_start(gDATABUF); // ppp start function
        if (ret == PPP_SUCCESS || pppoe_retry_count > PPP_MAX_RETRY_COUNT) {
            break; // PPPoE Connected or connect failed by over retry count
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (ret == PPP_SUCCESS) { // 1 : success
        printf("\r\n<<<< PPPoE Success >>>>\r\n");
        printf("Assigned IP address : %d.%d.%d.%d\r\n", pppoe_ip[0], pppoe_ip[1], pppoe_ip[2], pppoe_ip[3]);
        printf("\r\n==================================================\r\n");
        printf("    AFTER PPPoE, Net Configuration Information        \r\n");
        printf("==================================================\r\n");
        getSHAR(str);
        printf("MAC address  : %x:%x:%x:%x:%x:%x\r\n", str[0], str[1], str[2], str[3], str[4], str[5]);
        getSUBR(str);
        printf("SUBNET MASK  : %d.%d.%d.%d\r\n", str[0], str[1], str[2], str[3]);
        getGAR(str);
        printf("G/W IP ADDRESS : %d.%d.%d.%d\r\n", str[0], str[1], str[2], str[3]);
        getSIPR(str);
        printf("SOURCE IP ADDRESS : %d.%d.%d.%d\r\n\r\n", str[0], str[1], str[2], str[3]);
    } else { // failed
        printf("\r\n<<<< PPPoE Failed >>>>\r\n");
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    // Sockets are opened in blocking mode; disable Task WDT to avoid resets
    // during long waits and manual network testing.
    esp_task_wdt_delete(NULL);
    esp_task_wdt_deinit();

    xTaskCreate(pppoe_task, "pppoe_task", 8192, NULL, 5, NULL);
}
