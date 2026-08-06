/*
 * SPDX-License-Identifier: CC0-1.0
 *
 * UPnP IGD protocol steps, ported from WIZnet-PICO-C examples/upnp (UPnP.c).
 * See upnp_core.h for the shape of the port; the notes here cover what the
 * conversion had to change and why.
 *
 * The parsers below are the original's. The five network steps are not: each
 * one used to open a WIZnet socket by number, spin on getSn_SR() until the chip
 * reported the right state, transfer, poll the receive register, and close.
 * None of that survives contact with BSD sockets, so each step now builds a
 * request, hands it to upnp_transport.h, and parses the reply.
 *
 * That also removed a defect rather than porting it. Every receive loop was
 * written as
 *
 *     endTime = my_time + 3;
 *     while (recv(...) <= 0 && my_time < endTime);
 *
 * where my_time is only advanced by data_process_count_handle(), which nothing
 * calls -- in the original, in WIZnet-PICO-C, or in this component. my_time
 * therefore stays 0, the guard never trips, and a router that does not answer
 * hangs the loop forever instead of timing out after three seconds. The same
 * dead counter shows up in the ioLibrary TFTP client (see examples/tftp), so it
 * is worth reporting upstream. Timeouts are the transport's business here and
 * are passed in as milliseconds.
 *
 * The original's inet_addr / inet_ntoa / htons / htonl / ntohs / ntohl and the
 * other helpers around them are gone: the seam takes addresses as strings, so
 * nothing referenced them any more. Only ATOI/C2D, which the parsers use, are
 * kept.
 */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "upnp_core.h"
#include "upnp_transport.h"
#include "upnp_xml.h"

#define CONT_BUFFER_SIZE  (1024 * 2)  /**< Content Buffer Size */
#define SEND_BUFFER_SIZE  (2048 * 2)  /**< Send Buffer Size */
#define RECV_BUFFER_SIZE  (1024 * 4)  /**< Receive Buffer Size */

#define SSDP_GROUP_IP     "239.255.255.250"
#define SSDP_GROUP_PORT   1900
#define SSDP_LOCAL_PORT   1901        /**< port the M-SEARCH is sent from */

/* The original waited "3" of a counter that never advanced. Three seconds is
 * what it meant, and MX:3 in the search header asks responders to answer
 * within that window, so keep it for SSDP and allow more for the HTTP steps --
 * fetching a description can be slow on a busy router. */
#define SSDP_TIMEOUT_MS   3000
#define HTTP_TIMEOUT_MS   5000

static upnp_step_t s_step = UPNP_STEP_NONE;

static char descURL[64]      = {'\0'};  /**< Description URL */
static char descIP[16]       = {'\0'};  /**< Description IP */
static char descPORT[6]      = {'\0'};  /**< Description Port */
static char descLOCATION[64] = {'\0'};  /**< Description Location */
static char controlURL[64]   = {'\0'};  /**< Control URL */
static char eventSubURL[64]  = {'\0'};  /**< Eventing Subscription URL */

static char content[CONT_BUFFER_SIZE]     = {'\0'};  /**< HTTP Body */
static char send_buffer[SEND_BUFFER_SIZE] = {'\0'};  /**< Send Buffer */
static char recv_buffer[RECV_BUFFER_SIZE] = {'\0'};  /**< Receive Buffer */

/**< SSDP Header */
static const char SSDP[] = "\
M-SEARCH * HTTP/1.1\r\n\
Host:239.255.255.250:1900\r\n\
ST:urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n\
Man:\"ssdp:discover\"\r\n\
MX:3\r\n\
\r\n\
";

static int parse_http(const char *xml);
static int parse_ssdp(const char *xml);
static int parse_description(const char *xml);
static int parse_error(const char *xml);
static unsigned short ATOI(const char *str, unsigned short base);

/* --------------------------------------------------------------------------
 * Discovered identity
 * ------------------------------------------------------------------------ */

upnp_step_t upnp_step(void)          { return s_step; }
const char *upnp_igd_ip(void)        { return descIP; }
uint16_t    upnp_igd_port(void)      { return ATOI(descPORT, 10); }
const char *upnp_control_url(void)   { return controlURL; }
const char *upnp_event_sub_url(void) { return eventSubURL; }

