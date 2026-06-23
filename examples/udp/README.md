# How to Test UDP Example

## Step 1: Prepare software

The following serial terminal program and UDP test tool are required for the UDP example test, download and install from below links.

- [Tera Term][link-tera_term]
- [Hercules][link-hercules]

## Step 2: Prepare hardware

1. Connect the WIZnet Ethernet chip (W5500 or W6300) to the ESP32-S3 board over SPI, following the pin table in [Step 3](#step-3-setup-udp-example).
2. Connect an Ethernet cable from the module's RJ45 port to your PC or network.
3. Connect the ESP32-S3 board to your PC with a USB cable.

![][link-hardware]

## Step 3: Setup UDP Example

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

Configure the network settings in the `examples/udp/main/main.c` file.

```cpp
static const wiz_NetInfo g_net_info = {
    .mac = {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}, // MAC address
    .ip  = {192, 168, 11, 2},                    // IP address
    .sn  = {255, 255, 255, 0},                   // Subnet Mask
    .gw  = {192, 168, 11, 1},                    // Gateway
    .dns = {8, 8, 8, 8},                         // DNS server
};
```

### UDP role configuration

This example runs as a UDP echo server or a UDP client. Select the role in menuconfig under **UDP Example Configuration -> UDP role** (`EXAMPLE_UDP_ROLE`):

- **UDP server (echo on port 5000)** — `EXAMPLE_UDP_SERVER` (default): the device opens UDP socket 0 on port 5000 and echoes every datagram it receives back to the sender.
- **UDP client (loopback to a peer server)** — `EXAMPLE_UDP_CLIENT`: the device acts as a loopback client to a peer UDP server.

The UDP port is fixed in `examples/udp/main/main.c`:

```cpp
/* Port */
#define PORT_LOOPBACK 5000
```

For the UDP client role, also set the peer (PC) UDP server address in `examples/udp/main/main.c`:

```cpp
/* Peer UDP server for the client role */
static uint8_t g_dest_ip[4] = {192, 168, 11, 100};
```

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

If flashing succeeds, the assigned IP and the UDP server start logs appear in the terminal.

```
ip: 192.168.11.2
UDP server (echo) on port 5000
```

![][link-run_socket_open]

Open Hercules, select the **UDP** tab, set the **Module IP** to the device IP `192.168.11.2` with **Port** `5000`, set a **Local Port**, then send a UDP datagram.
![][link-run_hercules]

The device echoes the same datagram back to your PC, confirming the UDP loopback works.
![][link-run_loopback]

## Appendix

- **UDP client role:** Select `EXAMPLE_UDP_CLIENT` in menuconfig to run the device as a loopback client to a peer UDP server. The peer address is `g_dest_ip` (`192.168.11.100`) and the port is `5000`; run a UDP server (e.g. a Hercules UDP socket on port 5000) on that PC, and the serial log shows `UDP client -> 192.168.11.100:5000`.
- **W6300 QSPI mode:** Quad mode (4-bit) requires the extra D2/D3 lines wired and selected in `Component config -> WIZnet TOE Component -> W6300 QSPI mode`. Single mode uses the same 4-wire wiring as W5500.

<!-- Link -->
[link-tera_term]: https://osdn.net/projects/ttssh2/releases/
[link-hercules]: https://www.hw-group.com/software/hercules-setup-utility

[link-hardware]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/udp/hardware.png
[link-config_main]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/udp/config_main.png
[link-config_component]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/udp/config_component.png
[link-config_wiz_toe]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/udp/config_wiz_toe.png

[link-build_log]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/udp/build_log.png
[link-run_socket_open]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/udp/run_socket_open.png
[link-run_hercules]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/udp/run_hercules.png
[link-run_loopback]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/udp/run_loopback.png
