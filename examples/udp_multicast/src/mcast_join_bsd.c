/*
 * SPDX-License-Identifier: CC0-1.0
 *
 * Multicast join for the software LwIP stack (see mcast_join.h).
 *
 * This is the ordinary way to do it: bind first, then ask to join. LwIP sends
 * the IGMP membership report and filters in software.
 *
 * One of two files in this example that include lwIP; the other is the engine.
 * mcast_join_toe.c must not, and does not.
 */
#include <errno.h>

#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"

#include "net_sock_ops.h"
#include "mcast_join.h"

static const char *TAG = "mcast_join";

int mcast_join_bsd(const void *ops, int fd, const char *group, uint16_t port)
{
    const net_sock_ops_t *o = (const net_sock_ops_t *)ops;

    /* struct ip_mreq carries no port: in BSD the group port is whatever the
     * socket is bound to, which is what the caller already did. */
    (void)port;

    struct ip_mreq mreq = {
        .imr_multiaddr.s_addr = inet_addr(group),
        .imr_interface.s_addr = htonl(INADDR_ANY),
    };
    if (o->setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                      &mreq, sizeof(mreq)) < 0) {
        ESP_LOGE(TAG, "IP_ADD_MEMBERSHIP for %s failed: errno %d", group, errno);
        return -1;
    }
    return 0;
}

/* Reported from here because this is the file that can see lwIP -- see the
 * declaration in mcast_join.h. */
int mcast_lwip_socket_offset(void)
{
    return LWIP_SOCKET_OFFSET;
}
