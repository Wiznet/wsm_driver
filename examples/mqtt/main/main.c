/**
 * MQTT example — ported from WIZnet-PICO-C examples/mqtt
 * (publish / subscribe / publish_subscribe).
 *
 * Select the mode in menuconfig: MQTT Example Configuration -> MQTT mode.
 * Point MQTT_BROKER_IP at your broker (e.g. mosquitto) before flashing.
 * The 1 ms MQTT keep-alive tick (MilliTimer_Handler) uses esp_timer.
 *
 * Works with W5500 or W6300 — select the chip in menuconfig:
 *   Component config -> WIZnet TOE Component -> WIZnet chip
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "esp_wiz_toe.h"
#include "esp_wiz_toe/Ethernet/socket.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wizchip_conf.h"
#include "mqtt_interface.h"
#include "MQTTClient.h"

#if defined(CONFIG_EXAMPLE_MQTT_PUBLISH) || defined(CONFIG_EXAMPLE_MQTT_PUBLISH_SUBSCRIBE)
#define DO_PUBLISH 1
#endif
#if defined(CONFIG_EXAMPLE_MQTT_SUBSCRIBE) || defined(CONFIG_EXAMPLE_MQTT_PUBLISH_SUBSCRIBE)
#define DO_SUBSCRIBE 1
#endif

/* Buffer */
#define ETHERNET_BUF_MAX_SIZE (1024 * 2)

/* Socket */
#define SOCKET_MQTT 0

/* Port */
#define PORT_MQTT 1883

/* Timeout */
#define DEFAULT_TIMEOUT 1000 // 1 second

/* MQTT */
#define MQTT_CLIENT_ID "esp32s3-wiz-toe"
#define MQTT_USERNAME "wiznet"
#define MQTT_PASSWORD "0123456789"
#define MQTT_PUBLISH_TOPIC "publish_topic"
#define MQTT_PUBLISH_PAYLOAD "Hello, World!"
#define MQTT_PUBLISH_PERIOD (1000 * 10) // 10 seconds
#define MQTT_SUBSCRIBE_TOPIC "subscribe_topic"
#define MQTT_KEEP_ALIVE 60

#ifdef CONFIG_ESP_WIZ_TOE_TX_BUF_KB
#define EXAMPLE_TX_BUF_KB CONFIG_ESP_WIZ_TOE_TX_BUF_KB
#else
#define EXAMPLE_TX_BUF_KB 2
#endif
#ifdef CONFIG_ESP_WIZ_TOE_RX_BUF_KB
#define EXAMPLE_RX_BUF_KB CONFIG_ESP_WIZ_TOE_RX_BUF_KB
#else
#define EXAMPLE_RX_BUF_KB 2
#endif

/* Network */
static const wiz_NetInfo g_net_info = {
    .mac = {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}, // MAC address
    .ip = {192, 168, 11, 2},                     // IP address
    .sn = {255, 255, 255, 0},                    // Subnet Mask
    .gw = {192, 168, 11, 1},                     // Gateway
    .dns = {8, 8, 8, 8},                         // DNS server
#if _WIZCHIP_ > W5500
    .ipmode = NETINFO_STATIC_ALL,
#endif
    .dhcp = NETINFO_STATIC,
};

/* MQTT */
static uint8_t g_mqtt_send_buf[ETHERNET_BUF_MAX_SIZE];
static uint8_t g_mqtt_recv_buf[ETHERNET_BUF_MAX_SIZE];
static uint8_t g_mqtt_broker_ip[4] = {192, 168, 11, 100};

static Network g_mqtt_network;
static MQTTClient g_mqtt_client;
static MQTTPacket_connectData g_mqtt_packet_connect_data = MQTTPacket_connectData_initializer;
#ifdef DO_PUBLISH
static MQTTMessage g_mqtt_message;
#endif

static esp_wiz_toe_spi_config_t g_spi_cfg;
static bool s_link_up = false;
static uint8_t g_buf_size_tx[_WIZCHIP_SOCK_NUM_];
static uint8_t g_buf_size_rx[_WIZCHIP_SOCK_NUM_];

