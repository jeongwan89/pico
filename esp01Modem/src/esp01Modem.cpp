#include <cstdio>
#include <memory>
#include <cstring>
#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "tusb.h"
#include <cmath>
#include "hardware/pio.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "pico/time.h"

#include "cpp/esp01_wrapper.hpp"
#include "cpp/mqtt_wrapper.hpp"
#include "cpp/tm1637_wrapper.hpp"

extern "C" {
#include "blink.pio.h"
#include "common/topics.h"
#include "common/config.h"
}

// UART defines
#define UART_ID uart1
#define BAUD_RATE 115200
#define UART_TX_PIN 4 //esp01 RX
#define UART_RX_PIN 5 //esp01 TX

static void blink_pin_forever(PIO pio, uint sm, uint offset, uint pin, uint freq) {
    blink_program_init(pio, sm, offset, pin);
    pio_sm_set_enabled(pio, sm, true);
    printf("Blinking pin %d at %d Hz\n", pin, freq);
    pio->txf[sm] = (125000000 / (2 * freq)) - 3;
}

// forward declaration of app-level C callback used below
static void app_mqtt_message_forwarder(const char *topic, const char *payload);

namespace app {
// forward declaration of app instance for C-style callback forwarding
static class App *g_app_instance = nullptr;

class App {
public:
    App() :
        display_(TM1637_DIO_PIN, TM1637_CLK_PIN),
        modem_(UART_ID) {
        // C MQTT config kept as POD alongside wrapper
        cfg_.host = MQTT_HOST;
        cfg_.port = MQTT_PORT;
        cfg_.client_id = MQTT_CLIENT_ID;
        cfg_.username = MQTT_USERNAME;
        cfg_.password = MQTT_PASSWORD;
        cfg_.keepalive = MQTT_KEEPALIVE;
        cfg_.clean_session = true;
    }

    // handle incoming MQTT messages on the app instance (called from C forwarder)
    void handle_mqtt_message(const char *topic, const char *payload) {
        // print to serial (debug monitor)
        printf("APP MQTT CB: topic=%s payload=%s\n", topic ? topic : "", payload ? payload : "");

        if (!topic || !payload) return;

        // For sensor topics, display a short form of payload on TM1637
        // We'll match against known TOPIC_SUB_x macros
        const char *subs[] = { TOPIC_SUB_1, TOPIC_SUB_2, TOPIC_SUB_3, TOPIC_SUB_4,
                               TOPIC_SUB_5, TOPIC_SUB_6, TOPIC_SUB_7, TOPIC_SUB_8 };

        for (int i = 0; i < 8; ++i) {
            if (strcmp(topic, subs[i]) == 0) {
                // Format payload into 4 chars. If numeric, show integer or with one decimal
                char disp[5] = {' ', ' ', ' ', ' ', '\0'};
                // Try to parse as float
                float v = 0.0f;
                int parsed = sscanf(payload, "%f", &v);
                if (parsed == 1) {
                    // For temperature we show 2-digit or X.X
                    // We'll display rounded integer if magnitude < 100
                    int iv = (int)std::round(v);
                    if (iv >= -9 && iv <= 99) {
                        snprintf(disp, sizeof(disp), "%3d", iv);
                    } else {
                        // fallback: copy first 3 chars
                        strncpy(disp, payload, 3);
                        disp[3] = '\0';
                    }
                } else {
                    // non-numeric: copy up to 4 chars
                    strncpy(disp, payload, 4);
                    disp[4] = '\0';
                }
                // Ensure null-termination and show
                disp[3] = disp[3] ? disp[3] : ' ';
                display_.show(disp);
                return;
            }
        }
        // If topic doesn't match sensor subs, optionally print but do not change display
    }

