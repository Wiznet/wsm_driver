/*
 * SPDX-License-Identifier: CC0-1.0
 *
 * esp_netif helpers for the two things the socket-based DHCP client in
 * dhcp_dns.c cannot do through a socket, shared by every interface that runs on
 * the software stack:
 *   - Wi-Fi, always                      (netif key "WIFI_STA_DEF")
 *   - Ethernet under the ETH backend     (netif key "ETH_DEF"), where the chip
 *     is an esp_eth MACRAW MAC and LwIP owns TCP/IP
 * The helpers take the netif key so one copy serves both; netif_dhcp_dns.c and
 * eth_dhcp_dns.c wrap them into the context-free dhcp_dns_ops_t vtables.
 *
 * Under the TOE backend the Ethernet side does NOT come here — its identity
 * lives in the chip's registers, so it uses wizchip_get/setnetinfo instead
 * (eth_dhcp_dns.c).
 */
#ifndef NETIF_DHCP_DNS_H
#define NETIF_DHCP_DNS_H

#include <stddef.h>

#include "dhcp_dns.h"

/* esp_netif if_keys of the two default interfaces (see esp_netif_defaults.h). */
#define NETIF_KEY_WIFI  "WIFI_STA_DEF"
#define NETIF_KEY_ETH   "ETH_DEF"

/*
 * dhcp_dns_ops_t::prepare for an esp_netif interface:
 *   - stops the stack's own DHCP client, so nothing else holds UDP port 68 and
 *     nothing else races us to install an address;
 *   - reports the interface MAC (the chaddr the client advertises);
 *   - reports the LwIP netif name ("st1", "en1", ...) so the client can pin its
 *     socket to this interface with SO_BINDTODEVICE. Without that a broadcast
 *     would leave through netif_default, which may be the other interface.
 */
void netif_dhcp_prepare(const char *ifkey, uint8_t mac[6], char *ifname, size_t ifname_len);

/* dhcp_dns_ops_t::apply_lease for an esp_netif interface: install ip/sn/gw and
 * the DNS server the lease carried. */
void netif_apply_lease(const char *ifkey, const dhcp_dns_netinfo_t *info);

#endif /* NETIF_DHCP_DNS_H */
