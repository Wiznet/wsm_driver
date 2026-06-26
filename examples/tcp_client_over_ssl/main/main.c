/**
 * TCP client over SSL example — ported from WIZnet-PICO-C
 * examples/tcp_client_over_ssl.
 *
 * Opens a TCP socket on the WIZnet chip, then runs an mbedTLS client over it
 * by wiring mbedTLS's BIO to the WIZnet socket send()/recv(). Certificate
 * verification is disabled (VERIFY_NONE) to keep the demo dependency-free,
 * same as the original. Point g_ssl_target_ip at your TLS server (port 443).
 *
 * mbedTLS is provided by ESP-IDF (v6: mbedTLS 4.0 / PSA crypto). RNG is
 * handled internally by PSA — no explicit RNG callback needed.
 *
 * Works with W5500 or W6300 — select the chip in menuconfig:
 *   Component config -> WIZnet TOE Component -> WIZnet chip
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_timer.h"
#include "esp_wiz_toe.h"
#include "esp_wiz_toe/Ethernet/socket.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wizchip_conf.h"

#include "mbedtls/x509_crt.h"
#include "mbedtls/error.h"
#include "mbedtls/ssl.h"

/* Buffer */
#define ETHERNET_BUF_MAX_SIZE (1024 * 2)

/* Socket */
#define SOCKET_SSL 0

/* Port */
#define PORT_SSL     443
#define PORT_SSL_SRC 5001

/* Connect/recv timeout (ms) */
#define SSL_RECV_TIMEOUT (1000 * 10)

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

/* SSL */
static uint8_t g_ssl_buf[ETHERNET_BUF_MAX_SIZE];
static uint8_t g_ssl_target_ip[4] = {192, 168, 11, 4};

static mbedtls_ssl_config g_conf;
static mbedtls_ssl_context g_ssl;

static esp_wiz_toe_spi_config_t g_spi_cfg;
static uint8_t g_buf_size_tx[_WIZCHIP_SOCK_NUM_];
static uint8_t g_buf_size_rx[_WIZCHIP_SOCK_NUM_];

