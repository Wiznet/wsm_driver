# How to Test TCP Server over SSL Example

## Step 1: Prepare software

The following serial terminal program and SSL test client are required for the TCP Server over SSL example test, download and install from below links.

- [Tera Term][link-tera_term]
- [OpenSSL][link-openssl]

## Step 2: Prepare hardware

1. Connect the WIZnet Ethernet chip (W5500 or W6300) to the ESP32-S3 board over SPI, following the pin table in [Step 3](#step-3-setup-tcp-server-over-ssl-example).
2. Connect an Ethernet cable from the module's RJ45 port to your PC or network.
3. Connect the ESP32-S3 board to your PC with a USB cable.

![][link-hardware]

## Step 3: Setup TCP Server over SSL Example

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

Configure the network settings in the `examples/tcp_server_over_ssl/main/main.c` file. The device serves TLS on this IP, port 443.

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

The listening socket and port are set in `examples/tcp_server_over_ssl/main/main.c`.

```cpp
/* Socket */
#define SOCKET_SSL 0

/* Port */
#define PORT_SSL 443
```

The server presents the bundled mbedTLS test certificate/key (`g_srv_crt_pem` / `g_srv_key_pem`, CN=`localhost`, RSA 2048). These are demo credentials — replace them with your own certificate and key for production.

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

When the device boots, the assigned IP and the listening log appear in the terminal.

```
ip: 192.168.11.2
 SSL server listening on port 443
```

![][link-run_socket_open]

From your PC (on the same `192.168.11.x` network), connect with the OpenSSL test client:

```bash
openssl s_client -connect 192.168.11.2:443
```

To exercise the RSA-only TLS 1.2 ciphersuites the server offers, force TLS 1.2:

```bash
openssl s_client -connect 192.168.11.2:443 -tls1_2 -cipher 'AES128-GCM-SHA256:AES256-SHA256:AES128-SHA256'
```

On connection the device completes the TLS handshake, prints the negotiated ciphersuite, and sends a greeting. Anything you type into the `s_client` console is echoed back:

```
 TCP connection accepted
 SSL handshake complete, ciphersuite TLS-RSA-WITH-AES-128-GCM-SHA256
 Received: hello
```

![][link-run_ssl]

The `s_client` console shows the server certificate (CN=localhost), the greeting `W5x00 SSL server ready`, and your echoed input. Closing the client triggers `Connection closed, waiting for new client`, and the device re-listens for the next connection.

## Appendix

- **Client authentication disabled:** For a dependency-free demo, the server sets `MBEDTLS_SSL_VERIFY_NONE`, so client certificates are not requested/checked. For production, require client certs with `mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED)` and a CA via `mbedtls_ssl_conf_ca_chain`.
- **Server certificate:** The bundled certificate/key come from the mbedTLS test suite (`server2-sha256.crt` / `server2.key`). Browsers and verifying clients will reject it (untrusted CA, CN=localhost); use `openssl s_client` which does not verify by default, or supply your own trusted certificate.
- **Ciphersuites:** The server restricts to RSA key-exchange suites (`TLS-RSA-WITH-AES-256-CBC-SHA256`, `-AES-128-GCM-SHA256`, `-AES-128-CBC-SHA256`), matching the WIZnet-PICO-C reference. These require `CONFIG_MBEDTLS_KEY_EXCHANGE_RSA` (enabled by default and pinned in `sdkconfig.defaults`). If the client negotiates TLS 1.3, that legacy list does not apply and the RSA certificate is used for a TLS 1.3 handshake instead.
- **mbedTLS and RNG:** mbedTLS is provided by ESP-IDF. The RNG callback is backed by the ESP hardware RNG via `esp_fill_random`, replacing the weak `rand()` used in the original WIZnet-PICO-C example.
- **W6300 QSPI mode:** Quad mode (4-bit) requires the extra D2/D3 lines wired and selected in `Component config -> WIZnet TOE Component -> W6300 QSPI mode`. Single mode uses the same 4-wire wiring as W5500.

<!-- Link -->
[link-tera_term]: https://osdn.net/projects/ttssh2/releases/
[link-openssl]: https://www.openssl.org/source/

[link-hardware]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tcp_server_over_ssl/hardware.png
[link-config_main]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tcp_server_over_ssl/config_main.png
[link-config_component]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tcp_server_over_ssl/config_component.png
[link-config_wiz_toe]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tcp_server_over_ssl/config_wiz_toe.png

[link-build_log]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tcp_server_over_ssl/build_log.png
[link-run_socket_open]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tcp_server_over_ssl/run_socket_open.png
[link-run_ssl]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/tcp_server_over_ssl/run_ssl.png
