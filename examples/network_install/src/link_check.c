/*
 * SPDX-License-Identifier: CC0-1.0
 *
 * PHY link / cable sanity check. Same logic and same messages as the original
 * WIZnet-PICO-C example; only the SPI/chip bring-up moved out (it now comes
 * from wiznet_net_init(), see main.c) and the loop lives in its own file.
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "wizchip_conf.h"

#include "link_check.h"
#include "net_config.h"     /* LINK_CHECK_INTERVAL_MS, LINK_CHECK_MAX_RETRY */

static const char *TAG = "link_check";

typedef struct {
    const char   *name;
    wiz_NetInfo   net_info;      /* copied: the caller's may be const/static */
    bool        (*is_up)(void);
} link_ctx_t;

static void link_check_task(void *arg)
{
    link_ctx_t *c = (link_ctx_t *)arg;
    uint8_t link_status;
    uint16_t count = 0;

    ESP_LOGI(TAG, "[%s] waiting for chip init...", c->name);
    while (!c->is_up()) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    do {
        link_status = wizphy_getphylink();
        ESP_LOGI(TAG, "[%s] PHY link: %u", c->name, (unsigned)link_status);

        if (link_status == PHY_LINK_OFF) {
            count++;
            if (count > LINK_CHECK_MAX_RETRY) {
                ESP_LOGE(TAG, "[%s] Link failed of Internal PHY.", c->name);
                break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(LINK_CHECK_INTERVAL_MS));
    } while (link_status == PHY_LINK_OFF);

    if (link_status == PHY_LINK_ON) {
        wiz_PhyConf phyconf;
        wizphy_getphyconf(&phyconf);

        ESP_LOGI(TAG, "[%s] Link OK of Internal PHY.", c->name);
        /* Kept exactly as the original WIZnet-PICO-C example prints it, including
         * the inverted-looking ternary (PHY_SPEED_10 -> "100"). Do not "fix" this
         * without checking the original first -- see README. */
        ESP_LOGI(TAG, "[%s] the %d Mbtis speed of Internal PHY.", c->name,
                 phyconf.speed == PHY_SPEED_10 ? 100 : 10);
        ESP_LOGI(TAG, "[%s] The %s Duplex Mode of the Internal PHY.", c->name,
                 phyconf.duplex == PHY_DUPLEX_FULL ? "Full-Duplex" : "Half-Duplex");
        ESP_LOGI(TAG, "[%s] Try ping the ip:%d.%d.%d.%d.", c->name,
                 c->net_info.ip[0], c->net_info.ip[1],
                 c->net_info.ip[2], c->net_info.ip[3]);
    } else {
        ESP_LOGE(TAG, "[%s] Please check whether the network cable is loose or disconnected.",
                 c->name);
    }

    free(c);
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void link_check_start(const char *name, const wiz_NetInfo *net_info,
                      bool (*is_up)(void))
{
    link_ctx_t *c = malloc(sizeof(*c));
    if (c == NULL) {
        ESP_LOGE(TAG, "[%s] out of memory", name);
        return;
    }
    c->name     = name;
    c->net_info = *net_info;
    c->is_up    = is_up;

    if (xTaskCreate(link_check_task, name, 4096, c, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "[%s] xTaskCreate failed", name);
        free(c);
    }
}
