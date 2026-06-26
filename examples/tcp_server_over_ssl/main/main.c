/**
 * TCP server over SSL example — ported from WIZnet-PICO-C
 * examples/tcp_server_over_ssl.
 *
 * Listens on a TCP socket on the WIZnet chip, then runs an mbedTLS server over
 * it by wiring mbedTLS's BIO to the WIZnet socket send()/recv(). The server
 * presents the mbedTLS PolarSSL test certificate/key (RSA) and offers the same
 * RSA-only ciphersuites as the original. Client authentication is disabled
 * (VERIFY_NONE). Each accepted client gets a greeting, then bytes are echoed
 * back; on disconnect the socket re-listens.
 *
 * mbedTLS is provided by ESP-IDF (3.6.x). RNG uses the ESP hardware RNG
 * (esp_fill_random) instead of the Pico's rand(); everything else mirrors the
 * reference.
 *
 * Works with W5500 or W6300 — select the chip in menuconfig:
 *   Component config -> WIZnet TOE Component -> WIZnet chip
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_random.h"
#include "esp_timer.h"
#include "esp_wiz_toe.h"
#include "esp_wiz_toe/Ethernet/socket.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wizchip_conf.h"

#include "mbedtls/x509_crt.h"
#include "mbedtls/error.h"
#include "mbedtls/ssl.h"
#include "mbedtls/pk.h"

/* Buffer */
#define ETHERNET_BUF_MAX_SIZE (1024 * 2)

/* Socket */
#define SOCKET_SSL 0

/* Port */
#define PORT_SSL 443

/* recv/handshake timeout (ms) */
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

static mbedtls_ssl_config g_conf;
static mbedtls_ssl_context g_ssl;
static mbedtls_x509_crt g_srv_cert;
static mbedtls_pk_context g_srv_key;

/* Server certificate + private key: mbedTLS test suite (server2-sha256.crt /
 * server2.key — CN=localhost, RSA 2048, PolarSSL Test CA). Demo credentials,
 * same as the WIZnet-PICO-C reference; replace with your own for production. */
static const unsigned char g_srv_crt_pem[] =
    "-----BEGIN CERTIFICATE-----\r\n"
    "MIIDNzCCAh+gAwIBAgIBAjANBgkqhkiG9w0BAQsFADA7MQswCQYDVQQGEwJOTDER\r\n"
    "MA8GA1UECgwIUG9sYXJTU0wxGTAXBgNVBAMMEFBvbGFyU1NMIFRlc3QgQ0EwHhcN\r\n"
    "MTkwMjEwMTQ0NDA2WhcNMjkwMjEwMTQ0NDA2WjA0MQswCQYDVQQGEwJOTDERMA8G\r\n"
    "A1UECgwIUG9sYXJTU0wxEjAQBgNVBAMMCWxvY2FsaG9zdDCCASIwDQYJKoZIhvcN\r\n"
    "AQEBBQADggEPADCCAQoCggEBAMFNo93nzR3RBNdJcriZrA545Do8Ss86ExbQWuTN\r\n"
    "owCIp+4ea5anUrSQ7y1yej4kmvy2NKwk9XfgJmSMnLAofaHa6ozmyRyWvP7BBFKz\r\n"
    "NtSj+uGxdtiQwWG0ZlI2oiZTqqt0Xgd9GYLbKtgfoNkNHC1JZvdbJXNG6AuKT2kM\r\n"
    "tQCQ4dqCEGZ9rlQri2V5kaHiYcPNQEkI7mgM8YuG0ka/0LiqEQMef1aoGh5EGA8P\r\n"
    "hYvai0Re4hjGYi/HZo36Xdh98yeJKQHFkA4/J/EwyEoO79bex8cna8cFPXrEAjya\r\n"
    "HT4P6DSYW8tzS1KW2BGiLICIaTla0w+w3lkvEcf36hIBMJcCAwEAAaNNMEswCQYD\r\n"
    "VR0TBAIwADAdBgNVHQ4EFgQUpQXoZLjc32APUBJNYKhkr02LQ5MwHwYDVR0jBBgw\r\n"
    "FoAUtFrkpbPe0lL2udWmlQ/rPrzH/f8wDQYJKoZIhvcNAQELBQADggEBAC465FJh\r\n"
    "Pqel7zJngHIHJrqj/wVAxGAFOTF396XKATGAp+HRCqJ81Ry60CNK1jDzk8dv6M6U\r\n"
    "HoS7RIFiM/9rXQCbJfiPD5xMTejZp5n5UYHAmxsxDaazfA5FuBhkfokKK6jD4Eq9\r\n"
    "1C94xGKb6X4/VkaPF7cqoBBw/bHxawXc0UEPjqayiBpCYU/rJoVZgLqFVP7Px3sv\r\n"
    "a1nOrNx8rPPI1hJ+ZOg8maiPTxHZnBVLakSSLQy/sWeWyazO1RnrbxjrbgQtYKz0\r\n"
    "e3nwGpu1w13vfckFmUSBhHXH7AAS/HpKC4IH7G2GAk3+n8iSSN71sZzpxonQwVbo\r\n"
    "pMZqLmbBm/7WPLc=\r\n"
    "-----END CERTIFICATE-----\r\n";