/* --------------------------------------------------------------------------
 * Protocol steps
 * ------------------------------------------------------------------------ */

int upnp_discover(void)
{
    memset(recv_buffer, '\0', RECV_BUFFER_SIZE);

    int n = upnp_transport_ssdp(SSDP_GROUP_IP, SSDP_GROUP_PORT, SSDP_LOCAL_PORT,
                                SSDP, recv_buffer, RECV_BUFFER_SIZE,
                                SSDP_TIMEOUT_MS);
    if (n < 0) {
        return UPNP_ERR_TIMEOUT;
    }
    if (n == 0) {
        return UPNP_ERR_TIMEOUT;
    }

    int ret = parse_ssdp(recv_buffer);
    if (ret == UPNP_OK) {
        s_step = UPNP_STEP_DISCOVERED;
    }
    return ret;
}

int upnp_get_description(void)
{
    if (s_step < UPNP_STEP_DISCOVERED) {
        return UPNP_ERR_STEP;
    }

    upnp_xml_get_header(send_buffer, SEND_BUFFER_SIZE,
                        descLOCATION, descIP, descPORT);

    memset(recv_buffer, '\0', RECV_BUFFER_SIZE);
    int n = upnp_transport_http(descIP, upnp_igd_port(), send_buffer,
                                recv_buffer, RECV_BUFFER_SIZE, HTTP_TIMEOUT_MS);
    if (n <= 0) {
        return UPNP_ERR_TIMEOUT;
    }

    int ret = parse_description(recv_buffer);
    if (ret == UPNP_OK) {
        s_step = UPNP_STEP_DESCRIBED;
    }
    return ret;
}

int upnp_subscribe(const char *local_ip, uint16_t event_port)
{
    if (s_step < UPNP_STEP_DESCRIBED) {
        return UPNP_ERR_STEP;
    }

    upnp_xml_subscribe(send_buffer, SEND_BUFFER_SIZE, eventSubURL,
                       descIP, descPORT, local_ip, event_port);

    memset(recv_buffer, '\0', RECV_BUFFER_SIZE);
    int n = upnp_transport_http(descIP, upnp_igd_port(), send_buffer,
                                recv_buffer, RECV_BUFFER_SIZE, HTTP_TIMEOUT_MS);
    if (n <= 0) {
        return UPNP_ERR_TIMEOUT;
    }
    return parse_http(recv_buffer);
}

/* The two port actions differ only in which body they build and which response
 * element confirms them, so they share one exchange. */
static int port_action(upnp_action_t action, const char *expect_response)
{
    if (s_step < UPNP_STEP_DESCRIBED) {
        return UPNP_ERR_STEP;
    }

    /* The POST header carries Content-Length, so the body has to exist first;
     * it is then appended to the header in send_buffer. */
    int body_len = (int)strlen(content);

    upnp_xml_post_header(send_buffer, SEND_BUFFER_SIZE, body_len, action,
                         controlURL, descIP, descPORT);
    size_t used = strlen(send_buffer);
    if (used + body_len >= SEND_BUFFER_SIZE) {
        printf("UPnP: request does not fit the send buffer\r\n");
        return UPNP_ERR_PARSE;
    }
    memcpy(send_buffer + used, content, body_len + 1);

    memset(recv_buffer, '\0', RECV_BUFFER_SIZE);
    int n = upnp_transport_http(descIP, upnp_igd_port(), send_buffer,
                                recv_buffer, RECV_BUFFER_SIZE, HTTP_TIMEOUT_MS);
    if (n <= 0) {
        return UPNP_ERR_TIMEOUT;
    }

    parse_http(recv_buffer);
    if (strstr(recv_buffer, expect_response) == NULL) {
        return parse_error(recv_buffer);
    }
    return UPNP_OK;
}

int upnp_add_port(const char *protocol, uint16_t external_port,
                  const char *internal_ip, uint16_t internal_port,
                  const char *description)
{
    upnp_xml_add_port(content, CONT_BUFFER_SIZE, protocol, external_port,
                      internal_ip, internal_port, description);
    return port_action(UPNP_ACTION_ADD_PORT,
                       "u:AddPortMappingResponse xmlns:u=\"urn:schemas-upnp-org:service:WANIPConnection:1\"");
}

