/*
 * SPDX-License-Identifier: CC0-1.0
 *
 * WIZnet TOE (W5500 / W6300) network install check.
 *
 * Checks the PHY link/cable status, prints the negotiated speed/duplex of the
 * chip's internal PHY and the IP address to ping -- a first-bring-up cable and
 * network sanity check.
 *
 * Ethernet only, and deliberately so: this example reads the chip's PHY
 * registers directly, which is below the BSD socket layer the other examples
 * switch between Ethernet and Wi-Fi. Wi-Fi has no equivalent registers, so
 * there is no second interface to start here. Only the file layout and the
 * config split follow the loopback-style examples.
 *
 * Config conventions follow esp_wiz_toe:
 *   - SPI / pins  -> component Kconfig, applied by net_backend_toe.c.
 *   - network id  -> the wiz_NetInfo below (byte arrays from net_config.h),
 *                    applied by wiznet_net_init() -> wizchip_setnetinfo().
 */

#include <stdio.h>

#include "sdkconfig.h"
#include "wizchip_conf.h"       /* wiz_NetInfo, NETINFO_STATIC */

#include "net_backend.h"
#include "net_config.h"
#include "link_check.h"

/* Network identity - esp_wiz_toe style (wiz_NetInfo). Applied to the WIZnet
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

void app_main(void)
{
    wiznet_net_init(&g_net_info);
    link_check_start("eth", &g_net_info, wiznet_net_is_up);
}
