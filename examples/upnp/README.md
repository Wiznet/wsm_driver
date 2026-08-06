# How to Test UPnP Example

## Step 1: Prepare software

The following serial terminal program and UPnP-enabled router are required for the UPnP example test, download and install from below links.

- [Tera Term][link-tera_term]

A UPnP-enabled router (Internet Gateway Device, IGD) on the same LAN is also required. No PC test utility is needed beyond the serial terminal; the example is driven entirely from the serial menu, and the result is confirmed in Tera Term and (optionally) on the router's UPnP / port-forwarding page.

## Step 2: Prepare hardware

1. Connect the WIZnet Ethernet chip (W5500 or W6300) to the ESP32-S3 board over SPI, following the pin table in [Step 3](#step-3-setup-upnp-example).
2. Connect an Ethernet cable from the module's RJ45 port to your UPnP-enabled router (the same LAN the router serves).
3. Connect the ESP32-S3 board to your PC with a USB cable.

![][link-hardware]

## Step 3: Setup UPnP Example

### Chip and SPI configuration

Set the target and open menuconfig:

```bash
idf.py set-target esp32s3
idf.py menuconfig
```

Select **Component config**.
![][link-config_main]

Select **WIZnet TOE Component** under Component config.
![][link-config_component]

Choose the WIZnet chip, and check the per-socket buffer size. SPI host, clock, and pins follow the selected chip automatically. In this example, SPI2 of the ESP32-S3 is used at 33 MHz.
![][link-config_wiz_toe]

> This example ships with **W6300** selected by default (`sdkconfig.defaults`). Switch to W5500 under `Component config -> WIZnet TOE Component -> WIZnet chip` if needed.

**W5500 wiring (standard SPI)**

| W5500 | ESP32-S3 Pin |
|-------|--------------|
| MISO  | 13 |
| MOSI  | 11 |
| SCLK  | 12 |
| CS    | 10 |
| RESET | 9  |
| INT   | 14 |

**W6300 wiring (QSPI)**

| W6300   | ESP32-S3 Pin |
|---------|--------------|
| D0 (MOSI) | 11 |
| D1 (MISO) | 13 |
| D2 (IO2)  | 14 *(Quad mode only)* |
| D3 (IO3)  | 9  *(Quad mode only)* |
| SCLK    | 12 |
| CS      | 10 |
| RESET   | 21 |
| INT     | 8  |

### Network configuration

Configure the network settings in `examples/upnp/inc/net_config.h`. The device IP must be a valid host on the router's LAN so the IGD can map a port back to it.

```cpp
#define NET_MAC_ADDR          {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}
#define NET_IP_ADDR           {192, 168, 11, 2}
#define NET_SUBNET_MASK       {255, 255, 255, 0}
#define NET_GATEWAY           {192, 168, 11, 1}
#define NET_DNS_ADDR          {8, 8, 8, 8}

#define NET_IP_ADDR_STR       "192.168.11.2"   /* the same address as text */

#define WIFI_SSID             ""      /* empty -> Ethernet only */
#define WIFI_PASS             ""
```

`NET_IP_ADDR_STR` has to match `NET_IP_ADDR`. The eventing subscription tells the router where to send notifications, and that header is text.

### UPnP configuration

The mapping the example asks for lives in the same file.

```cpp
#define UPNP_MAP_PROTOCOL     "TCP"
#define UPNP_MAP_EXT_PORT     8000
#define UPNP_MAP_INT_IP       NET_IP_ADDR_STR
#define UPNP_MAP_INT_PORT     8000
#define UPNP_MAP_DESCRIPTION  "esp_wiz_toe"

#define UPNP_DELETE_AFTER_ADD 1     /* 0 keeps the mapping for inspection */
#define UPNP_EVENT_LISTEN_SEC 10    /* 0 skips eventing entirely */
```

Set `UPNP_DELETE_AFTER_ADD` to 0 when you want to look at the rule in the router's admin page; at 1 each run leaves the router's table as it found it.

The client sends an SSDP M-SEARCH for `urn:schemas-upnp-org:device:InternetGatewayDevice:1` and drives the `WANIPConnection:1` service to add or delete port mappings.

### Architecture

| file | role |
| --- | --- |
| `inc/upnp_core.h` · `src/upnp_core.c` | the protocol steps and the response parsers |
| `inc/upnp_xml.h` · `src/upnp_xml.c` | builds the SSDP / HTTP / SOAP messages |
| `inc/upnp_transport.h` · `src/upnp_transport.c` | the network seam |
| `src/upnp_client.c` | runs one session: discover, describe, subscribe, map |
| `main/main.c` | orchestration only: bring the interface up, start the session |

The protocol implementation is ioLibrary's, carried in the example rather than used from `third_party`, so its network calls could be swapped for BSD ones without forking the submodule. It reaches the network exclusively through `upnp_transport.h`, whose implementation calls the component's `net_sock_ops_t` vtable — so the same code runs on the W6300's hardware sockets or on the software LwIP behind the Wi-Fi netif.

The seam is coarser than one function per socket call. Each step in the original opened a socket by number, spun on `getSn_SR()` until the chip reported `SOCK_INIT`, connected, spun again for `SOCK_ESTABLISHED`, transferred, polled the receive register, and closed. Those states have no BSD equivalent, so the seam takes a whole exchange — `upnp_transport_http()` sends one request and returns the response — and the chip-specific choreography disappears rather than being emulated.

### Running on Wi-Fi instead (optional)

Fill in `WIFI_SSID` / `WIFI_PASS` and set `UPNP_OVER_WIFI` to 1. Like the tftp example this runs one interface at a time: `upnp_core.c` keeps the discovered IGD in globals, so two concurrent sessions would share it.

On Wi-Fi the callback address advertised to the router is read off the netif after DHCP rather than taken from `NET_IP_ADDR_STR`.

### Differences from the original example

- **The serial menu is gone.** `hyperterminal.c` waited on keystrokes to pick each action, which cannot be exercised without someone at the keyboard. The sequence it drove — discover, describe, subscribe, add, delete — now runs on its own from `net_config.h`. The LED, network-setting and loopback menu entries went with it; loopback has its own example.
- **Timeouts work.** Every receive loop was written as `endTime = my_time + 3; while (recv(...) <= 0 && my_time < endTime);`, where `my_time` is only advanced by `data_process_count_handle()` — which nothing calls, here or upstream. `my_time` stays 0, the guard never trips, and a router that does not answer hangs the loop forever instead of timing out after three seconds. The same dead counter is in the ioLibrary TFTP client, so it is worth reporting upstream.
- **Message building no longer touches the chip.** `MakeSubscribe()` called `getSIPR()` to read the local address out of the WIZnet registers. It is a parameter now, and `upnp_xml.c` has no hardware dependency.
- **Parsed fields are bounded.** The parsers copied element bodies into fixed buffers using lengths taken from the document, so a long enough reply ran past the end of them. Everything here arrives from the router, so the lengths are checked.
- **The byte-order and address helpers are gone.** `UPnP.c` carried its own `inet_addr`, `inet_ntoa`, `htons`, `htonl`, `ntohs`, `ntohl` and friends, whose signatures clash with lwIP's. Because the seam takes addresses as strings, nothing referenced them any more.

## Step 4: Build

After completing the setup, build the project.

```bash
idf.py build
```

![][link-build_log]

## Step 5: Upload and Run

Flash the firmware and open the serial monitor. Replace the port with your board's serial port.

```bash
idf.py -p COMx flash monitor
```

On Linux/macOS:

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

The session runs to completion on its own. A successful run looks like this:

```
I (xxx) wiztoe_net: TOE up: 192.168.11.2 (WIZnet hardware TCP/IP)
I (xxx) upnp: [eth] sending M-SEARCH (1/5)
I (xxx) upnp_tx: SSDP reply from 192.168.11.1 (330 bytes)
I (xxx) upnp: [eth] IGD found at 192.168.11.1:5000
I (xxx) upnp: [eth] controlURL   /ctl/IPConn
I (xxx) upnp: [eth] eventSubURL  /evt/IPConn
I (xxx) upnp: [eth] subscribed to eventing
I (xxx) upnp: [eth] waiting 10s for eventing on port 5002
Receive Eventing(ExternalIPAddress): 203.0.113.7
I (xxx) upnp: [eth] mapped TCP 8000 -> 192.168.11.2:8000 ("esp_wiz_toe")
I (xxx) upnp: [eth] removed the mapping again
I (xxx) upnp: [eth] session finished
```

The log above is what the code prints, not a capture from a run — this example has not been tested against a live router yet.

`sending M-SEARCH` repeats up to five times; a single lost datagram is common and only the fifth failure gives up.

![][link-run_discovery]

### Confirm the mapping

Set `UPNP_DELETE_AFTER_ADD` to 0 and rebuild, then open your router's admin page and look at the **UPnP** or **Port Forwarding** list. A rule for external port `8000` -> `192.168.11.2:8000` with description `esp_wiz_toe` confirms the device created the mapping over UPnP.

![][link-run_router]

## Troubleshooting

### `no IGD answered`

```
E (xxx) upnp: [eth] no IGD answered — is UPnP enabled on the router?
```

The five M-SEARCH probes all went unanswered. In order of likelihood:

- **UPnP is off on the router.** Most consumer routers ship with it enabled, but many ISP-supplied ones do not. Look for "UPnP" or "UPnP IGD" in the admin page and turn it on.
- **The device IP is not on the router's LAN.** `NET_IP_ADDR` has to be a free host address in the router's subnet, and `NET_GATEWAY` has to be the router.
- **The router is not an IGD.** A switch or an access point in bridge mode has nothing to map; the search has to reach the device that owns the WAN connection.
- **The chip could not send to the multicast group.** `sendto()` to `239.255.255.250` needs a destination MAC, and ioLibrary's `sendto()` has no multicast handling — it hands the address to the chip, which tries to resolve it by ARP. If a capture shows ARP requests for `239.255.255.250` and no outgoing M-SEARCH, that is this. The fix belongs in `upnp_transport_ssdp()`, which would set the group before opening the socket the way `examples/udp_multicast` does; `upnp_core.c` would not change.

### `AddPortMapping failed (718)`

A positive value is the router's own UPnP error code, returned verbatim:

| code | meaning |
| --- | --- |
| 402 | invalid arguments |
| 501 | action failed |
| 715 | the router only accepts a wildcard remote host |
| 718 | that external port is already mapped to a different host |
| 725 | the router only accepts permanent leases |

718 usually means a previous run left its mapping behind. Delete it from the router's admin page, or run once with `UPNP_DELETE_AFTER_ADD` at 1.

### `subscribe failed`

Eventing is optional and the run continues without it. Some routers accept the SUBSCRIBE and then never call back, and some reject it outright — neither prevents port mapping. Set `UPNP_EVENT_LISTEN_SEC` to 0 to skip the step.

## Appendix

- **Echo-testing the mapped port:** the original's menu could start a TCP or UDP loopback on the port it had just mapped. That belongs to `examples/loopback`, which does the same thing on both interfaces — run it after this example with `UPNP_DELETE_AFTER_ADD` set to 0.
- **W6300 QSPI mode:** Quad mode (4-bit) requires the extra D2/D3 lines wired and selected in `Component config -> WIZnet TOE Component -> W6300 QSPI mode`. Single mode uses the same 4-wire wiring as W5500.

<!-- Link -->
[link-tera_term]: https://osdn.net/projects/ttssh2/releases/

[link-hardware]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/upnp/hardware.png
[link-config_main]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/upnp/config_main.png
[link-config_component]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/upnp/config_component.png
[link-config_wiz_toe]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/upnp/config_wiz_toe.png

[link-build_log]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/upnp/build_log.png
[link-run_discovery]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/upnp/run_discovery.png
[link-run_router]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/upnp/run_router.png
