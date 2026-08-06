/* Minimal Pico SDK compatibility shim so the vendored WIZnet-PICO-C example
 * sources build unmodified on ESP-IDF. */
#pragma once

#include <stdint.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

#ifndef PICO_ERROR_TIMEOUT
#define PICO_ERROR_TIMEOUT (-1)
#endif

#define sleep_ms(ms) vTaskDelay(pdMS_TO_TICKS(ms))

static inline int getchar_timeout_us(uint64_t timeout_us)
{
    uint64_t start = esp_timer_get_time();
    for (;;) {
        int c = getchar();
        if (c != EOF) {
            return c;
        }
        if ((uint64_t)(esp_timer_get_time() - start) >= timeout_us) {
            return PICO_ERROR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
