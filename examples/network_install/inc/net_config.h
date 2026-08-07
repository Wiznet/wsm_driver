/*
 * SPDX-License-Identifier: CC0-1.0
 *
 * Network install example configuration.
 *
 * Follows esp_wiz_toe's config conventions:
 *   - SPI / pin wiring is NOT configured here. It comes from the component
 *     Kconfig (menuconfig -> Component config -> WIZnet TOE Component) and is
 *     applied by net_backend_toe.c via esp_wiz_toe_spi_config_t.
 *   - The network identity is expressed as wiz_NetInfo fields (byte arrays);
 *     main.c assembles a wiz_NetInfo from these macros and hands it to
 *     wiznet_net_init(), which applies it with wizchip_setnetinfo().
 *
 * There is no Wi-Fi section here, unlike the socket-based examples: this one
 * only reads the WIZnet chip's PHY registers, and Wi-Fi has no equivalent.
 */
#ifndef NET_CONFIG_H
#define NET_CONFIG_H

/* ---- static network identity (esp_wiz_toe style: wiz_NetInfo byte arrays) ---- */
#define NET_MAC_ADDR          {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}  /* WIZnet OUI */
#define NET_IP_ADDR           {192, 168, 11, 2}
#define NET_SUBNET_MASK       {255, 255, 255, 0}
#define NET_GATEWAY           {192, 168, 11, 1}
#define NET_DNS_ADDR          {8, 8, 8, 8}

/* ---- link check ----
 * Poll the PHY every LINK_CHECK_INTERVAL_MS and give up after
 * LINK_CHECK_MAX_RETRY consecutive "link off" reads, as in the original.
 */
#define LINK_CHECK_INTERVAL_MS  500
#define LINK_CHECK_MAX_RETRY    10

#endif /* NET_CONFIG_H */
