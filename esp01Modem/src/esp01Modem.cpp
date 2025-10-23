#include <cstdio>
#include <memory>
#include <cstring>
#include "pico/stdlib.h"
#include "pico/stdio.h"
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

namespace app {
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

    void init() {
        stdio_init_all();
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
    // wait for module to boot
    sleep_ms(200);
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
    app.init();
    app.run();
    return 0;
}
