/*
 * SPDX-License-Identifier: CC0-1.0
 *
 * esp_netif helpers for the two things the socket-based responder in netbios.c
 * cannot do through a socket, shared by every interface that runs on the
 * software stack:
 *   - Wi-Fi, always                      (netif key "WIFI_STA_DEF")
 *   - Ethernet under the ETH backend     (netif key "ETH_DEF"), where the chip
 *     is an esp_eth MACRAW MAC and LwIP owns TCP/IP
 * The helpers take the netif key so one copy serves both; netif_netbios.c and
 * eth_netbios.c wrap them into the context-free netbios_ops_t vtables.
 *
 * Under the TOE backend the Ethernet side does NOT come here — its address
 * lives in the chip's registers, so it uses wizchip_getnetinfo() instead
 * (eth_netbios.c).
 */
#ifndef NETIF_NETBIOS_H
#define NETIF_NETBIOS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* esp_netif if_keys of the two default interfaces (see esp_netif_defaults.h). */
#define NETIF_KEY_WIFI  "WIFI_STA_DEF"
#define NETIF_KEY_ETH   "ETH_DEF"

/* netbios_ops_t::get_ip for an esp_netif interface: the address currently
 * installed on it. False while it has none (0.0.0.0), so the responder stays
 * quiet rather than advertising a bogus address. */
bool netif_netbios_get_ip(const char *ifkey, uint8_t ip[4]);

/* netbios_ops_t::get_ifname for an esp_netif interface: the LwIP netif name
 * ("st1", "en1", ...) the socket is pinned to with SO_BINDTODEVICE. Empty
 * string if the interface does not exist. */
void netif_netbios_get_ifname(const char *ifkey, char *ifname, size_t ifname_len);

#endif /* NETIF_NETBIOS_H */
