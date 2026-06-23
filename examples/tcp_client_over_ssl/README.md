# How to Test TCP Client over SSL Example

## Step 1: Prepare software

The following serial terminal program and SSL test server are required for the TCP Client over SSL example test, download and install from below links.

- [Tera Term][link-tera_term]
- [OpenSSL][link-openssl]

## Step 2: Prepare hardware

1. Connect the WIZnet Ethernet chip (W5500 or W6300) to the ESP32-S3 board over SPI, following the pin table in [Step 3](#step-3-setup-tcp-client-over-ssl-example).
2. Connect an Ethernet cable from the module's RJ45 port to your PC or network.
3. Connect the ESP32-S3 board to your PC with a USB cable.

![][link-hardware]

## Step 3: Setup TCP Client over SSL Example

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

Configure the network settings in the `examples/tcp_client_over_ssl/main/main.c` file.

```cpp
static const wiz_NetInfo g_net_info = {
    .mac = {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}, // MAC address
    .ip  = {192, 168, 11, 2},                    // IP address
    .sn  = {255, 255, 255, 0},                   // Subnet Mask
    .gw  = {192, 168, 11, 1},                    // Gateway
    .dns = {8, 8, 8, 8},                         // DNS server
};
```

### SSL server configuration

Set the target SSL server address in `examples/tcp_client_over_ssl/main/main.c`. The device connects to this PC-side SSL server on port 443.

```cpp
/* Socket */
#define SOCKET_SSL 0

/* Port */
#define PORT_SSL 443

static uint8_t g_ssl_target_ip[4] = {192, 168, 11, 3};
```

Make sure `g_ssl_target_ip` matches the IP of the PC running the OpenSSL test server.

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

Before flashing, start an SSL test server on your PC with OpenSSL. Generate a self-signed certificate (if you do not already have one), then run `s_server` on port 443:

```bash
openssl req -x509 -newkey rsa:2048 -keyout server.key -out server.crt -days 365 -nodes
openssl s_server -accept 443 -cert server.crt -key server.key
```

When the device boots, the assigned IP and the connection progress appear in the terminal.

```
ip: 192.168.11.2
 Connecting to 192.168.11.3:443
 TCP connected, starting TLS handshake
 TLS ok [ Ciphersuite: ... ]
```

![][link-run_socket_open]

After the TCP socket connects, the device runs the mbedTLS handshake against the OpenSSL server. On success it prints the negotiated ciphersuite and sends a test message to the server.
![][link-run_handshake]

The `openssl s_server` console shows the incoming connection and receives the test string `W5x00 TCP over SSL test`. Type any text in the `s_server` console to send it back; the device prints whatever the server sends.
![][link-run_ssl]

## Appendix

- **Certificate verification disabled:** For a dependency-free demo, the client sets `MBEDTLS_SSL_VERIFY_NONE`, so the server certificate is not checked. This lets the handshake succeed against a self-signed OpenSSL certificate. For production, load a CA certificate with `mbedtls_x509_crt_parse`, set `mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED)`, and provide it via `mbedtls_ssl_conf_ca_chain`.
- **mbedTLS and RNG:** mbedTLS is provided by ESP-IDF. The RNG callback is backed by the ESP hardware RNG via `esp_fill_random`, replacing the weak `rand()` used in the original WIZnet-PICO-C example.
- **Timeout:** The connect and TLS read operations use a 10 second timeout (`SSL_RECV_TIMEOUT`). If the handshake fails, confirm the server IP/port and that `s_server` is listening before the device boots.
- **W6300 QSPI mode:** Quad mode (4-bit) requires the extra D2/D3 lines wired and selected in `Component config -> WIZnet TOE Component -> W6300 QSPI mode`. Single mode uses the same 4-wire wiring as W5500.

<!-- Link -->
[link-tera_term]: https://osdn.net/projects/ttssh2/releases/
[link-openssl]: https://www.openssl.org/source/

[link-hardware]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tcp_client_over_ssl/hardware.png
[link-config_main]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tcp_client_over_ssl/config_main.png
[link-config_component]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tcp_client_over_ssl/config_component.png
[link-config_wiz_toe]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tcp_client_over_ssl/config_wiz_toe.png

[link-build_log]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tcp_client_over_ssl/build_log.png
[link-run_socket_open]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tcp_client_over_ssl/run_socket_open.png
[link-run_handshake]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tcp_client_over_ssl/run_handshake.png
[link-run_ssl]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tcp_client_over_ssl/run_ssl.png
