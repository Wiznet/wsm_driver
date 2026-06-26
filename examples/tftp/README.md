# How to Test TFTP Example

## Step 1: Prepare software

The following serial terminal program and TFTP server are required for the TFTP example test, download and install from the links below.

- [Tera Term][link-tera_term]
- [Tftpd64][link-tftpd64]

> **Note:** Tftpd64 is a TFTP server. Do not confuse it with FileZilla (FTP) — TFTP and FTP are different protocols.

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

Configure the network settings in `examples/tftp/main/main.c`.

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

The device acts as a TFTP **client**: it sends a read (download) request to the PC-side TFTP server and reports the result. Set the server IP and the file name to download in `examples/tftp/main/main.c`.

```cpp
/* TFTP server: change to your environment */
#define TFTP_SERVER_IP "192.168.11.4"        // ← your PC's IP address
#define TFTP_SERVER_FILE_NAME "tftp_test_file.txt"
```

`TFTP_SERVER_IP` must match the PC's IP address on the same subnet as the device.

## Step 4: Configure Tftpd64

Open **Tftpd64** and select the **Tftp Server** tab.

Configure the following items:

| Setting                            | Value                                                      |
| ---------------------------------- | ---------------------------------------------------------- |
| Current Directory / Base Directory | Folder that contains `tftp_test_file.txt` (e.g. `C:\tftp`) |
| Server interfaces                  | Your PC's Ethernet IP address (e.g. `192.168.11.4`)        |
| TFTP Security                      | **Standard** or **Read Only**                              |

Create the test file in the base directory. For example, create the following file:

```text
C:\tftp\tftp_test_file.txt
```

Any file content is fine.

> **Important:** The `Server interfaces` field must not be set to `::1` or `127.0.0.1`. Those are loopback interfaces and cannot be accessed by the WIZnet device. Select the actual PC Ethernet interface IP address, such as `192.168.11.4`.

> **Note:** If the device reports `File not found or No Access`, first check that the file is visible from Tftpd64 by pressing **Show Dir**. The file name must exactly match `TFTP_SERVER_FILE_NAME` in the source code.


## Step 5: Build

After completing the setup, build the project.

```bash
idf.py build
```

![][link-build_log]

## Step 6: Upload and Run

Flash the firmware and open the serial monitor. Replace the port with your board's serial port.

```bash
idf.py -p COMx flash monitor
```

On Linux/macOS:

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

When the device boots, the assigned IP appears, then it sends the read request to the server.

```
ip: 192.168.11.2
tftp server ip: 192.168.11.4, file name: tftp_test_file.txt
send request
```

When the download completes, the device prints the success result.

```
tftp read success, file name: tftp_test_file.txt
```

![][link-run_tftp_success]

## Troubleshooting

### `tftp read fail` — File not found or No Access

Check in order:

1. **File exists in the Tftpd64 directory**
   Confirm that `tftp_test_file.txt` exists in the folder configured as **Current Directory / Base Directory**.

2. **Check with Show Dir**
   Press **Show Dir** in Tftpd64. If `tftp_test_file.txt` does not appear there, Tftpd64 cannot access the file.

3. **Check the file extension**
   On Windows, file extensions may be hidden. Make sure the actual file name is not:

   ```text
   tftp_test_file.txt.txt
   ```

4. **Server interface**
   Tftpd64's **Server interfaces** must be set to the same PC IP address used by `TFTP_SERVER_IP` in the code.

   Example:

   ```cpp
   #define TFTP_SERVER_IP "192.168.11.4"
   ```

   Tftpd64 should also be bound to `192.168.11.4`.

5. **Check Tftpd64 Log Viewer**
   Open the **Log Viewer** tab and run the example again. If Tftpd64 receives the request but cannot open the file, it will report a file access or file not found message.

6. **Firewall / port conflict**
   If no request appears in the Tftpd64 log, check Windows Firewall or whether another TFTP server is already using UDP port 69.

   ```powershell
   netstat -ano | Select-String ":69 "
   ```

### `tftp read fail` — Timeout (no response)

- Check Windows Firewall: allow Tftpd64 on UDP port 69 (both inbound and outbound).
- Confirm the device and PC are on the same subnet.

## Appendix

- **TFTP server IP / file name:** `TFTP_SERVER_IP` must match the **Server interfaces** address configured in Tftpd64. `TFTP_SERVER_FILE_NAME` must exist in Tftpd64's **Base Directory**.
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
