/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * WIZnet TOE backend bring-up (see src/net_backend_toe.c).
 *
 * Config conventions follow esp_wiz_toe:
 *   - pins / SPI -> esp_wiz_toe_spi_config_t built from Kconfig (CONFIG_ESP_WIZ_TOE_*)
 *   - network    -> wiz_NetInfo + wizchip_setnetinfo() (ioLibrary standard)
 */
#ifndef NET_BACKEND_H
#define NET_BACKEND_H

#include <stdbool.h>

#include "wizchip_conf.h"   /* wiz_NetInfo */

#ifdef __cplusplus
extern "C" {
#endif

/* Bring up lwIP (shadow esp_netif holding the IPv4 identity) + the WIZnet chip
 * over SPI (pins/SPI from Kconfig), then apply the network identity to the
 * chip's hardware TCP/IP stack via wizchip_setnetinfo(net_info).
 * Blocks only for chip init, not for link. */
void wiznet_net_init(const wiz_NetInfo *net_info);

/* True once bring-up has completed. */
bool wiznet_net_is_up(void);

#ifdef __cplusplus
}
#endif

#endif /* NET_BACKEND_H */
