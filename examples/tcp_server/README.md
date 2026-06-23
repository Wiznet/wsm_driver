# How to Test TCP Server Example

## Step 1: Prepare software

The following serial terminal program and TCP test tool are required for the TCP Server example test, download and install from below links.

- [Tera Term][link-tera_term]
- [Hercules][link-hercules]

## Step 2: Prepare hardware

1. Connect the WIZnet Ethernet chip (W5500 or W6300) to the ESP32-S3 board over SPI, following the pin table in [Step 3](#step-3-setup-tcp-server-example).
2. Connect an Ethernet cable from the module's RJ45 port to your PC or network.
3. Connect the ESP32-S3 board to your PC with a USB cable.

![][link-hardware]

## Step 3: Setup TCP Server Example

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

Configure the network settings in the `examples/tcp_server/main/main.c` file.

```cpp
static const wiz_NetInfo s_net_info = {
    .mac = {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}, // MAC address
    .ip  = {192, 168, 11, 2},                    // IP address
    .sn  = {255, 255, 255, 0},                   // Subnet Mask
    .gw  = {192, 168, 11, 1},                    // Gateway
    .dns = {8, 8, 8, 8},                         // DNS server
    .dhcp = NETINFO_STATIC,
};
```

### TCP server configuration

The device runs a TCP server loopback on socket 0. It listens on `EXAMPLE_LISTEN_PORT`, which defaults to port 5000 in `examples/tcp_server/main/main.c`.

```cpp
#define EXAMPLE_SOCKET_NUM 0
static const uint16_t EXAMPLE_LISTEN_PORT = 5000;
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

If flashing succeeds, the device opens socket 0 and starts listening on port 5000.

```
0:Socket opened
0:Listen, TCP server loopback, port [5000]
```

![][link-run_socket_open]

Open Hercules, select the **TCP Client** tab, enter the device IP `192.168.11.2` and port `5000`, then click **Connect**.
![][link-run_hercules]

When the client connects, the device logs the peer address.

```
0:Connected - 192.168.11.100 : 50000
```

![][link-run_connected]

After connecting, send any data from Hercules. The device echoes the same data back to the client, confirming the TCP server loopback works.
![][link-run_loopback]

## Appendix

- **Closing the connection:** When the client disconnects, the device handles `SOCK_CLOSE_WAIT`, logs `0:Socket Closed`, and reopens the listening socket so the next client can connect.
- **W6300 QSPI mode:** Quad mode (4-bit) requires the extra D2/D3 lines wired and selected in `Component config -> WIZnet TOE Component -> W6300 QSPI mode`. Single mode uses the same 4-wire wiring as W5500.

<!-- Link -->
[link-tera_term]: https://osdn.net/projects/ttssh2/releases/
[link-hercules]: https://www.hw-group.com/software/hercules-setup-utility

[link-hardware]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tcp_server/hardware.png
[link-config_main]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tcp_server/config_main.png
[link-config_component]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tcp_server/config_component.png
[link-config_wiz_toe]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tcp_server/config_wiz_toe.png

[link-build_log]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tcp_server/build_log.png
[link-run_socket_open]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tcp_server/run_socket_open.png
[link-run_hercules]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tcp_server/run_hercules.png
[link-run_connected]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tcp_server/run_connected.png
[link-run_loopback]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tcp_server/run_loopback.png
