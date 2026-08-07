/*
 * SPDX-License-Identifier: CC0-1.0
 *
 * UDP echo engine. Same behaviour as the ioLibrary loopback_udps/loopback_udpc
 * this example was ported from, but written against the BSD socket API and
 * reached through a vtable, so the engine is backend-neutral (see udp_echo.h).
 */
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "udp_echo.h"
#include "net_config.h"     /* UDP_ECHO_BUF_SIZE */

static const char *TAG = "udp_echo";

/* Log the payload as text, like the ioLibrary client role does. Only enabled for
 * the client role so the server stays quiet under load. */
#ifdef CONFIG_EXAMPLE_UDP_CLIENT
#define UDP_ECHO_LOG_PAYLOAD 1
#else
#define UDP_ECHO_LOG_PAYLOAD 0
#endif

static void udp_echo_loop(const char *tag, const net_sock_ops_t *ops,
                          uint16_t bind_port, uint8_t *buf, int buf_size)
{
    int s = ops->socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0) {
        ESP_LOGE(TAG, "[%s] socket() failed: errno %d", tag, errno);
        return;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(bind_port),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (ops->bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "[%s] bind failed: errno %d", tag, errno);
        ops->close(s);
        return;
    }
    ESP_LOGI(TAG, "[%s] UDP echo on port %d", tag, bind_port);

    while (1) {
        struct sockaddr_in src;
        socklen_t sl = sizeof(src);
        int n = ops->recvfrom(s, buf, buf_size, 0, (struct sockaddr *)&src, &sl);
        if (n <= 0) {
            continue;
        }
#if UDP_ECHO_LOG_PAYLOAD
        ESP_LOGI(TAG, "[%s] %d bytes from %s:%d: %.*s", tag, n,
                 inet_ntoa(src.sin_addr), ntohs(src.sin_port), n, (const char *)buf);
#endif
        int off = 0;
        while (off < n) {                 /* echo back, handle partial sends */
            int w = ops->sendto(s, buf + off, n - off, 0, (struct sockaddr *)&src, sl);
            if (w < 0) {
                ESP_LOGE(TAG, "[%s] sendto failed: errno %d", tag, errno);
                break;
            }
            off += w;
        }
    }
}

/* --------------------------------------------------------------------------
 * Task launcher: same shape as examples/loopback's loopback_start().
 * ------------------------------------------------------------------------ */
typedef struct {
    const char           *name;
    const net_sock_ops_t *ops;
    uint16_t              bind_port;
    bool                (*is_up)(void);
} udp_echo_ctx_t;

static void udp_echo_task(void *arg)
{
    udp_echo_ctx_t *c = (udp_echo_ctx_t *)arg;

    uint8_t *buf = malloc(UDP_ECHO_BUF_SIZE);
    if (buf == NULL) {
        ESP_LOGE(TAG, "[%s] out of memory for %d-byte buffer", c->name, UDP_ECHO_BUF_SIZE);
        free(c);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "[%s] waiting for link...", c->name);
    while (!c->is_up()) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    udp_echo_loop(c->name, c->ops, c->bind_port, buf, UDP_ECHO_BUF_SIZE);

    free(buf);       /* udp_echo_loop only returns on a fatal setup error */
    free(c);
    vTaskDelete(NULL);
}

void udp_echo_start(const char *name, const net_sock_ops_t *ops,
                    uint16_t bind_port, bool (*is_up)(void))
{
    udp_echo_ctx_t *c = malloc(sizeof(*c));
    if (c == NULL) {
        ESP_LOGE(TAG, "[%s] out of memory", name);
        return;
    }
    c->name = name;
    c->ops = ops;
    c->bind_port = bind_port;
    c->is_up = is_up;

    if (xTaskCreate(udp_echo_task, name, 4096, c, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "[%s] xTaskCreate failed", name);
        free(c);
    }
}
