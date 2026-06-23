# How to Test TFTP Example

## Step 1: Prepare software

The following serial terminal program and TFTP server are required for the TFTP example test, download and install from below links.

- [Tera Term][link-tera_term]
- [Tftpd64][link-tftpd64]

## Step 2: Prepare hardware

1. Connect the WIZnet Ethernet chip (W5500 or W6300) to the ESP32-S3 board over SPI, following the pin table in [Step 3](#step-3-setup-tftp-example).
2. Connect an Ethernet cable from the module's RJ45 port to your PC or network.
3. Connect the ESP32-S3 board to your PC with a USB cable.

![][link-hardware]

## Step 3: Setup TFTP Example

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

Configure the network settings in the `examples/tftp/main/main.c` file.

```cpp
static const wiz_NetInfo g_net_info = {
    .mac = {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}, // MAC address
    .ip  = {192, 168, 11, 2},                    // IP address
    .sn  = {255, 255, 255, 0},                   // Subnet Mask
    .gw  = {192, 168, 11, 1},                    // Gateway
    .dns = {8, 8, 8, 8},                         // DNS server
};
```

### TFTP configuration

The device acts as a TFTP **client**: it sends a read (download) request to the PC-side TFTP server and reports the result. Set the server IP and the file name to download in `examples/tftp/main/main.c`. The client uses socket 1 and a 2048-byte transfer buffer; the read request uses the standard TFTP port (UDP 69).

```cpp
/* Socket */
#define TFTP_SOCKET_ID 1
#define TFTP_CLIENT_SOCKET_BUFFER_SIZE 2048

/* TFTP server: change to your environment */
#define TFTP_SERVER_IP "192.168.11.100"
#define TFTP_SERVER_FILE_NAME "tftp_test_file.txt"
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

Before flashing, prepare the PC-side TFTP server. Launch **Tftpd64**, open the **Tftp Server** tab, set **Current Directory** to a folder that contains the file named in `TFTP_SERVER_FILE_NAME` (`tftp_test_file.txt`), and set **Server interface** to the PC address `192.168.11.100`. The device and the PC must be on the same subnet.

![][link-run_tftpd64]

If flashing succeeds, the assigned IP appears, then the device sends the read request to the server.

```
ip: 192.168.11.2
tftp server ip: 192.168.11.100, file name: tftp_test_file.txt
send request
```

When the download completes, the device prints the success result in the serial monitor.

```
tftp read success, file name: tftp_test_file.txt
```

![][link-run_tftp_success]

If the file cannot be read (wrong file name, wrong server IP, or the server directory does not contain the file), the device prints the failure result instead.

```
tftp read fail, file name: tftp_test_file.txt
```

## Appendix

- **TFTP server IP / file name:** `TFTP_SERVER_IP` (`192.168.11.100`) must match the **Server interface** address in Tftpd64, and `TFTP_SERVER_FILE_NAME` (`tftp_test_file.txt`) must exist in the served **Current Directory**.
- **Retransmission timer:** An `esp_timer` fires `tftp_timeout_handler()` once per second to drive TFTP retransmission, so a dropped packet is retried automatically.
- **W6300 QSPI mode:** Quad mode (4-bit) requires the extra D2/D3 lines wired and selected in `Component config -> WIZnet TOE Component -> W6300 QSPI mode`. Single mode uses the same 4-wire wiring as W5500.

<!-- Link -->
[link-tera_term]: https://osdn.net/projects/ttssh2/releases/
[link-tftpd64]: https://pjo2.github.io/tftpd64/

[link-hardware]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tftp/hardware.png
[link-config_main]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tftp/config_main.png
[link-config_component]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tftp/config_component.png
[link-config_wiz_toe]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tftp/config_wiz_toe.png

[link-build_log]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tftp/build_log.png
[link-run_tftpd64]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tftp/run_tftpd64.png
[link-run_tftp_success]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tftp/run_tftp_success.png
