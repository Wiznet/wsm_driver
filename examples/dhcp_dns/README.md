# How to Test DHCP & DNS Example

## Step 1: Prepare software

The following serial terminal program and DHCP-enabled network are required for the DHCP & DNS example test, download and install from below links.

- [Tera Term][link-tera_term]

## Step 2: Prepare hardware

1. Connect the WIZnet Ethernet chip (W5500 or W6300) to the ESP32-S3 board over SPI, following the pin table in [Step 3](#step-3-setup-dhcp--dns-example).
2. Connect an Ethernet cable from the module's RJ45 port to a network with a DHCP server (e.g. your router).
3. Connect the ESP32-S3 board to your PC with a USB cable.

![][link-hardware]

## Step 3: Setup DHCP & DNS Example

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

The network settings are defined in `examples/dhcp_dns/main/main.c`. The `.dhcp` field is set to `NETINFO_DHCP`, so the static `ip`/`sn`/`gw`/`dns` values below are only fallbacks — the actual address is leased from the DHCP server at runtime.

```cpp
static wiz_NetInfo g_net_info = {
    .mac = {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}, // MAC address
    .ip  = {192, 168, 11, 2},                    // IP address
    .sn  = {255, 255, 255, 0},                   // Subnet Mask
    .gw  = {192, 168, 11, 1},                    // Gateway
    .dns = {8, 8, 8, 8},                         // DNS server
    .dhcp = NETINFO_DHCP,
};
```

### DHCP & DNS configuration

DHCP uses socket 0 and DNS uses socket 1. The hostname resolved over DNS is set in `examples/dhcp_dns/main/main.c`. By default it queries the DNS server leased from DHCP and resolves `www.wiznet.io`.

```cpp
/* Socket */
#define SOCKET_DHCP 0
#define SOCKET_DNS 1

/* DNS */
static uint8_t g_dns_target_domain[] = "www.wiznet.io";
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

If flashing succeeds, the device starts the DHCP client and prints the leased network information in the terminal.

```
 DHCP client running
 ip  : 192.168.11.100
 sn  : 255.255.255.0
 gw  : 192.168.11.1
 dns : 8.8.8.8
 DHCP leased time : 7200 seconds
 DHCP success
```

The leased IP (`ip`), subnet, gateway, and DNS values come from your DHCP server, so they will differ from the example above.

![][link-run_dhcp]

After the IP is leased, the device resolves the target hostname over DNS and prints the result.

```
 DNS success
 Target domain : www.wiznet.io
 IP of target domain : 211.244.224.36
```

Seeing the `Target domain` and its resolved IP confirms that both DHCP and DNS work. The resolved IP depends on current DNS records and may differ from the value above.

![][link-run_dns]

## Appendix

- **DHCP/DNS retry:** Both clients retry up to 5 times (`DHCP_RETRY_COUNT`, `DNS_RETRY_COUNT`). On timeout you will see `DHCP timeout occurred and retry N` or `DNS timeout occurred and retry N`; after the last retry the task halts with `DHCP failed` / `DNS failed`. Check that the Ethernet cable is connected to a DHCP-enabled network.
- **1-second tick:** The DHCP and DNS retransmission timers are driven by a 1-second `esp_timer` periodic callback (`DHCP_time_handler` / `DNS_time_handler`).
- **W6300 QSPI mode:** Quad mode (4-bit) requires the extra D2/D3 lines wired and selected in `Component config -> WIZnet TOE Component -> W6300 QSPI mode`. Single mode uses the same 4-wire wiring as W5500.

<!-- Link -->
[link-tera_term]: https://osdn.net/projects/ttssh2/releases/

[link-hardware]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/dhcp_dns/hardware.png
[link-config_main]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/dhcp_dns/config_main.png
[link-config_component]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/dhcp_dns/config_component.png
[link-config_wiz_toe]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/dhcp_dns/config_wiz_toe.png

[link-build_log]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/dhcp_dns/build_log.png
[link-run_dhcp]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/dhcp_dns/run_dhcp.png
[link-run_dns]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/dhcp_dns/run_dns.png