static uint32_t millis(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

/* WIZnet chip bring-up through the esp_wiz_toe port layer */
static void wizchip_port_initialize(void)
{
    memset(&g_spi_cfg, 0, sizeof(g_spi_cfg));
    g_spi_cfg.host_id = (spi_host_device_t)CONFIG_ESP_WIZ_TOE_SPI_HOST;
    g_spi_cfg.clock_hz = CONFIG_ESP_WIZ_TOE_SPI_CLOCK_HZ;
    g_spi_cfg.pin_miso = (gpio_num_t)CONFIG_ESP_WIZ_TOE_PIN_MISO;
    g_spi_cfg.pin_mosi = (gpio_num_t)CONFIG_ESP_WIZ_TOE_PIN_MOSI;
    g_spi_cfg.pin_sclk = (gpio_num_t)CONFIG_ESP_WIZ_TOE_PIN_SCLK;
    g_spi_cfg.pin_cs = (gpio_num_t)CONFIG_ESP_WIZ_TOE_PIN_CS;
    g_spi_cfg.pin_int = (gpio_num_t)CONFIG_ESP_WIZ_TOE_PIN_INT;
    g_spi_cfg.pin_rst = (gpio_num_t)CONFIG_ESP_WIZ_TOE_PIN_RST;
#ifdef CONFIG_ESP_WIZ_TOE_PIN_IO2
    g_spi_cfg.pin_io2 = (gpio_num_t)CONFIG_ESP_WIZ_TOE_PIN_IO2;
#else
    g_spi_cfg.pin_io2 = GPIO_NUM_NC;
#endif
#ifdef CONFIG_ESP_WIZ_TOE_PIN_IO3
    g_spi_cfg.pin_io3 = (gpio_num_t)CONFIG_ESP_WIZ_TOE_PIN_IO3;
#else
    g_spi_cfg.pin_io3 = GPIO_NUM_NC;
#endif
    g_spi_cfg.lock_timeout_ms = 5000;

    memset(g_buf_size_tx, EXAMPLE_TX_BUF_KB, sizeof(g_buf_size_tx));
    memset(g_buf_size_rx, EXAMPLE_RX_BUF_KB, sizeof(g_buf_size_rx));

    ESP_ERROR_CHECK(esp_wiz_toe_spi_init(&g_spi_cfg));
    ESP_ERROR_CHECK(esp_wiz_toe_spi_register_iolib_callbacks());
    ESP_ERROR_CHECK(esp_wiz_toe_spi_reset());
    ESP_ERROR_CHECK(esp_wiz_toe_spi_wizchip_check());

    if (wizchip_init(g_buf_size_tx, g_buf_size_rx) != 0) {
        printf("wizchip_init failed\n");
        abort();
    }

    do {
        if (esp_wiz_toe_spi_link_is_up(&s_link_up) != ESP_OK) {
            printf("PHY link state read failed, retrying...\n");
            s_link_up = false;
        } else if (!s_link_up) {
            printf("PHY link down, waiting...\n");
        }

        if (!s_link_up) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    } while (!s_link_up);
    printf("PHY up!\n");

    wizchip_setnetinfo((wiz_NetInfo *)&g_net_info);

    printf("ip: %d.%d.%d.%d -> broker %d.%d.%d.%d:%d\n",
           g_net_info.ip[0], g_net_info.ip[1], g_net_info.ip[2], g_net_info.ip[3],
           g_mqtt_broker_ip[0], g_mqtt_broker_ip[1], g_mqtt_broker_ip[2], g_mqtt_broker_ip[3],
           PORT_MQTT);
#if _WIZCHIP_ > W5500
    {
        uint8_t physr;
        uint32_t waited = 0;
        do {
            physr = getPHYSR();
            if (physr & PHYSR_LNK) break;
            vTaskDelay(pdMS_TO_TICKS(100));
            waited += 100;
        } while (waited < 3000);
        if (!(physr & PHYSR_LNK)) {
            printf("PHY link down after 3 s — check cable\n");
        }
    }
#endif
}

#ifdef DO_SUBSCRIBE
/* MQTT */
static void message_arrived(MessageData *msg_data)
{
    MQTTMessage *message = msg_data->message;

    printf("%.*s : %.*s\n",
           msg_data->topicName->lenstring.len, msg_data->topicName->lenstring.data,
           (int)message->payloadlen, (char *)message->payload);
}
#endif

/* 1 ms tick for the MQTT keep-alive timers */
static void repeating_timer_callback(void *arg)
{
    (void)arg;
    MilliTimer_Handler();
}

static void mqtt_task(void *arg)
{
    (void)arg;
    int32_t retval = 0;
    uint32_t start_ms = 0;
    uint32_t end_ms = 0;

    wizchip_port_initialize();

    // setRCR(0xFF);

    const esp_timer_create_args_t timer_args = {
        .callback = repeating_timer_callback,
        .name = "mqtt_1ms",
    };
    esp_timer_handle_t timer;
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, 1000)); // 1 millisecond

    NewNetwork(&g_mqtt_network, SOCKET_MQTT);

    retval = ConnectNetwork(&g_mqtt_network, g_mqtt_broker_ip, PORT_MQTT);
    if (retval != 1) {
        printf(" Network connect failed\n");
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    /* Initialize MQTT client */
    MQTTClientInit(&g_mqtt_client, &g_mqtt_network, DEFAULT_TIMEOUT,
                   g_mqtt_send_buf, ETHERNET_BUF_MAX_SIZE,
                   g_mqtt_recv_buf, ETHERNET_BUF_MAX_SIZE);

    /* Connect to the MQTT broker */
    g_mqtt_packet_connect_data.MQTTVersion = 3;
    g_mqtt_packet_connect_data.cleansession = 1;
    g_mqtt_packet_connect_data.willFlag = 0;
    g_mqtt_packet_connect_data.keepAliveInterval = MQTT_KEEP_ALIVE;
    g_mqtt_packet_connect_data.clientID.cstring = MQTT_CLIENT_ID;
    g_mqtt_packet_connect_data.username.cstring = MQTT_USERNAME;
    g_mqtt_packet_connect_data.password.cstring = MQTT_PASSWORD;

    retval = MQTTConnect(&g_mqtt_client, &g_mqtt_packet_connect_data);
    if (retval < 0) {
        printf(" MQTT connect failed : %d\n", (int)retval);
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    printf(" MQTT connected\n");

#ifdef DO_PUBLISH
    /* Configure publish message */
    g_mqtt_message.qos = QOS0;
    g_mqtt_message.retained = 0;
    g_mqtt_message.dup = 0;
    g_mqtt_message.payload = MQTT_PUBLISH_PAYLOAD;
    g_mqtt_message.payloadlen = strlen(g_mqtt_message.payload);
#endif

#ifdef DO_SUBSCRIBE
    /* Subscribe */
    retval = MQTTSubscribe(&g_mqtt_client, MQTT_SUBSCRIBE_TOPIC, QOS0, message_arrived);
    if (retval < 0) {
        printf(" Subscribe failed : %d\n", (int)retval);
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    printf(" Subscribed\n");
#endif

    start_ms = millis();

    /* Infinite loop */
    while (1) {
        if ((retval = MQTTYield(&g_mqtt_client, g_mqtt_packet_connect_data.keepAliveInterval)) < 0) {
            printf(" Yield error : %d\n", (int)retval);
            while (1) {
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }

#ifdef DO_PUBLISH
        end_ms = millis();

        if (end_ms > start_ms + MQTT_PUBLISH_PERIOD) {
            start_ms = millis();

            retval = MQTTPublish(&g_mqtt_client, MQTT_PUBLISH_TOPIC, &g_mqtt_message);
            if (retval < 0) {
                printf(" Publish failed : %d\n", (int)retval);
            } else {
                printf(" Published : %s\n", MQTT_PUBLISH_PAYLOAD);
            }
        }
#else
        (void)end_ms;
#endif
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void)
{
    // Sockets are opened in blocking mode; disable Task WDT to avoid resets
    // during long waits and manual network testing.
    esp_task_wdt_delete(NULL);
    esp_task_wdt_deinit();

    xTaskCreate(mqtt_task, "mqtt_task", 16384, NULL, 5, NULL);
}
