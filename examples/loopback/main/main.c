/**
 * Loopback example — ported from WIZnet-PICO-C examples/loopback.
 *
 * TCP server loopback on port 5000 by default; enable the TCP client / UDP
 * variants with the defines below, same as the original example.
 *
 * Works with W5500 or W6300 — select the chip in menuconfig:
 *   Component config -> WIZnet TOE Component -> WIZnet chip
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "esp_wiz_toe.h"
#include "esp_wiz_toe/Ethernet/socket.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wizchip_conf.h"
#include "loopback.h"
#include "driver/rmt_tx.h"
#if _WIZCHIP_ > W5500
#include "Application/Application.h"
#endif

/* Buffer */
#define ETHERNET_BUF_MAX_SIZE (1024 * 2)

/* Socket */
#define SOCKET_TCP_SERVER 0
#define SOCKET_TCP_CLIENT 1
#define SOCKET_UDP 2

/* Port */
#define PORT_TCP_SERVER 5000
#define PORT_TCP_CLIENT 5001
#define PORT_TCP_CLIENT_DEST 5002
#define PORT_UDP 5003

/* Select the loopback variants to run (same switches as WIZnet-PICO-C) */
#define TCP_SERVER
// #define TCP_CLIENT
// #define UDP

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

/* LED */
#define RMT_LED_STRIP_RESOLUTION_HZ 10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define RMT_LED_STRIP_GPIO_NUM      38

#define EXAMPLE_LED_NUMBERS         1

#define EXAMPLE_FRAME_DURATION_MS   20
#define EXAMPLE_ANGLE_INC_FRAME     0.02
#define EXAMPLE_ANGLE_INC_LED       0.3

static const char *TAG = "loopback_example";
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

static uint8_t g_tcp_client_destip[] = {192, 168, 11, 100};
static uint16_t g_tcp_client_destport = PORT_TCP_CLIENT_DEST;

/* Loopback buffers — keep large networking buffers out of task stack */
static uint8_t g_tcp_server_buf[ETHERNET_BUF_MAX_SIZE];
static uint8_t g_tcp_client_buf[ETHERNET_BUF_MAX_SIZE];
static uint8_t g_udp_buf[ETHERNET_BUF_MAX_SIZE];

static esp_wiz_toe_spi_config_t g_spi_cfg;
static uint8_t g_buf_size_tx[_WIZCHIP_SOCK_NUM_];
static uint8_t g_buf_size_rx[_WIZCHIP_SOCK_NUM_];

static uint8_t led_strip_pixels[EXAMPLE_LED_NUMBERS * 3];

static const rmt_symbol_word_t ws2812_zero = {
    .level0 = 1,
    .duration0 = 0.3 * RMT_LED_STRIP_RESOLUTION_HZ / 1000000, // T0H=0.3us
    .level1 = 0,
    .duration1 = 0.9 * RMT_LED_STRIP_RESOLUTION_HZ / 1000000, // T0L=0.9us
};

static const rmt_symbol_word_t ws2812_one = {
    .level0 = 1,
    .duration0 = 0.9 * RMT_LED_STRIP_RESOLUTION_HZ / 1000000, // T1H=0.9us
    .level1 = 0,
    .duration1 = 0.3 * RMT_LED_STRIP_RESOLUTION_HZ / 1000000, // T1L=0.3us
};

//reset defaults to 50uS
static const rmt_symbol_word_t ws2812_reset = {
    .level0 = 0,
    .duration0 = RMT_LED_STRIP_RESOLUTION_HZ / 1000000 * 50 / 2,
    .level1 = 0,
    .duration1 = RMT_LED_STRIP_RESOLUTION_HZ / 1000000 * 50 / 2,
};

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

    wizchip_setnetinfo((wiz_NetInfo *)&g_net_info);
#if _WIZCHIP_ > W5500
    set_loopback_mode_W6x00(AS_IPV4);
#endif

    printf("ip: %d.%d.%d.%d\n", g_net_info.ip[0], g_net_info.ip[1], g_net_info.ip[2], g_net_info.ip[3]);
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


