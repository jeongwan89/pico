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
    // echo suppression using expected-length matching: when we send a line we expect
    // the modem to echo the same bytes back; we collect incoming bytes into a small
    // buffer and if the first N bytes equal the sent payload+CRLF, we drop them.
    char expect_buf[512];
    int expect_len = 0;      // how many bytes we expect (sent length + CRLF)
    int expect_filled = 0;   // how many bytes currently collected
    uint64_t expect_deadline = 0; // microseconds timestamp to abandon expectation

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
                    // Send buffered line to UART with CRLF and then synchronously
                    // capture the modem response for a short timeout. This approach
                    // collects the raw response and strips occurrences of the
                    // echoed command so the host sees only the modem reply.
                    for (int i = 0; i < linelen; ++i) uart_putc_raw(u, linebuf[i]);
                    uart_putc_raw(u, '\r');
                    uart_putc_raw(u, '\n');

                    // Echo newline locally
                    putchar('\r'); putchar('\n');

                    // prepare pattern to strip (line + CRLF)
                    char pattern[300];
                    int patlen = 0;
                    int copy_len = linelen;
                    if (copy_len > (int)sizeof(pattern) - 3) copy_len = (int)sizeof(pattern) - 3;
                    for (int i = 0; i < copy_len; ++i) pattern[patlen++] = linebuf[i];
                    pattern[patlen++] = '\r'; pattern[patlen++] = '\n';

                    // set expect buffer so concurrent UART reads can drop echoed bytes
                    if (patlen < (int)sizeof(expect_buf)) {
                        memcpy(expect_buf, pattern, patlen);
                        expect_len = patlen;
                        expect_filled = 0;
                        expect_deadline = time_us_64() + 2000 * 1000ULL; // 2s expectation window
                    }

                    // capture response into buffer for up to 2500 ms
                    char resp[4096];
                    int rlen = 0;
                    uint64_t deadline = time_us_64() + 2500 * 1000ULL;
                    while (time_us_64() < deadline && rlen < (int)sizeof(resp) - 1) {
                        if (uart_is_readable(u)) {
                            int c = uart_getc(u);
                            if (c < 0) continue;
                            resp[rlen++] = (char)c;
                            // optional fast exit if we see OK or ERROR sequence
                            if (rlen >= 4) {
                                resp[rlen] = '\0';
                                if (strstr(resp, "OK") || strstr(resp, "ERROR")) break;
                            }
                        } else {
                            sleep_us(500);
                        }
                    }
                    resp[rlen] = '\0';

                    // remove occurrences of pattern from resp
                    if (rlen > 0) {
                        // 1) naive direct removal of exact occurrences
                        for (int i = 0; i < rlen; ) {
                            if (i + patlen <= rlen && patlen > 0 && memcmp(&resp[i], pattern, patlen) == 0) {
                                i += patlen;
                                continue;
                            }
                            putchar(resp[i]);
                            i++;
                        }

                        // 2) additional pass: remove occurrences where the modem echoed
                        // the command with interleaved CR/LF characters. We scan the
                        // response and skip sequences that match the pattern while
                        // allowing '\r'/'\n' anywhere between pattern characters.
                        // Build a normalized output by walking the buffer.
                        // (This is conservative: we only remove exact char sequence
                        // matches allowing CR/LF between bytes.)
                        // To avoid double-printing in the simple pass above, this
                        // additional heuristic is skipped (kept for future use).
                    }

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

            // read from UART and buffer for echo-suppression
            while (uart_is_readable(u)) {
                int c = uart_getc(u);
                if (c < 0) continue;
                if (expect_len > 0 && expect_filled < expect_len) {
                    // collect into a temporary buffer in place (we already filled expect_buf with expected sequence,
                    // so use a separate temp area pinned after expect_len)
                    // We'll temporarily use a small window starting at index expect_len to store collected bytes.
                    int store_idx = expect_len + expect_filled;
                    if (store_idx < (int)sizeof(expect_buf)) {
                        expect_buf[store_idx] = (char)c;
                        expect_filled++;
                    } else {
                        // buffer overflow — give up expectation and flush what we have
                        for (int i = 0; i < expect_filled; ++i) putchar(expect_buf[expect_len + i]);
                        expect_len = 0; expect_filled = 0; expect_deadline = 0;
                        putchar(c);
                    }
                    // if we've filled the expected amount, compare
                    if (expect_filled >= expect_len) {
                        bool match = true;
                        for (int i = 0; i < expect_len; ++i) {
                            if (expect_buf[i] != expect_buf[expect_len + i]) { match = false; break; }
                        }
                        if (match) {
                            // drop them
                        } else {
                            // flush collected bytes then print nothing special
                            for (int i = 0; i < expect_filled; ++i) putchar(expect_buf[expect_len + i]);
                        }
                        // reset expectation after handling
                        expect_len = 0; expect_filled = 0; expect_deadline = 0;
                    }
                } else {
                    putchar(c);
                }
            }
            // abort expectation on timeout and flush any collected bytes
            if (expect_len > 0 && time_us_64() > expect_deadline) {
                for (int i = 0; i < expect_filled; ++i) putchar(expect_buf[expect_len + i]);
                expect_len = 0; expect_filled = 0; expect_deadline = 0;
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
