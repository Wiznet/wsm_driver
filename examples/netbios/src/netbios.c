/*
 * SPDX-License-Identifier: CC0-1.0
 *
 * NetBIOS name-service responder engine. Same behaviour as the ioLibrary
 * do_netbios() this example was ported from, but written against the BSD socket
 * API and reached through a vtable, so the engine is backend-neutral (see
 * netbios.h).
 *
 * The port from the ioLibrary version is mostly a simplification: the getSn_SR()
 * state machine (SOCK_UDP / SOCK_CLOSED, with the app re-opening the hardware
 * socket itself) collapses into one bind() plus a recvfrom() loop, and the
 * hand-rolled htons/htonl/checksum helpers give way to lwIP's. The register work
 * still happens -- it just moved into the component, where the whole project
 * shares one copy of it.
 */
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>        /* strcasecmp: NetBIOS names are case-insensitive */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "netbios.h"
#include "net_config.h"     /* NETBIOS_PORT, NETBIOS_BUF_SIZE, ... */

static const char *TAG = "netbios";

/* ==========================================================================
 * NetBIOS name service wire format (RFC 1001 / 1002)
 * ========================================================================== */

/* A NetBIOS name is 16 bytes: 15 space-padded name characters plus a one-byte
 * service suffix. Level-1 encoding sends each byte as two characters, so the
 * label on the wire is 32 bytes long. */
#define NETBIOS_NAME_LEN        16
#define NETBIOS_ENCNAME_LEN     (NETBIOS_NAME_LEN * 2)

/* Header flags */
#define NETB_HFLAG_RESPONSE           0x8000U
#define NETB_HFLAG_OPCODE             0x7800U
#define NETB_HFLAG_OPCODE_NAME_QUERY  0x0000U
#define NETB_HFLAG_AUTHORATIVE        0x0400U
#define NETB_HFLAG_RECURS_DESIRED     0x0100U

/* Name flags (answer only) */
#define NETB_NFLAG_NODETYPE_BNODE     0x0000U

typedef struct __attribute__((packed)) {
    uint16_t trans_id;
    uint16_t flags;
    uint16_t questions;
    uint16_t answer_rrs;
    uint16_t authority_rrs;
    uint16_t additional_rrs;
} netbios_hdr_t;

/*
 * The question and the answer share a prefix: both start with the encoded name,
 * the record type and the class. The question stops there; the answer continues
 * with ttl/datalen/flags/addr. One struct describes both, and a question is read
 * through the first NETBIOS_QUESTION_LEN bytes of it.
 */
typedef struct __attribute__((packed)) {
    uint8_t  nametype;                          /* label length: 32 */
    uint8_t  encname[NETBIOS_ENCNAME_LEN + 1];  /* 32 characters + the root label */
    uint16_t type;
    uint16_t cls;
    uint32_t ttl;                               /* answer only */
    uint16_t datalen;                           /* answer only */
    uint16_t flags;                             /* answer only */
    uint8_t  addr[4];                           /* answer only */
} netbios_name_hdr_t;

#define NETBIOS_QUESTION_LEN  offsetof(netbios_name_hdr_t, ttl)

typedef struct __attribute__((packed)) {
    netbios_hdr_t      hdr;
    netbios_name_hdr_t name;
} netbios_resp_t;

/* Everything one responder instance owns. One of these per interface, so the
 * Ethernet and Wi-Fi responders share no state at all. */
typedef struct {
    const char          *name;
    const netbios_ops_t *ops;
    const char          *nb_name;
    bool               (*is_up)(void);
    char                 ifname[8];   /* "" when the stack has no netif name */
} netbios_ctx_t;

/* ---- socket helpers (same shape as examples/dhcp_dns) -------------------- */

/* Pin the socket to one netif so a broadcast query arriving on the OTHER
 * interface is not answered here, and the reply leaves through this one. Best
 * effort: the TOE has no netif to name, and its --wrap accepts the option as a
 * no-op (the chip IS the interface). */
static void sock_bind_iface(const net_sock_ops_t *sk, int fd, const char *ifname)
{
    if (ifname == NULL || ifname[0] == '\0') {
        return;
    }
    struct ifreq ifr = {0};
    size_t n = strlen(ifname);
    if (n > sizeof(ifr.ifr_name) - 1) {
        n = sizeof(ifr.ifr_name) - 1;       /* ifr_name is IFNAMSIZ, "st1"-sized */
    }
    memcpy(ifr.ifr_name, ifname, n);
    sk->setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof(ifr));
}