static uint32_t millis(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
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
    g_spi_cfg.lock_timeout_ms = SSL_RECV_TIMEOUT;

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
    printf("ip: %d.%d.%d.%d\n",
           g_net_info.ip[0], g_net_info.ip[1], g_net_info.ip[2], g_net_info.ip[3]);

#if _WIZCHIP_ > W5500
    /* W6300 PHY needs up to ~500 ms after reset for auto-negotiation.
     * Poll until link is up before attempting any TCP connect. */
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

/* mbedTLS BIO -> WIZnet socket glue. ctx carries the socket number. */
static int tcp_send(void *ctx, const unsigned char *buf, size_t len)
{
    return send((uint8_t)(uintptr_t)ctx, (uint8_t *)buf, (uint16_t)len);
}

static int tcp_recv(void *ctx, unsigned char *buf, size_t len)
{
    return recv((uint8_t)(uintptr_t)ctx, (uint8_t *)buf, (uint16_t)len);
}

static int recv_timeout(void *ctx, unsigned char *buf, size_t len, uint32_t timeout)
{
    uint16_t recv_len = 0;
    uint32_t start_ms = millis();

    do {
        getsockopt((uint8_t)(uintptr_t)ctx, SO_RECVBUF, &recv_len);
        if (recv_len > 0) {
            return recv((uint8_t)(uintptr_t)ctx, (uint8_t *)buf, (uint16_t)len);
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    } while ((millis() - start_ms) < timeout);

    return MBEDTLS_ERR_SSL_TIMEOUT;
}

static int wizchip_ssl_init(uint8_t socket_fd)
{
    int retval;

    mbedtls_ssl_init(&g_ssl);
    mbedtls_ssl_config_init(&g_conf);

    if ((retval = mbedtls_ssl_config_defaults(&g_conf,
                                              MBEDTLS_SSL_IS_CLIENT,
                                              MBEDTLS_SSL_TRANSPORT_STREAM,
                                              MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
        printf(" failed\n  ! mbedtls_ssl_config_defaults returned %d\n", retval);
        return -1;
    }

    mbedtls_ssl_conf_authmode(&g_conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_read_timeout(&g_conf, SSL_RECV_TIMEOUT);

    if ((retval = mbedtls_ssl_setup(&g_ssl, &g_conf)) != 0) {
        printf(" failed\n  ! mbedtls_ssl_setup returned %d\n", retval);
        return -1;
    }

    /* ctx is the WIZnet socket number, recovered the same way in the BIO */
    mbedtls_ssl_set_bio(&g_ssl, (void *)(uintptr_t)socket_fd, tcp_send, tcp_recv, recv_timeout);

    return 0;
}

static void ssl_client_task(void *arg)
{
    (void)arg;
    int32_t retval = 0;
    uint32_t start_ms = 0;

    wizchip_port_initialize();

    if (wizchip_ssl_init(SOCKET_SSL) < 0) {
        printf(" SSL initialize failed\n");
        vTaskDelete(NULL);
        return;
    }

    /* SF_FORCE_ARP (W6300): forces an ARP request before TCP CONNECT so the
     * chip resolves the destination MAC. Without it the W6300 sends SYN to a
     * null MAC when the ARP cache is empty, which is silently dropped. */
#if _WIZCHIP_ > W5500
    retval = socket(SOCKET_SSL, Sn_MR_TCP, PORT_SSL_SRC, SF_TCP_NODELAY | SF_FORCE_ARP);
#else
    retval = socket(SOCKET_SSL, Sn_MR_TCP, PORT_SSL_SRC, SF_TCP_NODELAY);
#endif
    if (retval != SOCKET_SSL) {
        printf(" Socket failed %d\n", (int)retval);
        vTaskDelete(NULL);
        return;
    }

    printf(" Connecting to %d.%d.%d.%d:%d\n",
           g_ssl_target_ip[0], g_ssl_target_ip[1], g_ssl_target_ip[2], g_ssl_target_ip[3], PORT_SSL);

#if _WIZCHIP_ > W5500
    /* Use manual CONNECT for W6300: the library connect() variadic macro
     * routes 3-arg calls to connect_W5x00(), and connect_W6x00() interacts
     * badly with SF_FORCE_ARP in sock_io_mode. Direct register writes are
     * cleaner and match what connect_IO_6 does internally. */
    setSn_DPORTR(SOCKET_SSL, PORT_SSL);
    setSn_DIPR(SOCKET_SSL, g_ssl_target_ip);
    setSn_CR(SOCKET_SSL, Sn_CR_CONNECT);
    while (getSn_CR(SOCKET_SSL));

    retval = SOCKERR_TIMEOUT;
    start_ms = millis();
    while ((millis() - start_ms) < (uint32_t)SSL_RECV_TIMEOUT) {
        uint8_t sr = getSn_SR(SOCKET_SSL);
        if (sr == SOCK_ESTABLISHED) { retval = SOCK_OK; break; }
        if (getSn_IR(SOCKET_SSL) & Sn_IR_TIMEOUT) {
            setSn_IR(SOCKET_SSL, Sn_IR_TIMEOUT);
            retval = SOCKERR_TIMEOUT;
            break;
        }
        if (sr == SOCK_CLOSED) { retval = SOCKERR_SOCKCLOSED; break; }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
#else
    start_ms = millis();
    do {
        retval = connect(SOCKET_SSL, g_ssl_target_ip, PORT_SSL);
        if ((retval == SOCK_OK) || (retval == SOCKERR_TIMEOUT)) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    } while ((millis() - start_ms) < SSL_RECV_TIMEOUT);
#endif

    if (retval != SOCK_OK) {
        printf(" Connect failed %d\n", (int)retval);
        vTaskDelete(NULL);
        return;
    }
    printf(" TCP connected, starting TLS handshake\n");

    while ((retval = mbedtls_ssl_handshake(&g_ssl)) != 0) {
        if ((retval != MBEDTLS_ERR_SSL_WANT_READ) && (retval != MBEDTLS_ERR_SSL_WANT_WRITE)) {
            printf(" failed\n  ! mbedtls_ssl_handshake returned -0x%x\n", (unsigned)-retval);
            vTaskDelete(NULL);
            return;
        }
    }

    printf(" TLS ok [ Ciphersuite: %s ]\n", mbedtls_ssl_get_ciphersuite(&g_ssl));

    memset(g_ssl_buf, 0x00, ETHERNET_BUF_MAX_SIZE);
    strcpy((char *)g_ssl_buf, " W5x00 TCP over SSL test\n");
    mbedtls_ssl_write(&g_ssl, g_ssl_buf, strlen((char *)g_ssl_buf));

    /* Infinite loop: echo back whatever the server sends */
    while (1) {
        uint16_t len = 0;
        getsockopt(SOCKET_SSL, SO_RECVBUF, &len);

        if (len > 0) {
            if (len > ETHERNET_BUF_MAX_SIZE) {
                len = ETHERNET_BUF_MAX_SIZE;
            }
            memset(g_ssl_buf, 0x00, ETHERNET_BUF_MAX_SIZE);
            mbedtls_ssl_read(&g_ssl, g_ssl_buf, len);
            printf("%s", (char *)g_ssl_buf);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void)
{
    xTaskCreate(ssl_client_task, "ssl_client_task", 16384, NULL, 5, NULL);
}
