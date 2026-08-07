/*
 * SPDX-License-Identifier: CC0-1.0
 *
 * PPPoE session bring-up.
 *
 * NOTE: unlike the socket-based examples in this repo, this one takes no socket
 * vtable and has no Wi-Fi counterpart. The vendored PPPoE.c drives the WIZnet
 * chip's PPPoE registers directly, which sits below the BSD socket layer the
 * loopback pattern switches on, and Wi-Fi has no such registers. So only the
 * file layout and the config split follow the other examples; the engine stays
 * Ethernet-only, and W5500-only, by nature.
 */
#ifndef PPPOE_SESSION_H
#define PPPOE_SESSION_H

#include <stdbool.h>

/*
 * Spawn a task that waits until is_up() reports the chip initialized, runs the
 * PPPoE negotiation, and prints the assigned address plus the resulting network
 * configuration read back from the chip.
 *
 *   name  - short label; also the task name and log tag (e.g. "eth")
 *   is_up - predicate the task polls for chip-init readiness
 */
void pppoe_session_start(const char *name, bool (*is_up)(void));

#endif /* PPPOE_SESSION_H */
