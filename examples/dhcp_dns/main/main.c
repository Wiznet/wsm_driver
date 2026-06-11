/**
 * DHCP & DNS example — ported from WIZnet-PICO-C examples/dhcp_dns.
 *
 * Obtains an IP address via DHCP, then resolves www.wiznet.io via DNS.
 * The 1-second DHCP/DNS tick uses esp_timer instead of the Pico repeating
 * timer.
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
#include "dhcp.h"
#include "dns.h"

/* Buffer */
#define ETHERNET_BUF_MAX_SIZE (1024 * 2)

/* Socket */
#define SOCKET_DHCP 0
#define SOCKET_DNS 1

/* Retry count */
#define DHCP_RETRY_COUNT 5
#define DNS_RETRY_COUNT 5

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
static wiz_NetInfo g_net_info = {
    .mac = {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}, // MAC address
    .ip = {192, 168, 11, 2},                     // IP address
    .sn = {255, 255, 255, 0},                    // Subnet Mask
    .gw = {192, 168, 11, 1},                     // Gateway
    .dns = {8, 8, 8, 8},                         // DNS server
#if _WIZCHIP_ > W5500
    .ipmode = NETINFO_STATIC_ALL,
#endif
    .dhcp = NETINFO_DHCP,
};

static uint8_t g_ethernet_buf[ETHERNET_BUF_MAX_SIZE]; // common buffer

/* DHCP */
static uint8_t g_dhcp_get_ip_flag = 0;

/* DNS */
static uint8_t g_dns_target_domain[] = "www.wiznet.io";
static uint8_t g_dns_target_ip[4];
static uint8_t g_dns_get_ip_flag = 0;

static esp_wiz_toe_spi_config_t g_spi_cfg;
static uint8_t g_buf_size_tx[_WIZCHIP_SOCK_NUM_];
static uint8_t g_buf_size_rx[_WIZCHIP_SOCK_NUM_];

static void print_network_information(const wiz_NetInfo *info)
{
    printf(" ip  : %d.%d.%d.%d\n", info->ip[0], info->ip[1], info->ip[2], info->ip[3]);
    printf(" sn  : %d.%d.%d.%d\n", info->sn[0], info->sn[1], info->sn[2], info->sn[3]);
    printf(" gw  : %d.%d.%d.%d\n", info->gw[0], info->gw[1], info->gw[2], info->gw[3]);
    printf(" dns : %d.%d.%d.%d\n", info->dns[0], info->dns[1], info->dns[2], info->dns[3]);
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
}

/* DHCP */
static void wizchip_dhcp_assign(void)
{
    getIPfromDHCP(g_net_info.ip);
    getGWfromDHCP(g_net_info.gw);
    getSNfromDHCP(g_net_info.sn);
    getDNSfromDHCP(g_net_info.dns);

    g_net_info.dhcp = NETINFO_DHCP;

    /* Network initialize */
    wizchip_setnetinfo(&g_net_info); // apply from DHCP
    print_network_information(&g_net_info);
    printf(" DHCP leased time : %ld seconds\n", (long)getDHCPLeasetime());
}

static void wizchip_dhcp_conflict(void)
{
    printf(" Conflict IP from DHCP\n");

    // halt or reset or any...
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void wizchip_dhcp_init(void)
{
    printf(" DHCP client running\n");
    DHCP_init(SOCKET_DHCP, g_ethernet_buf);
    reg_dhcp_cbfunc(wizchip_dhcp_assign, wizchip_dhcp_assign, wizchip_dhcp_conflict);
}

/* 1-second tick for the DHCP/DNS retransmission timers */
static void repeating_timer_callback(void *arg)
{
    (void)arg;
    DHCP_time_handler();
    DNS_time_handler();
}

static void dhcp_dns_task(void *arg)
{
    (void)arg;
    uint8_t retval = 0;
    uint8_t dhcp_retry = 0;
    uint8_t dns_retry = 0;

    wizchip_port_initialize();

    vTaskDelay(pdMS_TO_TICKS(2000));

    const esp_timer_create_args_t timer_args = {
        .callback = repeating_timer_callback,
        .name = "dhcp_dns_1s",
    };
    esp_timer_handle_t timer;
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, 1000 * 1000)); // 1 second

    if (g_net_info.dhcp == NETINFO_DHCP) { // DHCP
        wizchip_dhcp_init();
    } else { // static
        wizchip_setnetinfo(&g_net_info);
        print_network_information(&g_net_info);
    }

    DNS_init(SOCKET_DNS, g_ethernet_buf);

    /* Infinite loop */
    while (1) {
        /* Assigned IP through DHCP */
        if (g_net_info.dhcp == NETINFO_DHCP) {
            retval = DHCP_run();

            if (retval == DHCP_IP_LEASED) {
                if (g_dhcp_get_ip_flag == 0) {
                    printf(" DHCP success\n");
                    g_dhcp_get_ip_flag = 1;
                }
            } else if (retval == DHCP_FAILED) {
                g_dhcp_get_ip_flag = 0;
                dhcp_retry++;

                if (dhcp_retry <= DHCP_RETRY_COUNT) {
                    printf(" DHCP timeout occurred and retry %d\n", dhcp_retry);
                }
            }

            if (dhcp_retry > DHCP_RETRY_COUNT) {
                printf(" DHCP failed\n");
                DHCP_stop();
                while (1) {
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
            }
            vTaskDelay(pdMS_TO_TICKS(1000)); // wait for 1 second
        }

        /* Get IP through DNS */
        if ((g_dns_get_ip_flag == 0) && (retval == DHCP_IP_LEASED)) {
            while (1) {
                if (DNS_run(g_net_info.dns, g_dns_target_domain, g_dns_target_ip) > 0) {
                    printf(" DNS success\n");
                    printf(" Target domain : %s\n", g_dns_target_domain);
                    printf(" IP of target domain : %d.%d.%d.%d\n",
                           g_dns_target_ip[0], g_dns_target_ip[1], g_dns_target_ip[2], g_dns_target_ip[3]);
                    g_dns_get_ip_flag = 1;
                    break;
                } else {
                    dns_retry++;
                    if (dns_retry <= DNS_RETRY_COUNT) {
                        printf(" DNS timeout occurred and retry %d\n", dns_retry);
                    }
                }

                if (dns_retry > DNS_RETRY_COUNT) {
                    printf(" DNS failed\n");
                    while (1) {
                        vTaskDelay(pdMS_TO_TICKS(1000));
                    }
                }
                vTaskDelay(pdMS_TO_TICKS(1000)); // wait for 1 second
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

    xTaskCreate(dhcp_dns_task, "dhcp_dns_task", 8192, NULL, 5, NULL);
}
