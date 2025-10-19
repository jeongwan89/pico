#include <cstdio>
#include <memory>
#include <cstring>
#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "hardware/pio.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"

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

        // Prepare MQTT wrapper
        mqtt_ = std::make_unique<net::MqttClient>(cfg_);
    }

    void run() {
        // Line-based forwarding: collect a line from stdin, on Enter send line + CRLF to UART
        uart_inst_t* u = UART_ID;
        char linebuf[256];
        int linelen = 0;
        int cur = 0; // cursor position within line
        static uint32_t last = 0;

        auto redraw = [&](void) {
            // redraw the whole input line and position cursor at 'cur'
            putchar('\r');
            // print buffer
            for (int i = 0; i < linelen; ++i) putchar(linebuf[i]);
            // clear one extra char to erase leftovers
            putchar(' ');
            // return carriage and move cursor to current position
            putchar('\r');
            for (int i = 0; i < cur; ++i) putchar(linebuf[i]);
        };

        while (true) {
            int ch = getchar_timeout_us(0);
            if (ch != PICO_ERROR_TIMEOUT) {
                if (ch == 27) {
                    // possible escape sequence for arrow keys
                    int c1 = getchar_timeout_us(1000);
                    if (c1 == '[') {
                        int c2 = getchar_timeout_us(1000);
                        if (c2 == 'D') {
                            // left
                            if (cur > 0) { cur--; redraw(); }
                        } else if (c2 == 'C') {
                            // right
                            if (cur < linelen) { cur++; redraw(); }
                        }
                        // else ignore other sequences
                    }
                } else if (ch == '\r' || ch == '\n') {
                    // Send buffered line to UART with CRLF
                    for (int i = 0; i < linelen; ++i) uart_putc_raw(u, linebuf[i]);
                    uart_putc_raw(u, '\r');
                    uart_putc_raw(u, '\n');
                    // Echo newline on host and reset buffer
                    putchar('\r'); putchar('\n');
                    linelen = 0; cur = 0;
                } else if (ch == 8 || ch == 127) {
                    // Backspace: remove char before cursor
                    if (cur > 0) {
                        memmove(&linebuf[cur-1], &linebuf[cur], linelen - cur);
                        linelen--; cur--;
                        redraw();
                    }
                } else if (ch >= 32 && ch < 127) {
                    // printable char: insert at cursor
                    if (linelen < (int)sizeof(linebuf) - 1) {
                        memmove(&linebuf[cur+1], &linebuf[cur], linelen - cur);
                        linebuf[cur] = (char)ch;
                        linelen++; cur++;
                        redraw();
                    }
                }
            }

            // read from UART and print to stdout
            while (uart_is_readable(u)) {
                int c = uart_getc(u);
                if (c >= 0) putchar(c);
            }

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
};
}

int main() {
    app::App app;
    app.init();
    app.run();
    return 0;
}