static const unsigned char g_srv_key_pem[] =
    "-----BEGIN RSA PRIVATE KEY-----\r\n"
    "MIIEpAIBAAKCAQEAwU2j3efNHdEE10lyuJmsDnjkOjxKzzoTFtBa5M2jAIin7h5r\r\n"
    "lqdStJDvLXJ6PiSa/LY0rCT1d+AmZIycsCh9odrqjObJHJa8/sEEUrM21KP64bF2\r\n"
    "2JDBYbRmUjaiJlOqq3ReB30Zgtsq2B+g2Q0cLUlm91slc0boC4pPaQy1AJDh2oIQ\r\n"
    "Zn2uVCuLZXmRoeJhw81ASQjuaAzxi4bSRr/QuKoRAx5/VqgaHkQYDw+Fi9qLRF7i\r\n"
    "GMZiL8dmjfpd2H3zJ4kpAcWQDj8n8TDISg7v1t7HxydrxwU9esQCPJodPg/oNJhb\r\n"
    "y3NLUpbYEaIsgIhpOVrTD7DeWS8Rx/fqEgEwlwIDAQABAoIBAQCXR0S8EIHFGORZ\r\n"
    "++AtOg6eENxD+xVs0f1IeGz57Tjo3QnXX7VBZNdj+p1ECvhCE/G7XnkgU5hLZX+G\r\n"
    "Z0jkz/tqJOI0vRSdLBbipHnWouyBQ4e/A1yIJdlBtqXxJ1KE/ituHRbNc4j4kL8Z\r\n"
    "/r6pvwnTI0PSx2Eqs048YdS92LT6qAv4flbNDxMn2uY7s4ycS4Q8w1JXnCeaAnYm\r\n"
    "WYI5wxO+bvRELR2Mcz5DmVnL8jRyml6l6582bSv5oufReFIbyPZbQWlXgYnpu6He\r\n"
    "GTc7E1zKYQGG/9+DQUl/1vQuCPqQwny0tQoX2w5tdYpdMdVm+zkLtbajzdTviJJa\r\n"
    "TWzL6lt5AoGBAN86+SVeJDcmQJcv4Eq6UhtRr4QGMiQMz0Sod6ettYxYzMgxtw28\r\n"
    "CIrgpozCc+UaZJLo7UxvC6an85r1b2nKPCLQFaggJ0H4Q0J/sZOhBIXaoBzWxveK\r\n"
    "nupceKdVxGsFi8CDy86DBfiyFivfBj+47BbaQzPBj7C4rK7UlLjab2rDAoGBAN2u\r\n"
    "AM2gchoFiu4v1HFL8D7lweEpi6ZnMJjnEu/dEgGQJFjwdpLnPbsj4c75odQ4Gz8g\r\n"
    "sw9lao9VVzbusoRE/JGI4aTdO0pATXyG7eG1Qu+5Yc1YGXcCrliA2xM9xx+d7f+s\r\n"
    "mPzN+WIEg5GJDYZDjAzHG5BNvi/FfM1C9dOtjv2dAoGAF0t5KmwbjWHBhcVqO4Ic\r\n"
    "BVvN3BIlc1ue2YRXEDlxY5b0r8N4XceMgKmW18OHApZxfl8uPDauWZLXOgl4uepv\r\n"
    "whZC3EuWrSyyICNhLY21Ah7hbIEBPF3L3ZsOwC+UErL+dXWLdB56Jgy3gZaBeW7b\r\n"
    "vDrEnocJbqCm7IukhXHOBK8CgYEAwqdHB0hqyNSzIOGY7v9abzB6pUdA3BZiQvEs\r\n"
    "3LjHVd4HPJ2x0N8CgrBIWOE0q8+0hSMmeE96WW/7jD3fPWwCR5zlXknxBQsfv0gP\r\n"
    "3BC5PR0Qdypz+d+9zfMf625kyit4T/hzwhDveZUzHnk1Cf+IG7Q+TOEnLnWAWBED\r\n"
    "ISOWmrUCgYAFEmRxgwAc/u+D6t0syCwAYh6POtscq9Y0i9GyWk89NzgC4NdwwbBH\r\n"
    "4AgahOxIxXx2gxJnq3yfkJfIjwf0s2DyP0kY2y6Ua1OeomPeY9mrIS4tCuDQ6LrE\r\n"
    "TB6l9VGoxJL4fyHnZb8L5gGvnB1bbD8cL6YPaDiOhcRseC9vBiEuVg==\r\n"
    "-----END RSA PRIVATE KEY-----\r\n";

