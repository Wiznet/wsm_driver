/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

/*
 * UDP multicast receiver on the WIZnet TOE (W5500 / W6300), ported from
 * WIZnet-PICO-C examples/udp_multicast/udp_multicast_receiver.
 *
 * app_main only orchestrates: bring the interfaces up, start a receiver on
 * each, and return. The receive logic lives in the backend-neutral engine
 * mcast_rx.c, which takes a socket vtable, so both interfaces run the very same
 * code at the same level:
 *   - Ethernet (WIZnet chip) on MCAST_GROUP_PORT      (vtable: net_eth_ops)
 *   - Wi-Fi STA              on WIFI_MCAST_GROUP_PORT (vtable: net_wifi_ops)
 *
 * Group membership goes through the standard setsockopt(IP_ADD_MEMBERSHIP). On
 * the Ethernet side the component turns that into the chip's hardware group
 * filter (Sn_DHAR / Sn_DIPR / Sn_DPORT plus a socket reopen with Sn_MR_MULTI);
 * on Wi-Fi it is plain LwIP IGMP. Neither the engine nor this file has an #if.
 *
 * Wi-Fi is optional: leave WIFI_SSID empty in net_config.h to run Ethernet-only.
 *
 * Works with W5500 or W6300 — select the chip in menuconfig:
 *   Component config -> WIZnet TOE Component -> WIZnet chip
 */
#include "sdkconfig.h"
#include "wizchip_conf.h"       /* wiz_NetInfo, NETINFO_STATIC */

#include "net_backend.h"
#include "wifi_backend.h"
#include "net_sock_ops.h"
#include "net_config.h"
#include "mcast_rx.h"

/* Network identity — esp_wiz_toe style (wiz_NetInfo). Applied to the WIZnet
 * chip's hardware TCP/IP stack by wiznet_net_init() -> wizchip_setnetinfo(). */
static const wiz_NetInfo g_net_info = {
    .mac = NET_MAC_ADDR,
    .ip  = NET_IP_ADDR,
    .sn  = NET_SUBNET_MASK,
    .gw  = NET_GATEWAY,
    .dns = NET_DNS_ADDR,
#if _WIZCHIP_ > W5500
    .ipmode = NETINFO_STATIC_ALL,
#endif
    .dhcp = NETINFO_STATIC,
};

/* An empty SSID means "no AP configured" — run Ethernet-only rather than
 * spinning on a connect that can never succeed. A plain runtime test rather than
 * an #if: the preprocessor cannot inspect a string literal, and the compiler
 * folds this away anyway. */
#define WIFI_CONFIGURED  (WIFI_SSID[0] != '\0')

void app_main(void)
{
    /* Ethernet (WIZnet chip) first: it initializes esp_netif + the default event
     * loop that Wi-Fi then reuses, and applies g_net_info to the chip. */
    wiznet_net_init(&g_net_info);
    if (WIFI_CONFIGURED) {
        wifi_net_init(WIFI_SSID, WIFI_PASS);
    }

    /* Start both receivers as sibling tasks; each waits for its own link.
     * Same call shape — only the label, vtable, port and readiness predicate
     * differ. */
    mcast_rx_start("eth", &net_eth_ops, MCAST_GROUP_IP, MCAST_GROUP_PORT,
                   wiznet_net_is_up);
    if (WIFI_CONFIGURED) {
        mcast_rx_start("wifi", &net_wifi_ops, MCAST_GROUP_IP, WIFI_MCAST_GROUP_PORT,
                       wifi_net_is_up);
    }
}
