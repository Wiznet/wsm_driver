/*
 * SPDX-License-Identifier: CC0-1.0
 *
 * UPnP IGD protocol steps, ported from WIZnet-PICO-C examples/upnp (UPnP.c).
 *
 * The message construction and the response parsers are the original's. What
 * changed is everything below them: each step used to take a WIZnet socket
 * number and drive the chip directly, and now each one builds a request, hands
 * it to upnp_transport.h, and parses what comes back. The socket argument is
 * gone because the seam owns the socket for the duration of one exchange.
 *
 * The steps are ordered and the later ones refuse to run early -- the control
 * URL only exists once the description has been fetched. upnp_step() reports
 * how far the session has got.
 */
#ifndef UPNP_CORE_H
#define UPNP_CORE_H

#include <stdint.h>

/* How far the session has progressed. Each step raises it by one. */
typedef enum {
    UPNP_STEP_NONE = 0,     /* nothing discovered yet */
    UPNP_STEP_DISCOVERED,   /* SSDP found an IGD: descIP / descPORT known */
    UPNP_STEP_DESCRIBED,    /* description fetched: controlURL / eventSubURL known */
} upnp_step_t;

/* Shared result codes. Port actions may also return a positive UPnP error code
 * straight from the router (718 "ConflictInMappingEntry" and friends). */
#define UPNP_OK             0
#define UPNP_ERR_PARSE      1     /* the reply arrived but did not parse */
#define UPNP_ERR_TIMEOUT  (-1)    /* nobody answered in time */
#define UPNP_ERR_STEP     (-2)    /* called before the prerequisite step */

upnp_step_t upnp_step(void);

/* Discovered identity, valid from the step that fills it onward. */
const char *upnp_igd_ip(void);          /* after UPNP_STEP_DISCOVERED */
uint16_t    upnp_igd_port(void);        /* after UPNP_STEP_DISCOVERED */
const char *upnp_control_url(void);     /* after UPNP_STEP_DESCRIBED */
const char *upnp_event_sub_url(void);   /* after UPNP_STEP_DESCRIBED */

/* Step 1: multicast an M-SEARCH and parse the first reply. */
int upnp_discover(void);

/* Step 2: GET the device description to learn the control and eventing URLs. */
int upnp_get_description(void);

/* Step 3 (optional): subscribe to eventing, asking the IGD to call back to
 * local_ip:event_port. */
int upnp_subscribe(const char *local_ip, uint16_t event_port);

/* Port mapping. protocol is "TCP" or "UDP". */
int upnp_add_port(const char *protocol, uint16_t external_port,
                  const char *internal_ip, uint16_t internal_port,
                  const char *description);
int upnp_delete_port(const char *protocol, uint16_t external_port);

/* Print the interesting fields of an eventing notification. */
void upnp_parse_eventing(const char *xml);

#endif /* UPNP_CORE_H */
