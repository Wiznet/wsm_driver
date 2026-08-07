/*
 * SPDX-License-Identifier: CC0-1.0
 *
 * Transport seam for the TFTP client.
 *
 * The ioLibrary TFTP implementation this example carries reaches the network
 * through exactly four wrappers -- open, send, receive, close -- and its
 * protocol logic (RRQ / DATA / ACK, block numbers, retransmission) never touches
 * a socket. Pulling those four out here is what lets the same protocol code run
 * on BSD sockets, and on either interface, without being rewritten.
 *
 * Keeping the seam in its own header also keeps lwIP out of tftp_core.c. That
 * matters: ioLibrary's netutil.h declares inet_addr, inet_ntoa, htons, htonl,
 * ntohs and ntohl, all of which lwip/sockets.h also provides, with different
 * return types. Only tftp_transport.c includes lwIP, so the two never meet.
 *
 * Addresses are plain host-order uint32_t and uint16_t, as the protocol code
 * already uses them -- no sockaddr, no byte-order surprises across the seam.
 */
#ifndef TFTP_TRANSPORT_H
#define TFTP_TRANSPORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bind the transport to one interface. Call once before tftpc_init().
 *
 * Takes const void* rather than const net_sock_ops_t*: naming that type would
 * mean including net_sock_ops.h, which includes lwip/sockets.h, and this header
 * is included by tftp_core.c precisely to keep lwIP out of it. Only
 * tftp_transport.c knows the real type. */
void tftp_transport_bind(const void *ops);

/* Open a UDP socket on `local_port`. Returns a descriptor, or -1.
 * Receives must not block the protocol loop, so the socket is given a short
 * receive timeout rather than being left blocking. */
int  tftp_transport_open(uint16_t local_port);

/* Send `len` bytes to ip:port (both host order). Returns bytes sent, or -1. */
int  tftp_transport_send(int fd, const uint8_t *buf, uint32_t len,
                         uint32_t ip, uint16_t port);

/* Receive up to `len` bytes, reporting the sender in *ip / *port (host order).
 * Returns bytes received, or -1 when nothing arrived before the timeout. */
int  tftp_transport_recv(int fd, uint8_t *buf, uint32_t len,
                         uint32_t *ip, uint16_t *port);

void tftp_transport_close(int fd);

#ifdef __cplusplus
}
#endif

#endif /* TFTP_TRANSPORT_H */