    void init() {
        stdio_init_all();
        // Wait briefly for the host to open the CDC serial port so early
        // boot messages are less likely to be missed. Fall back after 2s.
        const uint32_t usb_wait_ms = 2000;
        uint32_t usb_start = to_ms_since_boot(get_absolute_time());
        bool cdc_connected = false;
        while (to_ms_since_boot(get_absolute_time()) - usb_start < usb_wait_ms) {
            if (tud_cdc_connected()) { cdc_connected = true; break; }
            sleep_ms(50);
        }
        if (cdc_connected) printf("BOOT: stdio connected\n");
        else printf("BOOT: stdio_init_all() complete (no host)\n");
    // Reset ESP-01 (active low) to ensure clean boot when sharing power
    // Safer sequence: write de-asserted level first, then enable output to avoid glitches
    gpio_init(ESP01_RESET_PIN);
    // de-assert (HIGH) before switching to output
    gpio_put(ESP01_RESET_PIN, 1);
    gpio_set_dir(ESP01_RESET_PIN, GPIO_OUT);
        // small settle delay
        sleep_ms(10);
        // assert reset (LOW) briefly then release
    gpio_put(ESP01_RESET_PIN, 0);
        sleep_ms(100);
    gpio_put(ESP01_RESET_PIN, 1);
    // wait for module to boot (increase to be more tolerant of slow ESP boot)
    sleep_ms(800);
        // Blink demo
        PIO pio = pio0;
        uint offset = pio_add_program(pio, &blink_program);
        printf("Loaded program at %d\n for pio_add_program", offset);
#ifdef PICO_DEFAULT_LED_PIN
        blink_pin_forever(pio, 0, offset, PICO_DEFAULT_LED_PIN, 3);
#else
        blink_pin_forever(pio, 0, offset, 6, 3);
#endif
        // UART for ESP-01
        uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

        display_.brightness(3);
        display_.show("BOOT");

        if (!modem_.init()) {
            printf("ESP01 init failed\n");
        }
        // register global transport for mqtt client code
        modem_.register_global();

        // Ensure Wi-Fi is connected: attempt join until AT+CIFSR returns an IP
        char ipbuf[64] = {0};
        std::string ipstr;
        int retry = 0;
        while (true) {
            if (modem_.getIP(ipstr)) {
                strncpy(ipbuf, ipstr.c_str(), sizeof(ipbuf)-1);
                printf("Got IP: %s\n", ipbuf);
                display_.show("IPOK");
                break;
            }
            printf("WiFi not up yet, attempting join to %s (attempt %d)\n", WIFI_SSID, retry+1);
            if (!modem_.joinWifi(WIFI_SSID, WIFI_PASSWORD)) {
                printf("joinWifi failed\n");
            } else {
                printf("joinWifi returned, waiting for IP...\n");
            }
            // exponential backoff up to 16s
            int backoff = 2000 * (1 << (retry > 4 ? 4 : retry));
            if (backoff > 16000) backoff = 16000;
            sleep_ms(backoff);
            retry++;
        }

        // Build client_id with persistent storage in this App instance
        uint32_t r = (uint32_t) (to_ms_since_boot(get_absolute_time()) ^ (uint32_t)(uintptr_t)&r);
        char cidbuf[256];
        snprintf(cidbuf, sizeof(cidbuf), "%s-%08x", MQTT_CLIENT_ID, (unsigned)r);
        client_id_str_ = std::string(cidbuf);
        cfg_.client_id = client_id_str_.c_str();

        // Prepare MQTT wrapper
        mqtt_ = std::make_unique<net::MqttClient>(cfg_);

        // Attempt MQTT connect with exponential backoff, update display
        int mretry = 0;
        while (true) {
            printf("Attempting MQTT connect (attempt %d)\n", mretry+1);
            display_.show("MQTT");
            if (mqtt_->connect()) {
                printf("MQTT connected\n");
                display_.show("OK  ");
                    // Subscribe to configured topics
                    mqtt_->subscribe(TOPIC_SUB_1);
                    mqtt_->subscribe(TOPIC_SUB_2);
                    mqtt_->subscribe(TOPIC_SUB_3);
                    mqtt_->subscribe(TOPIC_SUB_4);
                    mqtt_->subscribe(TOPIC_SUB_5);
                    mqtt_->subscribe(TOPIC_SUB_6);
                    mqtt_->subscribe(TOPIC_SUB_7);
                    mqtt_->subscribe(TOPIC_SUB_8);
                    mqtt_->subscribe(TOPIC_SUB_9);
                    mqtt_->subscribe(TOPIC_SUB_10);
            // register app-level callback to receive messages
        // Set global instance pointer so the C forwarder can dispatch to this object
        g_app_instance = this;
        mqtt_->set_callback(app_mqtt_message_forwarder);
                    break;
            }
            printf("MQTT connect failed\n");
            int backoff = 1000 * (1 << (mretry > 6 ? 6 : mretry));
            if (backoff > 32000) backoff = 32000;
            sleep_ms(backoff);
            mretry++;
        }
    }

    void run() {
        // Simplified main loop: drop any UART bytes coming from ESP, run MQTT loop,
        // and update display. The interactive serial-forwarding functionality was
        // intentionally removed as it's no longer needed.
        uart_inst_t* u = UART_ID;
        static uint32_t last = 0;
        while (true) {
            // drain UART input to avoid buffer growth
            while (uart_is_readable(u)) {
                int c = uart_getc(u);
                (void)c;
            }

            // run mqtt background processing if available
            if (mqtt_) mqtt_->loop();

            // heartbeat display update once per second
            uint32_t now = to_ms_since_boot(get_absolute_time());
            if (now - last > 1000) {
                display_.show("RUN ");
                last = now;
            }

            tight_loop_contents();
        }
    }

private:
    display::TM1637 display_;
    modem::Esp01 modem_;
    mqtt_config_t cfg_{};
    std::unique_ptr<net::MqttClient> mqtt_;
    std::string client_id_str_;
};
}

int main() {
    app::App app;
    // store global pointer for mqtt forwarder
    app::g_app_instance = &app;
    app.init();
    app.run();
    return 0;
}

// C-style forwarder implementation that dispatches to the App instance
static void app_mqtt_message_forwarder(const char *topic, const char *payload) {
    if (app::g_app_instance) app::g_app_instance->handle_mqtt_message(topic, payload);
}