int upnp_delete_port(const char *protocol, uint16_t external_port)
{
    upnp_xml_delete_port(content, CONT_BUFFER_SIZE, protocol, external_port);
    return port_action(UPNP_ACTION_DELETE_PORT,
                       "u:DeletePortMappingResponse xmlns:u=\"urn:schemas-upnp-org:service:WANIPConnection:1\"");
}

/* --------------------------------------------------------------------------
 * String parse functions -- carried over from the original
 * ------------------------------------------------------------------------ */

/**
 * @brief	This function parses the HTTP header.
 * @return	0: success, 1: received xml parse error
 */
static int parse_http(const char *xml)
{
    const char *loc = 0;
    if (strstr(xml, "200 OK") != NULL) {
        return UPNP_OK;
    }
    loc = strstr(xml, "\r\n");
    memset(content, '\0', CONT_BUFFER_SIZE);
    if (loc != NULL) {
        strncpy(content, xml, (size_t)(loc - xml));
    }
    printf("\r\nHTTP Error:\r\n%s\r\n\r\n", content);
    return UPNP_ERR_PARSE;
}

/**
 * @brief	This function parses the received SSDP message from IGD.
 * @return	0: success, 1: received xml parse error
 */
static int parse_ssdp(const char *xml)
{
    const char LOCATION_[] = "LOCATION: ";
    const char *start = 0, *end = 0;

    if (parse_http(xml) != UPNP_OK) return UPNP_ERR_PARSE;

    /* Find Description URL("http://192.168.0.1:3121/etc/linuxigd/gatedesc.xml") */
    if ((start = strstr(xml, LOCATION_)) == NULL) return UPNP_ERR_PARSE;
    if ((end = strstr(start, "\r\n")) == NULL) return UPNP_ERR_PARSE;
    if ((size_t)(end - start - strlen(LOCATION_)) >= sizeof(descURL)) return UPNP_ERR_PARSE;
    memset(descURL, '\0', sizeof(descURL));
    strncpy(descURL, start + strlen(LOCATION_), (size_t)(end - start - strlen(LOCATION_)));

    /* Find IP of IGD("http://192.168.0.1") */
    if ((start = strstr(descURL, "http://")) == NULL) return UPNP_ERR_PARSE;
    if ((end = strstr(start + 7, ":")) == NULL) return UPNP_ERR_PARSE;
    if ((size_t)(end - start - 7) >= sizeof(descIP)) return UPNP_ERR_PARSE;
    memset(descIP, '\0', sizeof(descIP));
    strncpy(descIP, start + 7, (size_t)(end - start - 7));

    /* Find PORT of IGD("3121") */
    start = end + 1;
    if ((end = strstr(start, "/")) == NULL) return UPNP_ERR_PARSE;
    if ((size_t)(end - start) >= sizeof(descPORT)) return UPNP_ERR_PARSE;
    memset(descPORT, '\0', sizeof(descPORT));
    strncpy(descPORT, start, (size_t)(end - start));

    /* Find Description Location("/etc/linuxigd/gatedesc.xml") -- the rest of
     * the URL. memcpy rather than strncpy: a bound taken from the source's own
     * length tells the compiler nothing about the destination, which it rightly
     * flags as a truncation hazard. */
    start = end;
    size_t loc_len = strlen(start);
    if (loc_len >= sizeof(descLOCATION)) return UPNP_ERR_PARSE;
    memcpy(descLOCATION, start, loc_len + 1);

    return UPNP_OK;
}

/**
 * @brief	This function parses the received description message from IGD.
 * @return	0: success, 1: received xml parse error
 */
