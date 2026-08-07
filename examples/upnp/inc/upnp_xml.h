/*
 * SPDX-License-Identifier: CC0-1.0
 *
 * UPnP message builders, ported from WIZnet-PICO-C examples/upnp (MakeXML.c).
 *
 * The message text is the original's. Two things changed:
 *
 *   - The builders took the discovered URLs from globals declared `extern` in
 *     MakeXML.c, one of which (descPORT) was declared with a different size
 *     than its definition. They are passed in now, so there is one declaration
 *     of each and it lives in upnp_core.c.
 *
 *   - MakeSubscribe() read the local address straight off the chip with
 *     getSIPR(), which tied message construction to the WIZnet hardware. The
 *     address is a parameter now and this file has no chip dependency left.
 */
#ifndef UPNP_XML_H
#define UPNP_XML_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    UPNP_ACTION_DELETE_PORT = 0,
    UPNP_ACTION_ADD_PORT    = 1,
} upnp_action_t;

/* HTTP GET for the device description. */
void upnp_xml_get_header(char *dest, size_t size,
                         const char *desc_location,
                         const char *igd_ip, const char *igd_port);

/* HTTP POST header for a SOAP control action; the body is appended by the
 * caller because Content-Length has to be known first. */
void upnp_xml_post_header(char *dest, size_t size, int content_length,
                          upnp_action_t action,
                          const char *control_url,
                          const char *igd_ip, const char *igd_port);

/* SUBSCRIBE request asking the IGD to notify local_ip:listen_port. */
void upnp_xml_subscribe(char *dest, size_t size,
                        const char *event_sub_url,
                        const char *igd_ip, const char *igd_port,
                        const char *local_ip, uint16_t listen_port);

/* SOAP bodies for the two port-mapping actions. */
void upnp_xml_add_port(char *dest, size_t size, const char *protocol,
                       uint16_t external_port, const char *internal_ip,
                       uint16_t internal_port, const char *description);
void upnp_xml_delete_port(char *dest, size_t size, const char *protocol,
                          uint16_t external_port);

#endif /* UPNP_XML_H */
