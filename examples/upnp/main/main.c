/**
 * UPnP example — ported from WIZnet-PICO-C examples/upnp.
 *
 * Discovers an IGD (Internet Gateway Device, e.g. a home router) via SSDP,
 * then drives the serial menu (hyperterminal.c) to add/delete port mappings
 * over UPnP. UPnP.c / MakeXML.c / hyperterminal.c are carried with the
 * example, same as in the original.
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
#include "UPnP.h"
#include "hyperterminal.h"

/* Buffer */
#define ETHERNET_BUF_MAX_SIZE (1024 * 2)

/* Socket */
#define SOCKET_UPNP 0

/* Port */
#define PORT_TCP 8000
#define PORT_UDP 5000

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

/* Socket buffer sizes — same split as the original example */
static uint8_t g_tx_size[_WIZCHIP_SOCK_NUM_] = {4, 4, 2, 1, 1, 1, 1, 2};
static uint8_t g_rx_size[_WIZCHIP_SOCK_NUM_] = {4, 4, 2, 1, 1, 1, 1, 2};

/* UPNP */
static uint8_t g_upnp_buf[ETHERNET_BUF_MAX_SIZE];

static esp_wiz_toe_spi_config_t g_spi_cfg;

/* User LED stand-in: the original drives the Pico onboard LED */
static void setUserLEDStatus(uint8_t val)
{
    printf("[USER LED] %s\n", val ? "ON" : "OFF");
}

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

    ESP_ERROR_CHECK(esp_wiz_toe_spi_init(&g_spi_cfg));
    ESP_ERROR_CHECK(esp_wiz_toe_spi_register_iolib_callbacks());
    ESP_ERROR_CHECK(esp_wiz_toe_spi_reset());
    ESP_ERROR_CHECK(esp_wiz_toe_spi_wizchip_check());

    if (wizchip_init(g_tx_size, g_rx_size) != 0) {
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

static void upnp_task(void *arg)
{
    (void)arg;

    printf("wiznet chip upnp example.\r\n");

    UserLED_Control_Init(setUserLEDStatus);

    wizchip_port_initialize();

    do {
        printf("Send SSDP.. \r\n");
        vTaskDelay(pdMS_TO_TICKS(100));
    } while (SSDPProcess(SOCKET_UPNP) != 0); // SSDP Search discovery

    if (GetDescriptionProcess(SOCKET_UPNP) == 0) { // GET IGD description
        printf("GetDescription Success!!\r\n");
    } else {
        printf("GetDescription Fail!!\r\n");
    }

    if (SetEventing(SOCKET_UPNP) == 0) { // Subscribes IGD event messages
        printf("SetEventing Success!!\r\n");
    } else {
        printf("SetEventing Fail!!\r\n");
    }

    Main_Menu(SOCKET_UPNP, SOCKET_UPNP + 1, SOCKET_UPNP + 2, g_upnp_buf, PORT_TCP, PORT_UDP); // Main menu

    /* Infinite loop */
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

    xTaskCreate(upnp_task, "upnp_task", 8192, NULL, 5, NULL);
}
