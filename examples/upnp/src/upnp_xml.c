/*
 * SPDX-License-Identifier: CC0-1.0
 *
 * UPnP message builders, ported from WIZnet-PICO-C examples/upnp (MakeXML.c).
 * See upnp_xml.h for what changed and why.
 *
 * The message text below is the original's, kept literal so it can be diffed
 * against upstream.
 */
#include <stdio.h>
#include <string.h>

#include "upnp_xml.h"

/**< SOAP header & tail */
static const char soap_start[] =
"\
<?xml version=\"1.0\"?>\r\n\
<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\" SOAP-ENV:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><SOAP-ENV:Body>\
";

static const char soap_end[] =
"\
</SOAP-ENV:Body></SOAP-ENV:Envelope>\r\n\
";

/**< Delete Port Mapping */
static const char DeletePortMapping_[] = "<m:DeletePortMapping xmlns:m=\"urn:schemas-upnp-org:service:WANIPConnection:1\">";
static const char _DeletePortMapping[] = "</m:DeletePortMapping>";

/**< New Remote Host */
static const char NewRemoteHost_[] = "<NewRemoteHost xmlns:dt=\"urn:schemas-microsoft-com:datatypes\" dt:dt=\"string\">";
static const char _NewRemoteHost[] = "</NewRemoteHost>";

/**< New External Port */
static const char NewExternalPort_[] = "<NewExternalPort xmlns:dt=\"urn:schemas-microsoft-com:datatypes\" dt:dt=\"ui2\">";
static const char _NewExternalPort[] = "</NewExternalPort>";

/**< New Protocol */
static const char NewProtocol_[] = "<NewProtocol xmlns:dt=\"urn:schemas-microsoft-com:datatypes\" dt:dt=\"string\">";
static const char _NewProtocol[] = "</NewProtocol>";

/**< Add Port Mapping */
static const char AddPortMapping_[] = "<m:AddPortMapping xmlns:m=\"urn:schemas-upnp-org:service:WANIPConnection:1\">";
static const char _AddPortMapping[] = "</m:AddPortMapping>";

/**< New Internal Port */
static const char NewInternalPort_[] = "<NewInternalPort xmlns:dt=\"urn:schemas-microsoft-com:datatypes\" dt:dt=\"ui2\">";
static const char _NewInternalPort[] = "</NewInternalPort>";

/**< New Internal Client */
static const char NewInternalClient_[] = "<NewInternalClient xmlns:dt=\"urn:schemas-microsoft-com:datatypes\" dt:dt=\"string\">";
static const char _NewInternalClient[] = "</NewInternalClient>";

/**< New Enabled */
static const char NewEnabled[] = "<NewEnabled xmlns:dt=\"urn:schemas-microsoft-com:datatypes\" dt:dt=\"boolean\">1</NewEnabled>";

/**< New Port Mapping Description */
static const char NewPortMappingDescription_[] = "<NewPortMappingDescription xmlns:dt=\"urn:schemas-microsoft-com:datatypes\" dt:dt=\"string\">";
static const char _NewPortMappingDescription[] = "</NewPortMappingDescription>";

/**< New Lease Duration */
static const char NewLeaseDuration[] = "<NewLeaseDuration xmlns:dt=\"urn:schemas-microsoft-com:datatypes\" dt:dt=\"ui4\">0</NewLeaseDuration>";

/*
 * The original built every message with bare strcat() into a caller-supplied
 * buffer and trusted it to be big enough. The pieces are bounded here instead:
 * the description and the discovered URLs come off the wire, so their length is
 * not ours to assume.
 */
static void append(char *dest, size_t size, const char *src)
{
    size_t used = strlen(dest);
    if (used >= size - 1) {
        return;
    }
    strncat(dest + used, src, size - used - 1);
}

static void append_uint(char *dest, size_t size, unsigned value)
{
    char num[12];
    snprintf(num, sizeof(num), "%u", value);
    append(dest, size, num);
}

/* "Host: 192.168.0.1:3121" -- every request carries the same pair. */
static void append_host(char *dest, size_t size,
                        const char *igd_ip, const char *igd_port)
{
    append(dest, size, "Host: ");
    append(dest, size, igd_ip);
    append(dest, size, ":");
    append(dest, size, igd_port);
}

