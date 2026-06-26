# How to Test TCP Server Multi Socket Example

## Step 1: Prepare software

The following serial terminal program and TCP/UDP test tool are required for the TCP Server Multi Socket example test, download and install from below links.

- [Tera Term][link-tera_term]
- [Hercules][link-hercules]

## Step 2: Prepare hardware

1. Connect the WIZnet Ethernet chip (W5500 or W6300) to the ESP32-S3 board over SPI, following the pin table in [Step 3](#step-3-setup-tcp-server-multi-socket-example).
2. Connect an Ethernet cable from the module's RJ45 port to your PC or network.
3. Connect the ESP32-S3 board to your PC with a USB cable.

![][link-hardware]

## Step 3: Setup TCP Server Multi Socket Example

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

Configure the network settings in the `examples/tcp_server_multi_socket/main/main.c` file.

```cpp
static const wiz_NetInfo g_net_info = {
    .mac = {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}, // MAC address
    .ip  = {192, 168, 11, 2},                    // IP address
    .sn  = {255, 255, 255, 0},                   // Subnet Mask
    .gw  = {192, 168, 11, 1},                    // Gateway
    .dns = {8, 8, 8, 8},                         // DNS server
};
```

### Server port configuration

Each hardware socket listens on its own TCP port: `PORT_TCP_SERVER + socket number`. With the base port set to 5000 and 8 sockets, the device listens on ports **5000, 5001, … 5007** — one per socket. The base port is set in `examples/tcp_server_multi_socket/main/main.c`.

```cpp
/* Port */
#define PORT_TCP_SERVER 5000
```

The socket count is chip-dependent (`_WIZCHIP_SOCK_NUM_`): both the W5500 and the W6300 provide 8 sockets, and the example opens one listening socket per port (5000..5007) automatically. Clients connect to a different port (5000, 5001, …) for each simultaneous connection.

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

If flashing succeeds, the assigned IP and the multi-socket server startup log appear in the terminal.

```
ip: 192.168.11.2
TCP multi-socket server on ports 5000-5007 (8 sockets)
```

![][link-run_socket_open]

Open Hercules, select the **TCP Client** tab, enter the device IP `192.168.11.2` and port `5000`, then connect. Open several Hercules windows (or several TCP Client tabs) and connect each one to a **different port** (`5000`, `5001`, `5002`, …) at the same IP to use multiple sockets at once.
![][link-run_hercules]

As each client connects, the device prints the socket number and peer address, for example:

```
0:Connected - 192.168.11.100 : 50312
1:Connected - 192.168.11.101 : 50315
```

Send data from each connected Hercules window. The device echoes the same data back to the sender independently, and logs the socket, peer, and message:

```
socket0 from:192.168.11.100 port: 50312  message:hello
socket1 from:192.168.11.101 port: 50315  message:world
```

![][link-run_loopback]

Confirm that every connection echoes its own data back without interfering with the others.

## Appendix

- **Number of simultaneous clients:** The device serves as many connections as the chip has hardware sockets (`_WIZCHIP_SOCK_NUM_`). Each socket is serviced in round-robin from a single task, so all open connections are handled together.
- **W6300 QSPI mode:** Quad mode (4-bit) requires the extra D2/D3 lines wired and selected in `Component config -> WIZnet TOE Component -> W6300 QSPI mode`. Single mode uses the same 4-wire wiring as W5500.

<!-- Link -->
[link-tera_term]: https://osdn.net/projects/ttssh2/releases/
[link-hercules]: https://www.hw-group.com/software/hercules-setup-utility

[link-hardware]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tcp_server_multi_socket/hardware.png
[link-config_main]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tcp_server_multi_socket/config_main.png
[link-config_component]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tcp_server_multi_socket/config_component.png
[link-config_wiz_toe]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tcp_server_multi_socket/config_wiz_toe.png

[link-build_log]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tcp_server_multi_socket/build_log.png
[link-run_socket_open]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tcp_server_multi_socket/run_socket_open.png
[link-run_hercules]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tcp_server_multi_socket/run_hercules.png
[link-run_loopback]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tcp_server_multi_socket/run_loopback.png