static void sock_set_rcvtimeo(const net_sock_ops_t *sk, int fd, uint32_t ms)
{
    struct timeval tv = {
        .tv_sec  = (long)(ms / 1000),
        .tv_usec = (long)((ms % 1000) * 1000),
    };
    sk->setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

/* ---- protocol ------------------------------------------------------------ */

/*
 * Undo the level-1 encoding of RFC 1001 4.1: every name byte arrives as two
 * characters, 'A' + high nibble and 'A' + low nibble. Writes the 15-character
 * name with its trailing padding removed; the 16th byte is the service suffix
 * and is not part of the name. Returns false if the label is not valid level-1
 * encoding at all.
 */
static bool netbios_name_decode(const uint8_t *enc, char *out, size_t out_len)
{
    uint8_t raw[NETBIOS_NAME_LEN];

    for (int i = 0; i < NETBIOS_NAME_LEN; i++) {
        char hi = (char)enc[i * 2];
        char lo = (char)enc[i * 2 + 1];
        if (hi < 'A' || hi > 'P' || lo < 'A' || lo > 'P') {
            return false;
        }
        raw[i] = (uint8_t)(((hi - 'A') << 4) | (lo - 'A'));
    }

    size_t n = NETBIOS_NAME_LEN - 1;            /* drop the service suffix */
    while (n > 0 && raw[n - 1] == ' ') {
        n--;
    }
    if (n >= out_len) {
        return false;
    }
    memcpy(out, raw, n);
    out[n] = '\0';
    return true;
}

/*
 * Inspect one received datagram. If it is a name query for `c->nb_name`, fill
 * `resp` with the answer and return its length; return 0 when the datagram
 * needs no reply (not a query, someone else's name, or malformed).
 */
static size_t netbios_handle(netbios_ctx_t *c, const uint8_t *req, size_t req_len,
                             netbios_resp_t *resp)
{
    if (req_len < sizeof(netbios_hdr_t) + NETBIOS_QUESTION_LEN) {
        return 0;
    }

    const netbios_hdr_t *hdr = (const netbios_hdr_t *)(const void *)req;
    const netbios_name_hdr_t *q =
        (const netbios_name_hdr_t *)(const void *)(req + sizeof(netbios_hdr_t));

    uint16_t flags = ntohs(hdr->flags);
    if ((flags & NETB_HFLAG_RESPONSE) != 0 ||
        (flags & NETB_HFLAG_OPCODE) != NETB_HFLAG_OPCODE_NAME_QUERY ||
        ntohs(hdr->questions) != 1 ||
        q->nametype != NETBIOS_ENCNAME_LEN) {
        return 0;
    }

    char asked[NETBIOS_NAME_LEN];
    if (!netbios_name_decode(q->encname, asked, sizeof(asked))) {
        return 0;
    }
    ESP_LOGI(TAG, "[%s] name query for \"%s\" (we are \"%s\")",
             c->name, asked, c->nb_name);
    if (strcasecmp(asked, c->nb_name) != 0) {
        return 0;                               /* not us */
    }

    uint8_t ip[4];
    if (!c->ops->get_ip(ip)) {
        ESP_LOGW(TAG, "[%s] query for \"%s\" but the interface has no address yet",
                 c->name, asked);
        return 0;
    }

    memset(resp, 0, sizeof(*resp));
    /* trans_id is echoed verbatim — it is already in network order. */
    resp->hdr.trans_id   = hdr->trans_id;
    resp->hdr.flags      = htons(NETB_HFLAG_RESPONSE | NETB_HFLAG_OPCODE_NAME_QUERY |
                                 NETB_HFLAG_AUTHORATIVE | NETB_HFLAG_RECURS_DESIRED);
    resp->hdr.answer_rrs = htons(1);

    /* Answer the exact name that was asked for, so the querier can match it. */
    resp->name.nametype = q->nametype;
    memcpy(resp->name.encname, q->encname, sizeof(resp->name.encname));
    resp->name.type     = q->type;
    resp->name.cls      = q->cls;
    resp->name.ttl      = htonl(NETBIOS_NAME_TTL);
    resp->name.datalen  = htons(sizeof(resp->name.flags) + sizeof(resp->name.addr));
    resp->name.flags    = htons(NETB_NFLAG_NODETYPE_BNODE);
    memcpy(resp->name.addr, ip, sizeof(resp->name.addr));

    ESP_LOGI(TAG, "[%s] \"%s\" -> %u.%u.%u.%u", c->name, asked,
             ip[0], ip[1], ip[2], ip[3]);
    return sizeof(*resp);
}

/* ---- responder ----------------------------------------------------------- */

static void netbios_serve(netbios_ctx_t *c, uint8_t *buf, int cap)
{
    const net_sock_ops_t *sk = c->ops->sock;

    c->ops->get_ifname(c->ifname, sizeof(c->ifname));

    int fd = sk->socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) {
        ESP_LOGE(TAG, "[%s] socket() failed: errno %d", c->name, errno);
        return;
    }

    int one = 1;
    /* Both interfaces want the well-known port 137. On one shared LwIP stack
     * (ETH backend) that is only legal with SO_REUSEADDR; SO_BINDTODEVICE below
     * then keeps each responder to its own interface. */
    sk->setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    sk->setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one));
    /* Bounds recvfrom() so a task on a quiet network loops instead of looking
     * wedged; nothing else depends on the period. */
    sock_set_rcvtimeo(sk, fd, NETBIOS_RECV_TIMEOUT_MS);
    sock_bind_iface(sk, fd, c->ifname);

    struct sockaddr_in me = {
        .sin_family = AF_INET,
        .sin_port = htons(NETBIOS_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (sk->bind(fd, (struct sockaddr *)&me, sizeof(me)) < 0) {
        ESP_LOGE(TAG, "[%s] bind to port %d failed: errno %d", c->name, NETBIOS_PORT, errno);
        sk->close(fd);
        return;
    }
    ESP_LOGI(TAG, "[%s] NetBIOS responder for \"%s\" on UDP port %d%s%s",
             c->name, c->nb_name, NETBIOS_PORT,
             c->ifname[0] ? " via netif " : "", c->ifname);

    while (1) {
        struct sockaddr_in src;
        socklen_t sl = sizeof(src);
        int n = sk->recvfrom(fd, buf, cap, 0, (struct sockaddr *)&src, &sl);
        if (n < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                continue;                       /* SO_RCVTIMEO expired */
            }
            /* A hard error would otherwise spin this loop with nothing to
             * block on, starving the idle task. Report it and back off. */
            ESP_LOGW(TAG, "[%s] recvfrom failed: errno %d", c->name, errno);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        if (n == 0) {
            continue;
        }

        const uint8_t *sip = (const uint8_t *)&src.sin_addr.s_addr;
        ESP_LOGI(TAG, "[%s] %d bytes from %u.%u.%u.%u:%u", c->name, n,
                 sip[0], sip[1], sip[2], sip[3], ntohs(src.sin_port));

        netbios_resp_t resp;
        size_t len = netbios_handle(c, buf, (size_t)n, &resp);
        if (len == 0) {
            continue;
        }
        int sent = sk->sendto(fd, &resp, len, 0, (struct sockaddr *)&src, sl);
        if (sent < 0) {
            ESP_LOGW(TAG, "[%s] sendto failed: errno %d", c->name, errno);
        } else {
            ESP_LOGI(TAG, "[%s] answered %d/%u bytes", c->name, sent, (unsigned)len);
        }
    }
}

/* ---- task launcher: same shape as examples/loopback ---------------------- */

static void netbios_task(void *arg)
{
    netbios_ctx_t *c = (netbios_ctx_t *)arg;

    uint8_t *buf = malloc(NETBIOS_BUF_SIZE);
    if (buf == NULL) {
        ESP_LOGE(TAG, "[%s] out of memory for %d-byte buffer", c->name, NETBIOS_BUF_SIZE);
        free(c);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "[%s] waiting for link...", c->name);
    while (!c->is_up()) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    netbios_serve(c, buf, NETBIOS_BUF_SIZE);

    free(buf);       /* netbios_serve only returns on a fatal setup error */
    free(c);
    vTaskDelete(NULL);
}

void netbios_start(const char *name, const netbios_ops_t *ops,
                   const char *nb_name, bool (*is_up)(void))
{
    netbios_ctx_t *c = calloc(1, sizeof(*c));
    if (c == NULL) {
        ESP_LOGE(TAG, "[%s] out of memory", name);
        return;
    }
    c->name = name;
    c->ops = ops;
    c->nb_name = nb_name;
    c->is_up = is_up;

    if (xTaskCreate(netbios_task, name, 4096, c, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "[%s] xTaskCreate failed", name);
        free(c);
    }
}