void upnp_xml_get_header(char *dest, size_t size, const char *desc_location,
                         const char *igd_ip, const char *igd_port)
{
    dest[0] = '\0';
    append(dest, size, "GET ");
    append(dest, size, desc_location);
    append(dest, size, " HTTP/1.1\r\n");
    append(dest, size, "Accept: text/xml, application/xml\r\n");
    append(dest, size, "User-Agent: Mozilla/4.0 (compatible; UPnP/1.0; Windows NT/5.1)\r\n");
    append_host(dest, size, igd_ip, igd_port);
    append(dest, size, "\r\nConnection: Keep-Alive\r\nCache-Control: no-cache\r\nPragma: no-cache\r\n\r\n");
}

void upnp_xml_post_header(char *dest, size_t size, int content_length,
                          upnp_action_t action, const char *control_url,
                          const char *igd_ip, const char *igd_port)
{
    dest[0] = '\0';
    append(dest, size, "POST ");
    append(dest, size, control_url);
    append(dest, size, " HTTP/1.1\r\n");
    append(dest, size, "Content-Type: text/xml; charset=\"utf-8\"\r\n");
    append(dest, size, "SOAPAction: \"urn:schemas-upnp-org:service:WANIPConnection:1#");
    append(dest, size, action == UPNP_ACTION_ADD_PORT ? "AddPortMapping\""
                                                      : "DeletePortMapping\"");
    append(dest, size, "\r\nUser-Agent: Mozilla/4.0 (compatible; UPnP/1.0; Windows NT/5.1)\r\n");
    append_host(dest, size, igd_ip, igd_port);
    append(dest, size, "\r\nContent-Length: ");
    append_uint(dest, size, (unsigned)content_length);
    append(dest, size, "\r\nConnection: Keep-Alive\r\nCache-Control: no-cache\r\nPragma: no-cache\r\n\r\n");
}

void upnp_xml_subscribe(char *dest, size_t size, const char *event_sub_url,
                        const char *igd_ip, const char *igd_port,
                        const char *local_ip, uint16_t listen_port)
{
    dest[0] = '\0';
    append(dest, size, "SUBSCRIBE ");
    append(dest, size, event_sub_url);
    append(dest, size, " HTTP/1.1\r\n");
    append_host(dest, size, igd_ip, igd_port);
    append(dest, size, "\r\nUSER-AGENT: Mozilla/4.0 (compatible; UPnP/1.1; Windows NT/5.1)\r\n");
    append(dest, size, "CALLBACK: <http://");
    append(dest, size, local_ip);
    append(dest, size, ":");
    append_uint(dest, size, listen_port);
    append(dest, size, "/>");
    append(dest, size, "\r\nNT: upnp:event\r\nTIMEOUT: Second-1800\r\n\r\n");
}

void upnp_xml_add_port(char *dest, size_t size, const char *protocol,
                       uint16_t external_port, const char *internal_ip,
                       uint16_t internal_port, const char *description)
{
    dest[0] = '\0';
    append(dest, size, soap_start);
    append(dest, size, AddPortMapping_);
    append(dest, size, NewRemoteHost_);
    append(dest, size, _NewRemoteHost);
    append(dest, size, NewExternalPort_);
    append_uint(dest, size, external_port);
    append(dest, size, _NewExternalPort);
    append(dest, size, NewProtocol_);
    append(dest, size, protocol);
    append(dest, size, _NewProtocol);
    append(dest, size, NewInternalPort_);
    append_uint(dest, size, internal_port);
    append(dest, size, _NewInternalPort);
    append(dest, size, NewInternalClient_);
    append(dest, size, internal_ip);
    append(dest, size, _NewInternalClient);
    append(dest, size, NewEnabled);
    append(dest, size, NewPortMappingDescription_);
    append(dest, size, description);
    append(dest, size, _NewPortMappingDescription);
    append(dest, size, NewLeaseDuration);
    append(dest, size, _AddPortMapping);
    append(dest, size, soap_end);
}

void upnp_xml_delete_port(char *dest, size_t size, const char *protocol,
                          uint16_t external_port)
{
    dest[0] = '\0';
    append(dest, size, soap_start);
    append(dest, size, DeletePortMapping_);
    append(dest, size, NewRemoteHost_);
    append(dest, size, _NewRemoteHost);
    append(dest, size, NewExternalPort_);
    append_uint(dest, size, external_port);
    append(dest, size, _NewExternalPort);
    append(dest, size, NewProtocol_);
    append(dest, size, protocol);
    append(dest, size, _NewProtocol);
    append(dest, size, _DeletePortMapping);
    append(dest, size, soap_end);
}
