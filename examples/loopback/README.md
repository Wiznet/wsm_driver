# How to Test Loopback Example

## Step 1: Prepare software

The following serial terminal program and TCP/UDP test tool are required for the Loopback example test, download and install from below links.

- [Tera Term][link-tera_term]
- [Hercules][link-hercules]

## Step 2: Prepare hardware

1. Connect the WIZnet Ethernet chip (W5500 or W6300) to the ESP32-S3 board over SPI, following the pin table in [Step 3](#step-3-setup-loopback-example).
2. Connect an Ethernet cable from the module's RJ45 port to your PC or network.
3. Connect the ESP32-S3 board to your PC with a USB cable.

![][link-hardware]

## Step 3: Setup Loopback Example

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

Configure the network settings in the `examples/loopback/main/main.c` file.

```cpp
static const wiz_NetInfo g_net_info = {
    .mac = {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}, // MAC address
    .ip  = {192, 168, 11, 2},                    // IP address
    .sn  = {255, 255, 255, 0},                   // Subnet Mask
    .gw  = {192, 168, 11, 1},                    // Gateway
    .dns = {8, 8, 8, 8},                         // DNS server
};
```

### Loopback configuration

Select the loopback variant to run and its port in `examples/loopback/main/main.c`. By default the TCP server loopback runs on port 5000.

```cpp
/* Select the loopback variant (only one at a time) */
#define TCP_SERVER
// #define TCP_CLIENT
// #define UDP

/* Port */
#define PORT_TCP_SERVER      5000
#define PORT_TCP_CLIENT      5001
#define PORT_TCP_CLIENT_DEST 5002
#define PORT_UDP             5003
```

For the TCP client variant, also set the destination (PC server) address:

```cpp
static uint8_t  g_tcp_client_destip[] = {192, 168, 11, 100};
static uint16_t g_tcp_client_destport = PORT_TCP_CLIENT_DEST;
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

If flashing succeeds, the assigned IP and the TCP server socket open logs appear in the terminal.

```
ip: 192.168.11.2
```

![][link-run_socket_open]

Open Hercules, select the **TCP Client** tab, enter the device IP `192.168.11.2` and port `5000`, then connect.
![][link-run_hercules]

After connecting, send any data from Hercules. The device echoes the same data back, confirming the loopback works.
![][link-run_loopback]

## Appendix

- **TCP client / UDP variants:** Enable `TCP_CLIENT` or `UDP` instead of `TCP_SERVER` to test the other loopback modes. In TCP client mode the device connects to a PC TCP server (`g_tcp_client_destip`); in UDP mode open a Hercules UDP socket to the device IP on port `5003`.
- **W6300 QSPI mode:** Quad mode (4-bit) requires the extra D2/D3 lines wired and selected in `Component config -> WIZnet TOE Component -> W6300 QSPI mode`. Single mode uses the same 4-wire wiring as W5500.
- **WS2812B LED:** The example also drives an onboard WS2812B LED on GPIO38, cycling colors as a quick visual sanity check that the board is running.

<!-- Link -->
[link-tera_term]: https://osdn.net/projects/ttssh2/releases/
[link-hercules]: https://www.hw-group.com/software/hercules-setup-utility

[link-hardware]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/loopback/hardware.png
[link-config_main]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/loopback/config_main.png
[link-config_component]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/loopback/config_component.png
[link-config_wiz_toe]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/loopback/config_wiz_toe.png

[link-build_log]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/loopback/build_log.png
[link-run_socket_open]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/loopback/run_socket_open.png
[link-run_hercules]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/loopback/run_hercules.png
[link-run_loopback]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/loopback/run_loopback.png
