/*
 * SPDX-License-Identifier: CC0-1.0
 *
 * BSD implementation of the TFTP transport seam (see tftp_transport.h).
 *
 * This is the only file in the example that includes lwIP, which is what keeps
 * ioLibrary's netutil declarations and lwIP's from ever being seen together.
 *
 * Sockets are reached through the component's net_sock_ops_t vtable, so the
 * TFTP client runs on the WIZnet hardware sockets or on the software LwIP the
 * Wi-Fi netif is attached to, chosen by which vtable main.c hands over.
 */
#include <errno.h>
#include <string.h>

#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"

#include "net_sock_ops.h"
#include "tftp_transport.h"

static const char *TAG = "tftp_tx";

/* The protocol loop polls, so a receive must return promptly when idle.
 * Long enough to catch a reply in flight, short enough to keep tftpc_run()
 * responsive to its own retransmission timer. */
#define TFTP_RECV_TIMEOUT_MS  200

static const net_sock_ops_t *s_ops;

void tftp_transport_bind(const void *ops)
{
    s_ops = (const net_sock_ops_t *)ops;
}

int tftp_transport_open(uint16_t local_port)
{
    if (s_ops == NULL) {
        ESP_LOGE(TAG, "transport used before bind");
        return -1;
    }

    int fd = s_ops->socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) {
        ESP_LOGE(TAG, "socket() failed: errno %d", errno);
        return -1;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(local_port),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (s_ops->bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "bind(%u) failed: errno %d", local_port, errno);
        s_ops->close(fd);
        return -1;
    }

    struct timeval tv = {
        .tv_sec  = TFTP_RECV_TIMEOUT_MS / 1000,
        .tv_usec = (TFTP_RECV_TIMEOUT_MS % 1000) * 1000,
    };
    s_ops->setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    return fd;
}

int tftp_transport_send(int fd, const uint8_t *buf, uint32_t len,
                        uint32_t ip, uint16_t port)
{
    struct sockaddr_in dst = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = htonl(ip),
    };

    int n = s_ops->sendto(fd, buf, len, 0, (struct sockaddr *)&dst, sizeof(dst));
    if (n < 0) {
        /* Expected once per destination: the chip resolves the peer's MAC with
         * ARP inside sendto(), and the very first packet to a new address can
         * time out before the reply lands. The protocol's retransmission timer
         * covers it, so this is logged at debug level rather than as a warning. */
        ESP_LOGD(TAG, "sendto failed (errno %d) -- retransmission will retry", errno);
        return -1;
    }
    return n;
}

int tftp_transport_recv(int fd, uint8_t *buf, uint32_t len,
                        uint32_t *ip, uint16_t *port)
{
    struct sockaddr_in src;
    socklen_t sl = sizeof(src);

    int n = s_ops->recvfrom(fd, buf, len, 0, (struct sockaddr *)&src, &sl);
    if (n <= 0) {
        return -1;              /* timeout, or nothing to read yet */
    }

    /* The protocol code works in host order on both sides of this seam. */
    if (ip != NULL) {
        *ip = ntohl(src.sin_addr.s_addr);
    }
    if (port != NULL) {
        *port = ntohs(src.sin_port);
    }
    return n;
}

void tftp_transport_close(int fd)
{
    if (fd >= 0) {
        s_ops->close(fd);
    }
}
