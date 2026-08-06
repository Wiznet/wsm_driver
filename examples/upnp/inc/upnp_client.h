/*
 * SPDX-License-Identifier: CC0-1.0
 *
 * Backend-neutral driver for one UPnP session.
 *
 * Takes a socket vtable, so the same code runs on the WIZnet hardware sockets
 * or on the software LwIP stack behind the Wi-Fi netif.
 *
 * This replaces the original's hyperterminal.c, a serial menu that waited on
 * keystrokes to pick each action. A menu cannot be exercised without someone
 * at the keyboard, and it was the only reason the example pulled in a console
 * input layer; the sequence it drove is fixed here instead and configured from
 * net_config.h.
 */
#ifndef UPNP_CLIENT_H
#define UPNP_CLIENT_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Start the session on its own task.
 *
 * name    label used in the log ("eth", "wifi")
 * ops     socket vtable: &net_eth_ops or &net_wifi_ops
 * is_up   returns true once that interface can carry traffic
 * local_ip  returns this interface's address as text; advertised to the IGD in
 *           the eventing callback header. Called only after is_up(), because on
 *           Wi-Fi the address is a DHCP lease that does not exist before then.
 *           A function rather than a string for the same reason.
 */
void upnp_client_start(const char *name, const void *ops,
                       bool (*is_up)(void), const char *(*local_ip)(void));

#endif /* UPNP_CLIENT_H */
