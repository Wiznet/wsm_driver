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

Configure the network settings in the `examples/upnp/main/main.c` file. The device IP must be a valid host on the router's LAN so the IGD can map a port back to it.

```cpp
static const wiz_NetInfo g_net_info = {
    .mac = {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}, // MAC address
    .ip  = {192, 168, 11, 2},                    // IP address
    .sn  = {255, 255, 255, 0},                   // Subnet Mask
    .gw  = {192, 168, 11, 1},                    // Gateway
    .dns = {8, 8, 8, 8},                         // DNS server
};
```

### UPnP configuration

The socket and the loopback ports advertised by the menu are set in `examples/upnp/main/main.c`. Socket 0 runs the UPnP control point; the TCP/UDP loopback that the menu can start uses ports 8000 / 5000.

```cpp
/* Socket */
#define SOCKET_UPNP 0

/* Port */
#define PORT_TCP 8000
#define PORT_UDP 5000
```

The control point sends an SSDP M-SEARCH for `urn:schemas-upnp-org:device:InternetGatewayDevice:1` and drives the `WANIPConnection:1` service to add or delete port mappings. These defaults match the original WIZnet-PICO-C example; no change is normally needed.

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

On boot the device prints the assigned IP, then discovers the router (IGD) over SSDP, fetches its description, and subscribes to its events.

```
wiznet chip upnp example.
ip: 192.168.11.2
Send SSDP..
GetDescription Success!!
SetEventing Success!!
```

`Send SSDP..` repeats until a UPnP-enabled router replies. If discovery never succeeds, confirm the router has UPnP enabled and that the device IP is on its LAN.

![][link-run_discovery]

After discovery, the serial menu (the **WIZnet Chip Control Point**) appears in Tera Term. Type the number and press Enter to run an option.

```
====================== WIZnet Chip Control Point ===================
This Application is basic example of UART interface with
Windows Hyper Terminal.

==========================================================
                          APPLICATION MENU :

==========================================================

 1 - Set LED on
 2 - Set LED off
 3 - Show network setting
 4 - Set  network setting
 5 - Run TCP Loopback
 6 - Run UDP Loopback
 7 - UPnP PortForwarding: AddPort
 8 - UPnP PortForwarding: DeletePort
Enter your choice :
```

![][link-run_menu]

### Add a port mapping

Choose **7** to add a port forward. The menu prompts for the protocol and the external/internal port numbers; the internal IP is filled in automatically from the device's own IP.

```
Enter your choice : 7

Type a Protocol(TCP/UDP) : TCP

Type a External Port Number : 8000

Type a Internal Port Number : 8000
```

On success the device prints:

```
AddPort Success!!
```

A non-zero `AddPort Error Code is <n>` instead means the IGD rejected the request (for example, the port is already mapped).

![][link-run_addport]

### Confirm the mapping

Open your router's admin page and look at the **UPnP** or **Port Forwarding** list. A new rule for external port `8000` -> `192.168.11.2:8000` (description `W5500_uPnPGetway`) confirms the device created the mapping over UPnP.

![][link-run_router]

### Delete a port mapping

Choose **8**, enter the same protocol and external port number, and the mapping is removed:

```
Enter your choice : 8

Type a Protocol(TCP/UDP) : TCP

Type a External Port Number : 8000
DeletePort Success!!
```

The rule then disappears from the router's UPnP / port-forwarding list.

## Appendix

- **Other menu options:** `1` / `2` toggle the user LED (printed as `[USER LED] ON/OFF`), `3` shows the current network setting, and `4` lets you re-enter IP / subnet / gateway / DNS over the serial console.
- **TCP / UDP loopback:** `5` runs a TCP loopback on port `8000` and `6` runs a UDP loopback on port `5000`; press `Q` to stop either and return to the menu. These let a host echo-test the device on the port you just mapped.
- **Cancel input:** at the numeric prompts you can type `A` to cancel and return to the menu.
- **W6300 QSPI mode:** Quad mode (4-bit) requires the extra D2/D3 lines wired and selected in `Component config -> WIZnet TOE Component -> W6300 QSPI mode`. Single mode uses the same 4-wire wiring as W5500.

<!-- Link -->
[link-tera_term]: https://osdn.net/projects/ttssh2/releases/

[link-hardware]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/upnp/hardware.png
[link-config_main]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/upnp/config_main.png
[link-config_component]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/upnp/config_component.png
[link-config_wiz_toe]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/upnp/config_wiz_toe.png

[link-build_log]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/upnp/build_log.png
[link-run_discovery]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/upnp/run_discovery.png
[link-run_menu]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/upnp/run_menu.png
[link-run_addport]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/upnp/run_addport.png
[link-run_router]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/upnp/run_router.png