static size_t encoder_callback(const void *data, size_t data_size,
                               size_t symbols_written, size_t symbols_free,
                               rmt_symbol_word_t *symbols, bool *done, void *arg)
{
    // We need a minimum of 8 symbol spaces to encode a byte. We only
    // need one to encode a reset, but it's simpler to simply demand that
    // there are 8 symbol spaces free to write anything.
    if (symbols_free < 8) {
        return 0;
    }

    // We can calculate where in the data we are from the symbol pos.
    // Alternatively, we could use some counter referenced by the arg
    // parameter to keep track of this.
    size_t data_pos = symbols_written / 8;
    uint8_t *data_bytes = (uint8_t*)data;
    if (data_pos < data_size) {
        // Encode a byte
        size_t symbol_pos = 0;
        for (int bitmask = 0x80; bitmask != 0; bitmask >>= 1) {
            if (data_bytes[data_pos]&bitmask) {
                symbols[symbol_pos++] = ws2812_one;
            } else {
                symbols[symbol_pos++] = ws2812_zero;
            }
        }
        // We're done; we should have written 8 symbols.
        return symbol_pos;
    } else {
        //All bytes already are encoded.
        //Encode the reset, and we're done.
        symbols[0] = ws2812_reset;
        *done = 1; //Indicate end of the transaction.
        return 1; //we only wrote one symbol
    }
}

static void loopback_task(void *arg)
{
    (void)arg;
    int32_t retval = 0;

    wizchip_port_initialize();

    /* Infinite loop */
    while (1) {
#ifdef TCP_SERVER
        /* TCP server loopback test */
        if ((retval = loopback_tcps(SOCKET_TCP_SERVER, g_tcp_server_buf, PORT_TCP_SERVER)) < 0) {
            printf(" loopback_tcps error : %d\n", (int)retval);
            close(SOCKET_TCP_SERVER);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
#endif
#ifdef TCP_CLIENT
        /* TCP client loopback test */
        if ((retval = loopback_tcpc(SOCKET_TCP_CLIENT, g_tcp_client_buf, g_tcp_client_destip, g_tcp_client_destport)) < 0) {
            printf(" loopback_tcpc error : %d\n", (int)retval);
            close(SOCKET_TCP_CLIENT);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
#endif
#ifdef UDP
        /* UDP loopback test */
        if ((retval = loopback_udps(SOCKET_UDP, g_udp_buf, PORT_UDP)) < 0) {
            printf(" loopback_udps error : %d\n", (int)retval);
            close(SOCKET_UDP);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
#endif
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void led_task(void *arg)
{
    (void)arg;

    ESP_LOGI(TAG, "Create RMT TX channel");
    rmt_channel_handle_t led_chan = NULL;
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT, // select source clock
        .gpio_num = RMT_LED_STRIP_GPIO_NUM,
        .mem_block_symbols = 64, // increase the block size can make the LED less flickering
        .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
        .trans_queue_depth = 4, // set the number of transactions that can be pending in the background
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &led_chan));

    ESP_LOGI(TAG, "Create simple callback-based encoder");
    rmt_encoder_handle_t simple_encoder = NULL;
    const rmt_simple_encoder_config_t simple_encoder_cfg = {
        .callback = encoder_callback
        //Note we don't set min_chunk_size here as the default of 64 is good enough.
    };
    ESP_ERROR_CHECK(rmt_new_simple_encoder(&simple_encoder_cfg, &simple_encoder));

    ESP_LOGI(TAG, "Enable RMT TX channel");
    ESP_ERROR_CHECK(rmt_enable(led_chan));

    ESP_LOGI(TAG, "Start LED rainbow chase");
    rmt_transmit_config_t tx_config = {
        .loop_count = 0, // no transfer loop
    };
    float offset = 0;
    float brightness = 0.1f; // 10%

    while(1)
    {
        for (int led = 0; led < EXAMPLE_LED_NUMBERS; led++) {
            // Build RGB pixels. Each color is an offset sine, which gives a
            // hue-like effect.
            float angle = offset + (led * EXAMPLE_ANGLE_INC_LED);
            const float color_off = (M_PI * 2) / 3;
            led_strip_pixels[led * 3 + 0] = (sin(angle + color_off * 0) * 127 + 128)* brightness;
            led_strip_pixels[led * 3 + 1] = (sin(angle + color_off * 1) * 127 + 128)* brightness;
            led_strip_pixels[led * 3 + 2] = (sin(angle + color_off * 2) * 117 + 128)* brightness;
        }
        // Flush RGB values to LEDs
        ESP_ERROR_CHECK(rmt_transmit(led_chan, simple_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
        ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
        vTaskDelay(pdMS_TO_TICKS(EXAMPLE_FRAME_DURATION_MS));
        //Increase offset to shift pattern
        offset += EXAMPLE_ANGLE_INC_FRAME;
        if (offset > 2 * M_PI) {
            offset -= 2 * M_PI;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void)
{
    // Sockets are opened in blocking mode; disable Task WDT to avoid resets
    // during long waits and manual network testing.
    esp_task_wdt_delete(NULL);
    esp_task_wdt_deinit();

    xTaskCreate(loopback_task, "loopback_task", 8192, NULL, 5, NULL);
    xTaskCreate(led_task, "led_task", 8192, NULL, 4, NULL);
}
