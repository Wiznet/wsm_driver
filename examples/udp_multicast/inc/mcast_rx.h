/*
 * SPDX-License-Identifier: CC0-1.0
 *
 * Backend-neutral UDP multicast receiver. The socket entry points are injected
 * via the component's net_sock_ops_t vtable, so the same logic drives either
 * stack:
 *   - Ethernet: net_eth_ops (plain lwip_*). With ESP_WIZ_TOE_SOCKET_WRAP=1 these
 *     are redirected to the WIZnet hardware sockets by -Wl,--wrap; with =0 they
 *     are the software LwIP over esp_eth. Correct either way, no #if needed.
 *   - Wi-Fi: net_wifi_ops, which bypasses that wrap to reach the real LwIP.
 *
 * Group membership is requested with the standard setsockopt(IP_ADD_MEMBERSHIP).
 * On the TOE that reaches the chip's hardware group filter (the component turns
 * it into a socket reopen with Sn_MR_MULTI); on Wi-Fi it is plain LwIP IGMP.
 * The engine does not know or care which.
 */
#ifndef MCAST_RX_H
#define MCAST_RX_H

#include <stdbool.h>
#include <stdint.h>

#include "net_sock_ops.h"   /* net_sock_ops_t, net_eth_ops, net_wifi_ops */

/*
 * Spawn a FreeRTOS task that waits until is_up() reports the interface ready,
 * then joins `group` on `port` and logs every datagram it receives.
 *   name   - short label; also the task name and log prefix (e.g. "eth")
 *   ops    - socket vtable for this interface
 *   group  - IPv4 multicast group, dotted quad (e.g. "224.0.0.5")
 *   port   - port to bind, which is also the group port
 *   is_up  - predicate the task polls for link readiness
 */
void mcast_rx_start(const char *name, const net_sock_ops_t *ops,
                    const char *group, uint16_t port, bool (*is_up)(void));

#endif /* MCAST_RX_H */
