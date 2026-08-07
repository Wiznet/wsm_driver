#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    spi_host_device_t host_id;
    int clock_hz;
    gpio_num_t pin_miso;   /**< W6300 QSPI: IO1 */
    gpio_num_t pin_mosi;   /**< W6300 QSPI: IO0 */
    gpio_num_t pin_sclk;
    gpio_num_t pin_cs;
    gpio_num_t pin_rst;
    gpio_num_t pin_int;
    gpio_num_t pin_io2;    /**< W6300 quad mode only; ignored otherwise */
    gpio_num_t pin_io3;    /**< W6300 quad mode only; ignored otherwise */
    uint32_t lock_timeout_ms;
} esp_wiz_toe_spi_config_t;

esp_err_t esp_wiz_toe_spi_init(const esp_wiz_toe_spi_config_t *cfg);
esp_err_t esp_wiz_toe_spi_deinit(void);
esp_err_t esp_wiz_toe_spi_register_iolib_callbacks(void);
esp_err_t esp_wiz_toe_spi_reset(void);
esp_err_t esp_wiz_toe_spi_wizchip_check(void);
esp_err_t esp_wiz_toe_spi_link_is_up(bool *is_up);

#ifdef __cplusplus
}
#endif
