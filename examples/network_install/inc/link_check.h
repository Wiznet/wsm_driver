/*
 * SPDX-License-Identifier: CC0-1.0
 *
 * PHY link / cable sanity check for first bring-up.
 *
 * NOTE: unlike the socket-based examples in this repo, this one takes no socket
 * vtable and has no Wi-Fi counterpart. It reads the WIZnet chip's PHY registers
 * directly (wizphy_getphylink / wizphy_getphyconf), which is below the socket
 * layer the loopback pattern switches on, and Wi-Fi has no such registers. So
 * only the file layout and the config split follow the other examples; the
 * engine stays Ethernet-only by nature.
 */
#ifndef LINK_CHECK_H
#define LINK_CHECK_H

#include <stdbool.h>

#include "wizchip_conf.h"   /* wiz_NetInfo */

/*
 * Spawn a task that waits until is_up() reports the chip initialized, then
 * polls the PHY until the link comes up (or LINK_CHECK_MAX_RETRY expires) and
 * prints the negotiated speed / duplex plus the IP to ping.
 *
 *   name      - short label; also the task name and log tag (e.g. "eth")
 *   net_info  - the identity applied to the chip, used to print the ping target
 *   is_up     - predicate the task polls for chip-init readiness
 */
void link_check_start(const char *name, const wiz_NetInfo *net_info,
                      bool (*is_up)(void));

#endif /* LINK_CHECK_H */
