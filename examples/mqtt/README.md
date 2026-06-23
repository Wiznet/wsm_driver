# How to Test MQTT Example

## Step 1: Prepare software

The following serial terminal program and MQTT broker/client are required for the MQTT example test, download and install from below links.

- [Tera Term][link-tera_term]
- [mosquitto][link-mosquitto]
- [MQTTX][link-mqttx]

Run a broker (mosquitto) on your PC, and optionally use an MQTT client (MQTTX) to publish and subscribe alongside the device.

## Step 2: Prepare hardware

1. Connect the WIZnet Ethernet chip (W5500 or W6300) to the ESP32-S3 board over SPI, following the pin table in [Step 3](#step-3-setup-mqtt-example).
2. Connect an Ethernet cable from the module's RJ45 port to your PC or network.
3. Connect the ESP32-S3 board to your PC with a USB cable.

![][link-hardware]

## Step 3: Setup MQTT Example

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

Configure the network settings in the `examples/mqtt/main/main.c` file.

```cpp
static const wiz_NetInfo g_net_info = {
    .mac = {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}, // MAC address
    .ip  = {192, 168, 11, 2},                    // IP address
    .sn  = {255, 255, 255, 0},                   // Subnet Mask
    .gw  = {192, 168, 11, 1},                    // Gateway
    .dns = {8, 8, 8, 8},                         // DNS server
};
```

### MQTT broker configuration

Point the broker IP at the PC running mosquitto in `examples/mqtt/main/main.c`. The example connects to the broker on the standard MQTT port 1883.

```cpp
static uint8_t g_mqtt_broker_ip[4] = {192, 168, 11, 3};

#define PORT_MQTT 1883
```

The connection credentials, client ID, topics, and keep-alive are also defined in `examples/mqtt/main/main.c`.

```cpp
#define MQTT_CLIENT_ID       "esp32s3-wiz-toe"
#define MQTT_USERNAME        "wiznet"
#define MQTT_PASSWORD        "0123456789"
#define MQTT_PUBLISH_TOPIC   "publish_topic"
#define MQTT_PUBLISH_PAYLOAD "Hello, World!"
#define MQTT_SUBSCRIBE_TOPIC "subscribe_topic"
#define MQTT_KEEP_ALIVE      60
```

In publish mode the device sends `Hello, World!` to `publish_topic` every 10 seconds.

### MQTT mode

Select the MQTT mode in menuconfig under **MQTT Example Configuration -> MQTT mode**. The default is **Publish and subscribe**.

- **Publish only** — publish `Hello, World!` to `publish_topic` every 10 seconds.
- **Subscribe only** — subscribe to `subscribe_topic` and print arriving messages.
- **Publish and subscribe** — do both at once (default).

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

Before flashing, make sure a broker is running. On the PC, start mosquitto so it listens on port 1883:

```bash
mosquitto -v
```

If flashing succeeds, the assigned IP, target broker, and the connection result appear in the terminal.

```
ip: 192.168.11.2 -> broker 192.168.11.3:1883
 MQTT connected
 Subscribed
```

![][link-run_socket_open]

In **Publish** mode, the device publishes to `publish_topic` every 10 seconds and logs each send.

```
 Published : Hello, World!
```

Subscribe to that topic from your MQTT client to watch the messages arrive. Using MQTTX, create a connection to the broker, then subscribe to `publish_topic`.
![][link-run_subscribe]

In **Subscribe** mode, the device subscribes to `subscribe_topic`. Publish to that topic from your MQTT client (or `mosquitto_pub`), and the device prints the topic and payload.

```bash
mosquitto_pub -h 192.168.11.3 -t subscribe_topic -m "hello from PC"
```

```
subscribe_topic : hello from PC
```

![][link-run_publish]

## Appendix

- **MQTT mode:** `Publish and subscribe` (default) runs both paths at once, so the device publishes to `publish_topic` while also receiving on `subscribe_topic`. Change it under `MQTT Example Configuration -> MQTT mode`.
- **Authentication:** The example connects with username `wiznet` / password `0123456789`. If your broker enforces different credentials (or anonymous access), update `MQTT_USERNAME` / `MQTT_PASSWORD` in `main.c` or your broker config to match.
- **Keep-alive:** A 1 ms `esp_timer` tick drives the MQTT keep-alive timers, with a 60 second keep-alive interval (`MQTT_KEEP_ALIVE`).
- **W6300 QSPI mode:** Quad mode (4-bit) requires the extra D2/D3 lines wired and selected in `Component config -> WIZnet TOE Component -> W6300 QSPI mode`. Single mode uses the same 4-wire wiring as W5500.

<!-- Link -->
[link-tera_term]: https://osdn.net/projects/ttssh2/releases/
[link-mosquitto]: https://mosquitto.org/download/
[link-mqttx]: https://mqttx.app/

[link-hardware]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/mqtt/hardware.png
[link-config_main]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/mqtt/config_main.png
[link-config_component]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/mqtt/config_component.png
[link-config_wiz_toe]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/mqtt/config_wiz_toe.png

[link-build_log]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/mqtt/build_log.png
[link-run_socket_open]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/mqtt/run_socket_open.png
[link-run_subscribe]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/mqtt/run_subscribe.png
[link-run_publish]: https://raw.githubusercontent.com/Wiznet/esp_wiz_toe/main/static/image/mqtt/run_publish.png
