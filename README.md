# esp_wiz_toe

ESP-IDF component that provides a W5500 SPI port layer for direct WIZnet ioLibrary usage.

## Overview

This component is intentionally minimal.
It only handles:

- SPI transport setup for W5500
- ioLibrary callback registration (`reg_wizchip_*_cbfunc`)
- Hardware reset pin control
- PHY link state query

Application code uses ioLibrary APIs directly (`wizchip_init`, `socket`, `connect`, `send`, `recv`, `sendto`, `recvfrom`, DHCP/DNS APIs).

## Supported ESP-IDF targets

- esp32s3

## Supported WIZnet TOE chips

- W5500 (standard SPI)
- W6300 (QSPI: single 1-bit or quad 4-bit mode)

Other WIZnet chips may be added later.

### W6300 notes

W6300 uses QSPI framing (opcode + 16-bit address + dummy clocks + data) instead
of the W5500 SPI byte/burst interface. The port layer drives it with the
ESP32-S3 SPI master in half-duplex mode.

- **Single mode** (default): uses the same 4-wire wiring as W5500
  (MOSI=IO0, MISO=IO1, SCLK, CS).
- **Quad mode**: requires two extra data lines (IO2/IO3) wired to the chip and
  selected via `Component config -> WIZnet TOE Component -> W6300 QSPI mode`.

Select the chip in menuconfig: `Component config -> WIZnet TOE Component -> WIZnet chip -> W6300`.

## Public API

Header: `include/esp_wiz_toe.h`

- `esp_wiz_toe_spi_init`
- `esp_wiz_toe_spi_deinit`
- `esp_wiz_toe_spi_register_iolib_callbacks`
- `esp_wiz_toe_spi_reset`
- `esp_wiz_toe_spi_wizchip_check`
- `esp_wiz_toe_spi_link_is_up`

## Quick start (clone & build)

The repository root is a component; the buildable projects are the examples.

```bash
git clone --recursive https://github.com/Wiznet/esp_wiz_toe.git
cd esp_wiz_toe
idf.py -C examples/tcp_client build      # -C selects the project directory
```

For VSCode, open `esp_wiz_toe.code-workspace` (File -> Open Workspace from
File). The workspace registers each example as a folder, so the ESP-IDF
extension works as usual — run `ESP-IDF: Pick a Workspace Folder`, choose an
example, then use the normal build/flash/monitor buttons.

## Usage flow

1. Fill `esp_wiz_toe_spi_config_t`
2. Call `esp_wiz_toe_spi_init()`
3. Call `esp_wiz_toe_spi_register_iolib_callbacks()`
4. Call `esp_wiz_toe_spi_reset()`
5. Call ioLibrary init (`wizchip_init`) and network setup (`wizchip_setnetinfo`)
6. Use ioLibrary socket APIs directly

## ioLibrary dependency

`ioLibrary_Driver` is expected under `third_party/ioLibrary_Driver`.

```bash
git submodule add https://github.com/Wiznet/ioLibrary_Driver.git third_party/ioLibrary_Driver
git submodule update --init --recursive
```

If that folder is missing, the component cannot provide ioLibrary callback binding.

## Examples

Available examples (ported from [WIZnet-PICO-C](https://github.com/WIZnet-ioNIC/WIZnet-PICO-C);
each works with W5500 or W6300 — switch the chip in menuconfig — except
`pppoe`, which is W5500-only):

- `examples/loopback` — TCP server loopback (TCP client / UDP via defines)
- `examples/tcp_client` — TCP loopback client
- `examples/tcp_server` — TCP loopback server
- `examples/tcp_server_multi_socket` — same port served on every hardware socket
- `examples/udp` — UDP echo server or client (menuconfig choice)
- `examples/udp_multicast` — UDP multicast receiver (224.0.0.5:30000)
- `examples/dhcp_dns` — DHCP address lease + DNS lookup
- `examples/sntp` — network time from time.google.com
- `examples/tftp` — TFTP client file read
- `examples/http` — HTTP web server on port 80
- `examples/mqtt` — MQTT publish / subscribe / both (menuconfig choice)
- `examples/netbios` — NetBIOS name service responder
- `examples/network_install` — PHY link / cable bring-up check
- `examples/pppoe` — PPPoE session establishment (**W5500 only**; the vendored
  PPPoE driver uses W5500 registers absent on W6300)
- `examples/upnp` — IGD discovery + port mapping via serial menu
- `examples/tcp_client_over_ssl` — TLS client over the WIZnet socket using
  mbedTLS (cert verification disabled for the demo)
- `examples/w6300_loopback` — W6300 reference (chip pinned to W6300)

Not ported from WIZnet-PICO-C: `can` (RP2040 PIO-specific) and `ftp` (FTP
module not present in the ioLibrary submodule).

Each example is an independent ESP-IDF project that uses ioLibrary APIs
directly after SPI port-layer initialization. Build from the example folder:

```bash
cd examples/tcp_client
idf.py build
```

Chip, SPI host/clock/pin defaults and per-socket RX/TX buffer size can be
changed in menuconfig:

- `Component config -> WIZnet TOE Component -> WIZnet chip -> W5500`
- `Component config -> WIZnet TOE Component -> SPI host (2=SPI2, 3=SPI3)`
- `Component config -> WIZnet TOE Component -> SPI clock (Hz)`
- `Component config -> WIZnet TOE Component -> GPIO: MISO/MOSI/SCLK/CS/RESET/INT`
- `Component config -> WIZnet TOE Component -> Per-socket RX/TX buffer size (KB)`

Example-specific endpoint values are kept in each example source for simplicity:

- `examples/tcp_client/main/main.c` (`EXAMPLE_SERVER_IP`, `EXAMPLE_SERVER_PORT`)
- `examples/tcp_server/main/main.c` (`EXAMPLE_LISTEN_PORT`)
- `examples/w6300_loopback/main/main.c` (`EXAMPLE_TCP_LISTEN_PORT`, `EXAMPLE_UDP_LISTEN_PORT`)


## Using this component from ESP Component Registry

You can add this component to a new ESP-IDF project from the ESP Component Registry.

### 1. Create a new ESP-IDF project

```bash
idf.py create-project test_esp_wiz_toe_production
cd test_esp_wiz_toe_production
```
### 2. Set ESP-IDF target

```bash
idf.py set-target {target-chip}
Ex) idf.py set-target esp32s3
```

### 3. Add the component dependency

```bash
idf.py add-dependency "wiznet/esp_wiz_toe=={version}"
Ex) idf.py add-dependency "wiznet/esp_wiz_toe==0.1.0-alpha.5"
```
This command creates main/idf_component.yml automatically and adds the dependency.

After the dependency is resolved during build, the component will be downloaded under:

`managed_components/wiznet__esp_wiz_toe/`

### 4. Add example code to your project

Copy the example application code from one of the component examples into your project main source file.

For example, you can refer to:

`managed_components/wiznet__esp_wiz_toe/examples/tcp_client/main/main.c`
`managed_components/wiznet__esp_wiz_toe/examples/tcp_server/main/main.c`
`managed_components/wiznet__esp_wiz_toe/examples/w6300_loopback/main/main.c`

Then paste or adapt the code into your project source file, for example:

`main/test_esp_wiz_toe_production.c`

### 5. Build & Flash
```bash
idf.py build
idf.py -p COM38 flash monitor
```

![][link-esp_idf_terminal]

![][link-hercules]

## License notes

- This component is MIT licensed.
- `ioLibrary_Driver` is a third-party dependency with its own license terms.


[link-esp_idf_terminal]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/esp_idf_terminal.png
[link-hercules]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/hercules.png
