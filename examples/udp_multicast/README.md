# How to Test UDP Multicast Example

## Step 1: Prepare software

The following serial terminal program and UDP test tool are required for the UDP Multicast example test, download and install from below links.

- [Tera Term][link-tera_term]
- [Hercules][link-hercules]

## Step 2: Prepare hardware

1. Connect the WIZnet Ethernet chip (W5500 or W6300) to the ESP32-S3 board over SPI, following the pin table in [Step 3](#step-3-setup-udp-multicast-example).
2. Connect an Ethernet cable from the module's RJ45 port to your PC or network.
3. Connect the ESP32-S3 board to your PC with a USB cable.

![][link-hardware]

## Step 3: Setup UDP Multicast Example

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

Network identity, the group and the ports live in `examples/udp_multicast/inc/net_config.h`, the same way as in `examples/loopback`:

```cpp
#define NET_MAC_ADDR          {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}
#define NET_IP_ADDR           {192, 168, 11, 2}
#define NET_SUBNET_MASK       {255, 255, 255, 0}
#define NET_GATEWAY           {192, 168, 11, 1}
#define NET_DNS_ADDR          {8, 8, 8, 8}

#define WIFI_SSID             ""      /* empty -> Ethernet only */
#define WIFI_PASS             ""

#define MCAST_GROUP_IP        "224.0.0.5"
#define MCAST_GROUP_PORT      30000   /* Ethernet */
#define WIFI_MCAST_GROUP_PORT 30001   /* Wi-Fi    */
#define MCAST_BUF_SIZE        2048
```

`main.c` assembles a `wiz_NetInfo` from these and hands it to `wiznet_net_init()`, which applies it to the chip with `wizchip_setnetinfo()`.

`224.0.0.5` is the OSPF All-SPF-Routers group, the same one the WIZnet-PICO-C example uses. Any address in `224.0.0.0/4` works.

### How the group is joined

The engine asks for membership the ordinary BSD way:

```cpp
struct ip_mreq mreq = {
    .imr_multiaddr.s_addr = inet_addr(group),
    .imr_interface.s_addr = htonl(INADDR_ANY),
};
ops->setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
```

On Wi-Fi that is plain LwIP IGMP. On the WIZnet chip the component translates it: the chip filters the group **in hardware**, deriving the multicast MAC for `Sn_DHAR` from `Sn_DIPR` at the moment the socket opens. Because the group has to be in the registers *before* the socket opens — and BSD joins *after* `bind()` — the component closes and reopens the hardware socket with `Sn_MR_MULTI` when the join arrives. A join issued before `bind()` is recorded instead, and `bind()` opens with multicast enabled from the start. Either order works, and the example code stays plain BSD.

`struct ip_mreq` carries no port, so the group port is the port you bound — which is exactly what binding to a group's port means in BSD.

### Architecture

Same layout as `examples/loopback`:

| Path | Role |
|------|------|
| `inc/net_config.h` | network identity, group, ports, buffer size |
| `inc/mcast_rx.h` | engine API |
| `src/mcast_rx.c` | backend-neutral receiver (BSD sockets via a vtable) |
| `main/main.c` | orchestration only: bring interfaces up, start the tasks |

### Running on Wi-Fi at the same time (optional)

Fill in `WIFI_SSID` and the same receiver also comes up on a Wi-Fi STA:

```cpp
mcast_rx_start("eth",  &net_eth_ops,  MCAST_GROUP_IP, MCAST_GROUP_PORT,      wiznet_net_is_up);
mcast_rx_start("wifi", &net_wifi_ops, MCAST_GROUP_IP, WIFI_MCAST_GROUP_PORT, wifi_net_is_up);
```

The two use different ports because with `SOCKET_WRAP=0` (esp_eth backend) they share one LwIP stack, where the same port would clash on bind. Leave `WIFI_SSID` empty when committing.

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

If flashing succeeds, the assigned IP and the joined multicast group appear in the terminal.

```
I (522) wiztoe_net: TOE up: 192.168.11.2 (WIZnet hardware TCP/IP)
I (525) mcast_rx: [eth] waiting for link...
I (528) mcast_rx: [eth] listening to 224.0.0.5:30000
```

With Wi-Fi configured, the second receiver appears once DHCP has assigned an address:

```
I (xxxxx) wifi: got IP 192.168.11.7
I (xxxxx) mcast_rx: [wifi] listening to 224.0.0.5:30001
```

![][link-run_socket_open]

Open Hercules and select the **UDP** tab. Set the module IP to the multicast group `224.0.0.5`, set both **Port** and **Local port** to `30000`, then click **Listen** so Hercules joins the same group. Make sure your PC's active network adapter is on the same subnet (`192.168.11.x`) as the device.
![][link-run_hercules]

Send any data from Hercules to the multicast group. The device receives the datagram and prints it in the serial monitor, confirming multicast reception works.

```
I (xxxxx) mcast_rx: [eth] 5 bytes from 192.168.11.4: hello
```

![][link-run_multicast]

To check that the hardware filter really is filtering, send to a *different* group (say `224.0.0.9`) on the same port — the device should stay silent.

### If nothing arrives: check which adapter Windows sends multicast on

Multicast has no destination host to route toward, so Windows picks the egress adapter purely by interface metric — and a VirtualBox, VMware, WSL or Hyper-V virtual adapter usually has a *lower* metric than your real NIC, which silently wins. Hercules has no way to choose the interface, so the datagram leaves on the virtual adapter and the device never sees it. The device is fine; the packet never reached the wire.

Check the ordering:

```powershell
Get-NetRoute -DestinationPrefix '224.0.0.0/4' |
    Select-Object InterfaceAlias, InterfaceMetric | Sort-Object InterfaceMetric
```

If your Ethernet is not first, give it a lower metric (needs an elevated shell):

```powershell
Set-NetIPInterface -InterfaceIndex <your-ethernet-index> -InterfaceMetric 5
# undo later with: Set-NetIPInterface -InterfaceIndex <idx> -AutomaticMetric Enabled
```

To sidestep the whole issue, send from PowerShell instead — binding the socket to the wired address forces the interface:

```powershell
$local = New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Parse("192.168.11.4"), 0)
$c = New-Object System.Net.Sockets.UdpClient($local)
$b = [System.Text.Encoding]::ASCII.GetBytes("hello multicast")
$c.Send($b, $b.Length, "224.0.0.5", 30000)
$c.Close()
```

Wireshark on the wired adapter with the filter `ip.addr == 224.0.0.5` settles it either way: you should see the device's own IGMPv2 Membership Reports (proof the chip joined) alongside your outgoing datagrams.

## Appendix

- **Multicast group and IGMP:** Any host on the same network that sends to this group:port is received, and multiple receivers can join the same group at once. On the WIZnet chip the filtering happens in hardware, so multicast traffic for other groups never wakes the MCU. On the Wi-Fi side LwIP does the filtering and sends IGMP membership reports, which needs `CONFIG_LWIP_IGMP=y` (pinned in `sdkconfig.defaults`).
- **W6300 QSPI mode:** Quad mode (4-bit) requires the extra D2/D3 lines wired and selected in `Component config -> WIZnet TOE Component -> W6300 QSPI mode`. Single mode uses the same 4-wire wiring as W5500.

<!-- Link -->
[link-tera_term]: https://osdn.net/projects/ttssh2/releases/
[link-hercules]: https://www.hw-group.com/software/hercules-setup-utility

[link-hardware]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/udp_multicast/hardware.png
[link-config_main]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/udp_multicast/config_main.png
[link-config_component]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/udp_multicast/config_component.png
[link-config_wiz_toe]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/udp_multicast/config_wiz_toe.png

[link-build_log]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/udp_multicast/build_log.png
[link-run_socket_open]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/udp_multicast/run_socket_open.png
[link-run_hercules]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/udp_multicast/run_hercules.png
[link-run_multicast]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/udp_multicast/run_multicast.png
