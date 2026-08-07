/*
 * SPDX-License-Identifier: CC0-1.0
 *
 * TFTP read client. Drives the protocol implementation in tftp_core.c, supplies
 * its storage hook, and runs its retransmission tick.
 *
 * The ioLibrary original expects the application to provide both of those. The
 * ESP example this replaces provided the tick but not the hook, and left
 * F_STORAGE undefined, so every block was acknowledged and discarded -- a
 * transfer reported success without the file ever being looked at. Both are
 * wired up here.
 */
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lwip/inet.h"

#include "tftp_client.h"
#include "tftp_core.h"
#include "tftp_transport.h"
#include "net_config.h"

static const char *TAG = "tftp";

/* tftp_core.c holds its state in globals, so one transfer at a time. */
static uint8_t  g_socket_buf[MAX_MTU_SIZE];
static uint32_t g_bytes_received;
static uint16_t g_blocks_received;

/* Storage hook called by tftp_core.c for every DATA block (F_STORAGE).
 * A real application would write to flash or a filesystem here; the example
 * accounts for what arrived and shows the beginning of the file, which is
 * enough to prove the payload actually came through. */
void save_data(uint8_t *data, uint32_t data_len, uint16_t block_number)
{
    g_bytes_received += data_len;
    g_blocks_received = block_number;

    if (block_number == 1) {
        char preview[TFTP_PREVIEW_BYTES + 1];
        uint32_t n = (data_len < TFTP_PREVIEW_BYTES) ? data_len : TFTP_PREVIEW_BYTES;
        memcpy(preview, data, n);
        preview[n] = '\0';
        for (uint32_t i = 0; i < n; i++) {          /* keep the log one line */
            if (preview[i] == '\r' || preview[i] == '\n') {
                preview[i] = ' ';
            }
        }
        ESP_LOGI(TAG, "first block: \"%s\"%s", preview,
                 (data_len > TFTP_PREVIEW_BYTES) ? " ..." : "");
    }
}

/* 1 s tick for the protocol's retransmission timer. tftp_core.c counts these,
 * so without the tick a lost packet would never be retried. */
static void tftp_tick(void *arg)
{
    (void)arg;
    tftpc_timeout_handler();
}

typedef struct {
    const char           *name;
    const net_sock_ops_t *ops;
    const char           *server_ip;
    const char           *filename;
    bool                (*is_up)(void);
} tftp_client_ctx_t;

static void tftp_client_task(void *arg)
{
    tftp_client_ctx_t *c = (tftp_client_ctx_t *)arg;
    esp_timer_handle_t tick = NULL;

    ESP_LOGI(TAG, "[%s] waiting for link...", c->name);
    while (!c->is_up()) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    /* Bind the protocol's network seam to this interface before anything opens
     * a socket, then start the retransmission tick. */
    tftp_transport_bind(c->ops);

    const esp_timer_create_args_t tick_args = {
        .callback = tftp_tick,
        .name = "tftp_1s",
    };
    if (esp_timer_create(&tick_args, &tick) != ESP_OK ||
        esp_timer_start_periodic(tick, 1000 * 1000) != ESP_OK) {
        ESP_LOGE(TAG, "[%s] could not start the retransmission tick", c->name);
        goto done;
    }

    g_bytes_received = 0;
    g_blocks_received = 0;
    tftpc_init(0, g_socket_buf);

    ESP_LOGI(TAG, "[%s] requesting \"%s\" from %s", c->name, c->filename, c->server_ip);
    tftpc_read_request(ntohl(inet_addr(c->server_ip)), (uint8_t *)c->filename);

    while (1) {
        int state = tftpc_run();

        if (state == TFTP_SUCCESS) {
            ESP_LOGI(TAG, "[%s] \"%s\" received: %u bytes in %u blocks",
                     c->name, c->filename,
                     (unsigned)g_bytes_received, (unsigned)g_blocks_received);
            break;
        }
        if (state == TFTP_FAIL) {
            ESP_LOGE(TAG, "[%s] transfer of \"%s\" failed", c->name, c->filename);
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    tftpc_exit();

done:
    if (tick != NULL) {
        esp_timer_stop(tick);
        esp_timer_delete(tick);
    }
    free(c);
    vTaskDelete(NULL);
}

void tftp_client_start(const char *name, const net_sock_ops_t *ops,
                       const char *server_ip, const char *filename,
                       bool (*is_up)(void))
{
    tftp_client_ctx_t *c = malloc(sizeof(*c));
    if (c == NULL) {
        ESP_LOGE(TAG, "[%s] out of memory", name);
        return;
    }
    c->name = name;
    c->ops = ops;
    c->server_ip = server_ip;
    c->filename = filename;
    c->is_up = is_up;

    /* tftp_core.c builds each outgoing packet in a MAX_MTU_SIZE (1514 byte)
     * buffer on the stack, so this task needs noticeably more room than the
     * other examples' 4 KB. */
    if (xTaskCreate(tftp_client_task, name, 8192, c, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "[%s] xTaskCreate failed", name);
        free(c);
    }
}