/* RSA-only ciphersuites, matching the WIZnet-PICO-C reference. These TLS 1.2
 * suites require CONFIG_MBEDTLS_KEY_EXCHANGE_RSA (enabled by default). */
static const int g_ciphersuites[] = {
    MBEDTLS_TLS_RSA_WITH_AES_256_CBC_SHA256,
    MBEDTLS_TLS_RSA_WITH_AES_128_GCM_SHA256,
    MBEDTLS_TLS_RSA_WITH_AES_128_CBC_SHA256,
    0
};

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
     * Poll until link is up before listening. */
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

/* RNG callback for mbedTLS, backed by the ESP hardware RNG. */
static int ssl_random_callback(void *p_rng, unsigned char *output, size_t output_len)
{
    (void)p_rng;
    esp_fill_random(output, output_len);
    return 0;
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

/* Reset the TLS session for the next client and re-bind the BIO. */
static void ssl_reset_session(uint8_t socket_fd)
{
    int retval = mbedtls_ssl_session_reset(&g_ssl);
    if (retval != 0) {
        printf(" SSL session reset failed %d\n", retval);
        return;
    }
    mbedtls_ssl_set_bio(&g_ssl, (void *)(uintptr_t)socket_fd, tcp_send, tcp_recv, recv_timeout);
}

/* Parse the server certificate/key and bind them to the config. */
static int ssl_load_credentials(void)
{
    int retval;

    if ((retval = mbedtls_x509_crt_parse(&g_srv_cert, g_srv_crt_pem, sizeof(g_srv_crt_pem))) < 0) {
        printf(" failed\n  ! mbedtls_x509_crt_parse returned -0x%x\n", (unsigned)-retval);
        return retval;
    }

    if ((retval = mbedtls_pk_parse_key(&g_srv_key, g_srv_key_pem, sizeof(g_srv_key_pem),
                                       NULL, 0, ssl_random_callback, NULL)) != 0) {
        printf(" failed\n  ! mbedtls_pk_parse_key returned -0x%x\n", (unsigned)-retval);
        return retval;
    }

    if ((retval = mbedtls_ssl_conf_own_cert(&g_conf, &g_srv_cert, &g_srv_key)) != 0) {
        printf(" failed\n  ! mbedtls_ssl_conf_own_cert returned %d\n", retval);
        return retval;
    }

    return 0;
}

static int wizchip_ssl_init(uint8_t socket_fd)
{
    int retval;

    mbedtls_ssl_init(&g_ssl);
    mbedtls_ssl_config_init(&g_conf);
    mbedtls_x509_crt_init(&g_srv_cert);
    mbedtls_pk_init(&g_srv_key);

    if ((retval = mbedtls_ssl_config_defaults(&g_conf,
                                              MBEDTLS_SSL_IS_SERVER,
                                              MBEDTLS_SSL_TRANSPORT_STREAM,
                                              MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
        printf(" failed\n  ! mbedtls_ssl_config_defaults returned %d\n", retval);
        return -1;
    }

    mbedtls_ssl_conf_authmode(&g_conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&g_conf, ssl_random_callback, NULL);
    mbedtls_ssl_conf_ciphersuites(&g_conf, g_ciphersuites);
    mbedtls_ssl_conf_read_timeout(&g_conf, SSL_RECV_TIMEOUT);

    if (ssl_load_credentials() != 0) {
        return -1;
    }

    if ((retval = mbedtls_ssl_setup(&g_ssl, &g_conf)) != 0) {
        printf(" failed\n  ! mbedtls_ssl_setup returned %d\n", retval);
        return -1;
    }

    /* ctx is the WIZnet socket number, recovered the same way in the BIO */
    mbedtls_ssl_set_bio(&g_ssl, (void *)(uintptr_t)socket_fd, tcp_send, tcp_recv, recv_timeout);

    return 0;
}

static int ssl_handshake_blocking(void)
{
    int retval;

    do {
        retval = mbedtls_ssl_handshake(&g_ssl);
    } while ((retval == MBEDTLS_ERR_SSL_WANT_READ) || (retval == MBEDTLS_ERR_SSL_WANT_WRITE));

    return retval;
}

static void ssl_server_task(void *arg)
{
    (void)arg;
    int32_t retval = 0;
    int handshake_done = 0;
    uint16_t len = 0;

    wizchip_port_initialize();

    if (wizchip_ssl_init(SOCKET_SSL) < 0) {
        printf(" SSL initialize failed\n");
        vTaskDelete(NULL);
        return;
    }

    if ((retval = socket(SOCKET_SSL, Sn_MR_TCP, PORT_SSL, SF_TCP_NODELAY)) != SOCKET_SSL) {
        printf(" Socket failed %d\n", (int)retval);
        vTaskDelete(NULL);
        return;
    }

    if ((retval = listen(SOCKET_SSL)) != SOCK_OK) {
        printf(" Listen failed %d\n", (int)retval);
        vTaskDelete(NULL);
        return;
    }

    printf(" SSL server listening on port %d\n", PORT_SSL);

    /* Server loop */
    while (1) {
        switch (getSn_SR(SOCKET_SSL)) {
        case SOCK_ESTABLISHED:
            if (getSn_IR(SOCKET_SSL) & Sn_IR_CON) {
                setSn_IR(SOCKET_SSL, Sn_IR_CON);
                ssl_reset_session(SOCKET_SSL);
                handshake_done = 0;
                printf(" TCP connection accepted\n");
            }

            if (!handshake_done) {
                retval = ssl_handshake_blocking();
                if (retval != 0) {
                    char err_buf[128];
                    mbedtls_strerror(retval, err_buf, sizeof(err_buf));
                    printf(" SSL handshake failed -0x%x (%s)\n", (unsigned)-retval, err_buf);

                    mbedtls_ssl_close_notify(&g_ssl);
                    disconnect(SOCKET_SSL);
                    close(SOCKET_SSL);
                    handshake_done = 0;
                    break;
                }

                handshake_done = 1;
                printf(" SSL handshake complete, ciphersuite %s\n", mbedtls_ssl_get_ciphersuite(&g_ssl));

                const char *msg = " W5x00 SSL server ready\r\n";
                mbedtls_ssl_write(&g_ssl, (const unsigned char *)msg, strlen(msg));
            }

            getsockopt(SOCKET_SSL, SO_RECVBUF, &len);
            if (len > 0) {
                if (len >= ETHERNET_BUF_MAX_SIZE) {
                    len = ETHERNET_BUF_MAX_SIZE - 1;
                }

                memset(g_ssl_buf, 0x00, ETHERNET_BUF_MAX_SIZE);
                retval = mbedtls_ssl_read(&g_ssl, g_ssl_buf, len);

                if (retval > 0) {
                    g_ssl_buf[retval] = 0x00;
                    printf(" Received: %s", (char *)g_ssl_buf);
                    mbedtls_ssl_write(&g_ssl, g_ssl_buf, retval);
                } else if ((retval == 0) ||
                           ((retval != MBEDTLS_ERR_SSL_WANT_READ) && (retval != MBEDTLS_ERR_SSL_WANT_WRITE))) {
                    printf(" SSL read error %d\n", (int)retval);

                    mbedtls_ssl_close_notify(&g_ssl);
                    disconnect(SOCKET_SSL);
                    close(SOCKET_SSL);
                    handshake_done = 0;
                }
            }
            break;

        case SOCK_CLOSE_WAIT:
            mbedtls_ssl_close_notify(&g_ssl);
            disconnect(SOCKET_SSL);
            close(SOCKET_SSL);
            handshake_done = 0;
            printf(" Connection closed, waiting for new client\n");
            break;

        case SOCK_CLOSED:
            handshake_done = 0;
            ssl_reset_session(SOCKET_SSL);
            if (socket(SOCKET_SSL, Sn_MR_TCP, PORT_SSL, SF_TCP_NODELAY) == SOCKET_SSL) {
                if (listen(SOCKET_SSL) == SOCK_OK) {
                    printf(" Re-listening on port %d\n", PORT_SSL);
                }
            }
            break;

        default:
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void app_main(void)
{
    xTaskCreate(ssl_server_task, "ssl_server_task", 16384, NULL, 5, NULL);
}
