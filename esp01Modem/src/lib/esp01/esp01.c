#include "esp01.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>
#include "pico/time.h"

esp01_t esp01_create(uart_inst_t *uart) {
    esp01_t m = { .uart = uart };
    return m;
}
// send raw string (without CRLF)
static void esp01_write(uart_inst_t *u, const char *s) {
    while (*s) {
        uart_putc_raw(u, *s++);
    }
}

// send command with CRLF
static void esp01_send_cmd(uart_inst_t *u, const char *cmd) {
    esp01_write(u, cmd);
    esp01_write(u, "\r\n");
}

// read available data into buffer up to maxlen or timeout_ms, returns bytes read
static int esp01_read_response(uart_inst_t *u, char *buf, int maxlen, uint32_t timeout_ms) {
    int idx = 0;
    uint64_t deadline = time_us_64() + (uint64_t)timeout_ms * 1000ULL;
    while (time_us_64() < deadline && idx < maxlen - 1) {
        if (uart_is_readable(u)) {
            int c = uart_getc(u);
            if (c < 0) continue;
            buf[idx++] = (char)c;
            // fast exit if we see OK or ERROR
            if (idx >= 2) {
                buf[idx] = '\0';
                if (strstr(buf, "OK") || strstr(buf, "ERROR")) break;
            }
        } else {
            sleep_us(500);
        }
    }
    buf[idx] = '\0';
    return idx;
}

static bool esp01_expect_ok(uart_inst_t *u, uint32_t timeout_ms) {
    char resp[256];
    esp01_read_response(u, resp, sizeof(resp), timeout_ms);
    // printf("ESP resp: %s\n", resp);
    return strstr(resp, "OK") != NULL;
}

bool esp01_init(esp01_t *m) {
    if (!m || !m->uart) return false;
    // simple AT check
    esp01_send_cmd(m->uart, "AT");
    if (!esp01_expect_ok(m->uart, 1000)) {
        printf("esp01_init: AT no response\n");
        // still continue to try disabling echo
    }
    // Disable echo
    esp01_send_cmd(m->uart, "ATE0");
    if (!esp01_expect_ok(m->uart, 1000)) {
        printf("esp01_init: failed to disable echo\n");
        // not fatal
    }
    // Return true even if limited; full connectivity depends on WiFi join
    return true;
}

bool esp01_join_wifi(esp01_t *m, const wifi_config_t *cfg) {
    if (!m || !m->uart || !cfg) return false;
    char cmd[256];
    // Format: AT+CWJAP="ssid","pwd"
    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", cfg->ssid, cfg->password);
    esp01_send_cmd(m->uart, cmd);
    if (!esp01_expect_ok(m->uart, 20000)) {
        printf("esp01_join_wifi: join failed or timeout\n");
        return false;
    }
    return true;
}