static int parse_description(const char *xml)
{
    const char service_[]     = "urn:schemas-upnp-org:service:WANIPConnection:1";
    const char controlURL_[]  = "<controlURL>";
    const char eventSubURL_[] = "<eventSubURL>";
    const char *start = 0, *end = 0;

    if (parse_http(xml) != UPNP_OK) return UPNP_ERR_PARSE;

    /* Find Control URL("/etc/linuxigd/gateconnSCPD.ctl") */
    if ((start = strstr(xml, service_)) == NULL) return UPNP_ERR_PARSE;
    if ((start = strstr(start, controlURL_)) == NULL) return UPNP_ERR_PARSE;
    if ((end = strstr(start, "</controlURL>")) == NULL) return UPNP_ERR_PARSE;
    if ((size_t)(end - start - strlen(controlURL_)) >= sizeof(controlURL)) return UPNP_ERR_PARSE;
    memset(controlURL, '\0', sizeof(controlURL));
    strncpy(controlURL, start + strlen(controlURL_), (size_t)(end - start - strlen(controlURL_)));

    /* Find Eventing Subscription URL("/etc/linuxigd/gateconnSCPD.evt") */
    if ((start = strstr(xml, service_)) == NULL) return UPNP_ERR_PARSE;
    if ((start = strstr(start, eventSubURL_)) == NULL) return UPNP_ERR_PARSE;
    if ((end = strstr(start, "</eventSubURL>")) == NULL) return UPNP_ERR_PARSE;
    if ((size_t)(end - start - strlen(eventSubURL_)) >= sizeof(eventSubURL)) return UPNP_ERR_PARSE;
    memset(eventSubURL, '\0', sizeof(eventSubURL));
    strncpy(eventSubURL, start + strlen(eventSubURL_), (size_t)(end - start - strlen(eventSubURL_)));

    return UPNP_OK;
}

/*
 * Copy the text between <tag> and </tag> into content.
 *
 * The original copied straight into a 2 KB buffer with a length taken from the
 * document, so a long enough element ran off the end of it. Everything parsed
 * here arrives from the router, so the length is checked.
 */
static bool copy_element(const char *xml, const char *open, const char *close)
{
    const char *start = strstr(xml, open);
    if (start == NULL) return false;

    const char *end = strstr(start, close);
    if (end == NULL) return false;

    size_t len = (size_t)(end - start) - strlen(open);
    if (len >= CONT_BUFFER_SIZE) return false;

    memcpy(content, start + strlen(open), len);
    content[len] = '\0';
    return true;
}

static void print_element(const char *xml, const char *open, const char *close,
                          const char *label)
{
    if (copy_element(xml, open, close)) {
        printf("Receive Eventing(%s): %s\r\n", label, content);
    }
}

/**
 * @brief	This function parses and prints the received eventing message.
 */
void upnp_parse_eventing(const char *xml)
{
    print_element(xml, "<PossibleConnectionTypes>", "</PossibleConnectionTypes>",
                  "PossibleConnectionTypes");
    print_element(xml, "<ConnectionStatus>", "</ConnectionStatus>",
                  "ConnectionStatus");
    print_element(xml, "<ExternalIPAddress>", "</ExternalIPAddress>",
                  "ExternalIPAddress");
    print_element(xml, "<PortMappingNumberOfEntries>", "</PortMappingNumberOfEntries>",
                  "PortMappingNumberOfEntries");
}

/**
 * @brief	This function parses the received UPnP error message from IGD.
 * @return	0: success, 1: received xml parse error, other: UPnP error code
 */
static int parse_error(const char *xml)
{
    int ret = 0;

    /* Find Fault String */
    if (!copy_element(xml, "<faultstring>", "</faultstring>")) return UPNP_ERR_PARSE;
    printf("faultstring: %s\r\n", content);

    /* Find Error Code */
    if (!copy_element(xml, "<errorCode>", "</errorCode>")) return UPNP_ERR_PARSE;
    printf("errorCode: %s\r\n", content);
    ret = ATOI(content, 10);

    /* Find Error Description */
    if (!copy_element(xml, "<errorDescription>", "</errorDescription>")) return UPNP_ERR_PARSE;
    printf("errorDescription: %s\r\n\r\n", content);

    return ret;
}

/* --------------------------------------------------------------------------
 * The two helpers the parsers still need
 * ------------------------------------------------------------------------ */

static char C2D(unsigned char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    return (char)c;
}

static unsigned short ATOI(const char *str, unsigned short base)
{
    unsigned int num = 0;
    while (*str != 0) {
        num = num * base + C2D((unsigned char)*str++);
    }
    return (unsigned short)num;
}
