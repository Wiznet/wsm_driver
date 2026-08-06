/*
 * SPDX-License-Identifier: CC0-1.0
 *
 * UDP multicast receive engine. Same behaviour as the ioLibrary multicast_recv()
 * this example was ported from, but written against the BSD socket API and
 * reached through a vtable, so the engine is backend-neutral (see mcast_rx.h).
 *
 * The port from the ioLibrary version is mostly a simplification: the Sn_SR
 * state machine (SOCK_UDP / SOCK_CLOSED, with the app re-poking Sn_DIPR,
 * Sn_DPORT and re-opening the socket itself) collapses into bind() plus one
 * setsockopt(). The register work still happens -- it just moved into the
 * component, where the whole project shares one copy of it.
 */
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "mcast_rx.h"
#include "net_config.h"     /* MCAST_BUF_SIZE */

static const char *TAG = "mcast_rx";

static void mcast_rx_loop(const char *tag, const net_sock_ops_t *ops,
                          mcast_join_fn join, const char *group, uint16_t port,
                          uint8_t *buf, int buf_size)
{
    int s = ops->socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0) {
        ESP_LOGE(TAG, "[%s] socket() failed: errno %d", tag, errno);
        return;
    }

    int opt = 1;
    ops->setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (ops->bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "[%s] bind failed: errno %d", tag, errno);
        ops->close(s);
        return;
    }

    /* Joining after bind() is the usual BSD order. How it is carried out is the
     * one thing that differs between the two backends, so it arrives as a
     * function pointer -- see mcast_join.h. */
    if (join(ops, s, group, port) < 0) {
        ESP_LOGE(TAG, "[%s] joining %s failed", tag, group);
        ops->close(s);
        return;
    }
    ESP_LOGI(TAG, "[%s] listening to %s:%d", tag, group, port);

    while (1) {
        struct sockaddr_in src;
        socklen_t sl = sizeof(src);
        int n = ops->recvfrom(s, buf, buf_size - 1, 0, (struct sockaddr *)&src, &sl);
        if (n <= 0) {
            continue;
        }
        buf[n] = '\0';
        ESP_LOGI(TAG, "[%s] %d bytes from %s: %s", tag, n,
                 inet_ntoa(src.sin_addr), (const char *)buf);
    }
}

/* --------------------------------------------------------------------------
 * Task launcher: same shape as examples/loopback's loopback_start().
 * ------------------------------------------------------------------------ */
typedef struct {
    const char           *name;
    const net_sock_ops_t *ops;
    mcast_join_fn         join;
    const char           *group;
    uint16_t              port;
    bool                (*is_up)(void);
} mcast_rx_ctx_t;

static void mcast_rx_task(void *arg)
{
    mcast_rx_ctx_t *c = (mcast_rx_ctx_t *)arg;

    uint8_t *buf = malloc(MCAST_BUF_SIZE);
    if (buf == NULL) {
        ESP_LOGE(TAG, "[%s] out of memory for %d-byte buffer", c->name, MCAST_BUF_SIZE);
        free(c);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "[%s] waiting for link...", c->name);
    while (!c->is_up()) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    mcast_rx_loop(c->name, c->ops, c->join, c->group, c->port, buf, MCAST_BUF_SIZE);

    free(buf);       /* mcast_rx_loop only returns on a fatal setup error */
    free(c);
    vTaskDelete(NULL);
}

void mcast_rx_start(const char *name, const net_sock_ops_t *ops,
                    mcast_join_fn join, const char *group, uint16_t port,
                    bool (*is_up)(void))
{
    mcast_rx_ctx_t *c = malloc(sizeof(*c));
    if (c == NULL) {
        ESP_LOGE(TAG, "[%s] out of memory", name);
        return;
    }
    c->name = name;
    c->ops = ops;
    c->join = join;
    c->group = group;
    c->port = port;
    c->is_up = is_up;

    if (xTaskCreate(mcast_rx_task, name, 4096, c, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "[%s] xTaskCreate failed", name);
        free(c);
    }
}
