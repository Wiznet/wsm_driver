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

Configure the network settings in the `examples/udp_multicast/main/main.c` file.

```cpp
static const wiz_NetInfo g_net_info = {
    .mac = {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}, // MAC address
    .ip  = {192, 168, 11, 2},                    // IP address
    .sn  = {255, 255, 255, 0},                   // Subnet Mask
    .gw  = {192, 168, 11, 1},                    // Gateway
    .dns = {8, 8, 8, 8},                         // DNS server
};
```

### Multicast configuration

The device acts as a multicast **receiver**: it joins the multicast group on socket 0 and prints any datagram sent to that group. Set the multicast group address and port in `examples/udp_multicast/main/main.c`. By default the device joins `224.0.0.5` on port `30000`.

```cpp
static uint8_t  g_multicast_ip[4] = {224, 0, 0, 5}; // multicast ip address
static uint16_t g_multicast_port  = 30000;          // multicast port
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

If flashing succeeds, the assigned IP and the joined multicast group appear in the terminal.

```
ip: 192.168.11.2
multicast group 224.0.0.5:30000
```

![][link-run_socket_open]

Open Hercules and select the **UDP** tab. Set the module IP to the multicast group `224.0.0.5`, set both **Port** and **Local port** to `30000`, then click **Listen** so Hercules joins the same group. Make sure your PC's active network adapter is on the same subnet (`192.168.11.x`) as the device.
![][link-run_hercules]

Send any data from Hercules to the multicast group. The device receives the datagram and prints it in the serial monitor, confirming multicast reception works.
![][link-run_multicast]

## Appendix

- **Multicast group and IGMP:** The device joins group `224.0.0.5:30000` on socket 0 using the ioLibrary `multicast` helper. Any host on the same network that sends to this group:port will be received; multiple receivers can join the same group simultaneously.
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
