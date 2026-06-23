# How to Test SNTP Example

## Step 1: Prepare software

The following serial terminal program is required for the SNTP example test, download and install from the below link.

- [Tera Term][link-tera_term]

## Step 2: Prepare hardware

1. Connect the WIZnet Ethernet chip (W5500 or W6300) to the ESP32-S3 board over SPI, following the pin table in [Step 3](#step-3-setup-sntp-example).
2. Connect an Ethernet cable from the module's RJ45 port to your network so the device can reach the internet (gateway and DNS must be valid).
3. Connect the ESP32-S3 board to your PC with a USB cable.

![][link-hardware]

## Step 3: Setup SNTP Example

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

Configure the network settings in the `examples/sntp/main/main.c` file. The gateway and DNS must point to a working internet route so the device can reach the SNTP server.

```cpp
static const wiz_NetInfo g_net_info = {
    .mac = {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}, // MAC address
    .ip  = {192, 168, 11, 2},                    // IP address
    .sn  = {255, 255, 255, 0},                   // Subnet Mask
    .gw  = {192, 168, 11, 1},                    // Gateway
    .dns = {8, 8, 8, 8},                         // DNS server
};
```

### SNTP configuration

The SNTP server, timezone, and socket are set in `examples/sntp/main/main.c`. By default the device queries `time.google.com` (`216.239.35.0`) using socket 0, with the timezone set to Korea (`TIMEZONE 40`).

```cpp
/* Socket */
#define SOCKET_SNTP 0

/* Timeout */
#define RECV_TIMEOUT (1000 * 10) // 10 seconds

/* Timezone */
#define TIMEZONE 40 // Korea

static uint8_t g_sntp_server_ip[4] = {216, 239, 35, 0}; // time.google.com
```

To query a different server or set a different timezone, change `g_sntp_server_ip` and the `TIMEZONE` value.

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

If flashing succeeds, the assigned IP appears in the terminal.

```
ip: 192.168.11.2
```

![][link-run_socket_open]

The device then queries the SNTP server and prints the current date and time in the serial monitor in `yy-mo-dd, hh:mm:ss` format (the timezone offset from `TIMEZONE` is already applied).

```
 2026-6-23, 14:5:30
```

![][link-run_time]

No separate PC tool is needed — the result is verified entirely in the serial monitor.

## Appendix

- **SNTP failed:** If `SNTP failed : 0` is printed, the device could not reach the server within the 10-second timeout (`RECV_TIMEOUT`). Check that the Ethernet cable is connected and that the gateway and DNS in `g_net_info` provide a valid internet route.
- **Timezone:** `TIMEZONE` is a half-hour-step offset code from the WIZnet SNTP library; `40` corresponds to Korea (UTC+9). Adjust it for your region.
- **W6300 QSPI mode:** Quad mode (4-bit) requires the extra D2/D3 lines wired and selected in `Component config -> WIZnet TOE Component -> W6300 QSPI mode`. Single mode uses the same 4-wire wiring as W5500.

<!-- Link -->
[link-tera_term]: https://osdn.net/projects/ttssh2/releases/

[link-hardware]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/sntp/hardware.png
[link-config_main]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/sntp/config_main.png
[link-config_component]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/sntp/config_component.png
[link-config_wiz_toe]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/sntp/config_wiz_toe.png

[link-build_log]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/sntp/build_log.png
[link-run_socket_open]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/sntp/run_socket_open.png
[link-run_time]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/sntp/run_time.png
