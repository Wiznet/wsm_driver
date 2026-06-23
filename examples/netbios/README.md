# How to Test NetBIOS Example

## Step 1: Prepare software

The following serial terminal program and NetBIOS name lookup tools are required for the NetBIOS example test, download and install from below links.

- [Tera Term][link-tera_term]

The PC-side test uses the built-in Windows commands `ping` and `nbtstat` — no extra install is required.

## Step 2: Prepare hardware

1. Connect the WIZnet Ethernet chip (W5500 or W6300) to the ESP32-S3 board over SPI, following the pin table in [Step 3](#step-3-setup-netbios-example).
2. Connect an Ethernet cable from the module's RJ45 port to your PC or network.
3. Connect the ESP32-S3 board to your PC with a USB cable.

![][link-hardware]

## Step 3: Setup NetBIOS Example

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

Configure the network settings in the `examples/netbios/main/main.c` file.

```cpp
static const wiz_NetInfo g_net_info = {
    .mac = {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}, // MAC address
    .ip  = {192, 168, 11, 2},                    // IP address
    .sn  = {255, 255, 255, 0},                   // Subnet Mask
    .gw  = {192, 168, 11, 1},                    // Gateway
    .dns = {8, 8, 8, 8},                         // DNS server
};
```

### NetBIOS configuration

The NetBIOS responder runs on socket 3, defined in `examples/netbios/main/main.c`:

```cpp
/* Socket */
#define SOCK_NETBIOS 3
```

The registered NetBIOS name and the name-service UDP port are defined in `examples/netbios/main/netbios.c`:

```cpp
#define NETBIOS_BOARD_NAME     "W55RP20"             /*Define the NetBIOS name*/
#define NETBIOS_PORT           137                   /*The default port for the NetBIOS name service*/
```

The device answers NetBIOS name queries for the name `W55RP20`. Change `NETBIOS_BOARD_NAME` if you want the board to respond to a different hostname.

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

If flashing succeeds, the assigned IP appears and the NetBIOS socket opens on UDP port 137.

```
ip: 192.168.11.2
3:Opened, UDP loopback, port [137]
```

![][link-run_socket_open]

On a Windows PC connected to the same network, resolve the device by its NetBIOS name. Open a Command Prompt and run:

```bash
ping W55RP20
```

The name resolves to the device IP `192.168.11.2` and replies are returned, confirming the NetBIOS responder works.
![][link-run_ping]

When a name query arrives, the serial monitor logs the requester, the decoded name, and the response that was sent:

```
rem_ip_addr=192.168.11.100:137
netbios name query question
name is W55RP20


!! name is correct !!

send response
```

![][link-run_serial]

Alternatively, query the NetBIOS name table for the device IP directly:

```bash
nbtstat -A 192.168.11.2
```

The device's registered name `W55RP20` is listed in the returned name table.
![][link-run_nbtstat]

## Appendix

- **NetBIOS name:** The board responds only to the exact name in `NETBIOS_BOARD_NAME` (`W55RP20`). NetBIOS names are case-insensitive and limited to 16 characters (`NETBIOS_NAME_LEN`).
- **Same subnet required:** NetBIOS name service uses UDP broadcast on port 137, so the PC and the device must be on the same local subnet (`192.168.11.x` here). Name resolution does not cross routers.
- **W6300 QSPI mode:** Quad mode (4-bit) requires the extra D2/D3 lines wired and selected in `Component config -> WIZnet TOE Component -> W6300 QSPI mode`. Single mode uses the same 4-wire wiring as W5500.

<!-- Link -->
[link-tera_term]: https://osdn.net/projects/ttssh2/releases/

[link-hardware]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/netbios/hardware.png
[link-config_main]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/netbios/config_main.png
[link-config_component]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/netbios/config_component.png
[link-config_wiz_toe]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/netbios/config_wiz_toe.png

[link-build_log]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/netbios/build_log.png
[link-run_socket_open]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/netbios/run_socket_open.png
[link-run_ping]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/netbios/run_ping.png
[link-run_serial]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/netbios/run_serial.png
[link-run_nbtstat]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/netbios/run_nbtstat.png
