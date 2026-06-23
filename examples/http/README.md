# How to Test HTTP Server Example

## Step 1: Prepare software

The following serial terminal program and web browser are required for the HTTP Server example test, download and install from below links.

- [Tera Term][link-tera_term]
- A web browser (e.g. Chrome, Edge, or Firefox)

## Step 2: Prepare hardware

1. Connect the WIZnet Ethernet chip (W5500 or W6300) to the ESP32-S3 board over SPI, following the pin table in [Step 3](#step-3-setup-http-server-example).
2. Connect an Ethernet cable from the module's RJ45 port to your PC or network.
3. Connect the ESP32-S3 board to your PC with a USB cable.

![][link-hardware]

## Step 3: Setup HTTP Server Example

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

Configure the network settings in the `examples/http/main/main.c` file.

```cpp
static const wiz_NetInfo g_net_info = {
    .mac = {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}, // MAC address
    .ip  = {192, 168, 11, 2},                    // IP address
    .sn  = {255, 255, 255, 0},                   // Subnet Mask
    .gw  = {192, 168, 11, 1},                    // Gateway
    .dns = {8, 8, 8, 8},                         // DNS server
};
```

### HTTP server configuration

The HTTP server listens on port 80 and serves the web page across 4 hardware sockets. These settings are defined in `examples/http/main/main.c`.

```cpp
/* Socket */
#define HTTP_SOCKET_MAX_NUM 4

static uint8_t g_http_socket_num_list[HTTP_SOCKET_MAX_NUM] = {0, 1, 2, 3};
```

The page served to the browser is defined in `examples/http/main/web_page.h` and registered as `index.html`.

```cpp
#define index_page  "<!DOCTYPE html>"\
                    "<html lang=\"en\">"\
                    "<head>"\
                        "<meta charset=\"UTF-8\">"\
                        "<title>HTTP Server Example</title>"\
                    "</head>"\
                    "<body>"\
                        "<h1>Hello, World!</h1>"\
                    "</body>"\
                    "</html>"
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

If flashing succeeds, the HTTP server URL appears in the terminal once the chip is brought up.

```
HTTP server: http://192.168.11.2
```

![][link-run_socket_open]

Open a web browser and enter the device URL `http://192.168.11.2/` in the address bar.
![][link-run_browser]

The served web page appears, showing the **Hello, World!** heading from `web_page.h`.
![][link-run_webpage]

## Appendix

- **Concurrent connections:** The server runs `httpServer_run()` over 4 hardware sockets (`0, 1, 2, 3`), so up to four browser connections can be served at once. Adjust `HTTP_SOCKET_MAX_NUM` and `g_http_socket_num_list` to change this.
- **Session timeout:** A 1-second `esp_timer` periodically calls `httpServer_time_handler()` to drive the HTTP session timeout.
- **W6300 QSPI mode:** Quad mode (4-bit) requires the extra D2/D3 lines wired and selected in `Component config -> WIZnet TOE Component -> W6300 QSPI mode`. Single mode uses the same 4-wire wiring as W5500.

<!-- Link -->
[link-tera_term]: https://osdn.net/projects/ttssh2/releases/

[link-hardware]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/http/hardware.png
[link-config_main]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/http/config_main.png
[link-config_component]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/http/config_component.png
[link-config_wiz_toe]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/http/config_wiz_toe.png

[link-build_log]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/http/build_log.png
[link-run_socket_open]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/http/run_socket_open.png
[link-run_browser]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/http/run_browser.png
[link-run_webpage]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/http/run_webpage.png
