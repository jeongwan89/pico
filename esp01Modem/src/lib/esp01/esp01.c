#include "esp01.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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
    // Disable echo: send ATE0, wait, then send again if necessary and perform
    // a longer drain to ensure any echoed bytes are consumed before returning.
    esp01_send_cmd(m->uart, "ATE0");
    if (!esp01_expect_ok(m->uart, 1000)) {
        // retry once more before giving up
        esp01_send_cmd(m->uart, "ATE0");
        if (!esp01_expect_ok(m->uart, 1000)) {
            printf("esp01_init: failed to disable echo\n");
        }
    }

    // Drain residual bytes robustly: read and discard until we see no bytes for
    // a short quiet period, or until overall timeout. This handles fragmented
    // echoes that can arrive after the OK reply.
    uint64_t overall_deadline = time_us_64() + 800 * 1000ULL; // total 800ms max
    uint64_t quiet_deadline = 0;
    while (time_us_64() < overall_deadline) {
        if (uart_is_readable(m->uart)) {
            int c = uart_getc(m->uart);
            (void)c;
            // push quiet_deadline forward when we see data
            quiet_deadline = time_us_64() + 80 * 1000ULL; // require 80ms quiet
        } else {
            // if we've had a short quiet period, assume no more residual bytes
            if (quiet_deadline != 0 && time_us_64() > quiet_deadline) break;
            sleep_us(500);
        }
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

// --- TCP transport helpers ---
// Send AT+CIPSTART="TCP","host",port
bool esp01_tcp_connect(esp01_t *m, const char *host, int port, uint32_t timeout_ms) {
    if (!m || !m->uart || !host) return false;
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%d", host, port);
    esp01_send_cmd(m->uart, cmd);
    return esp01_expect_ok(m->uart, timeout_ms);
}

// Send raw bytes using AT+CIPSEND length then data
bool esp01_tcp_send(esp01_t *m, const uint8_t *data, int len, uint32_t timeout_ms) {
    if (!m || !m->uart || !data || len <= 0) return false;
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d", len);
    esp01_send_cmd(m->uart, cmd);
    // wait for '>' prompt
    char resp[128];
    int r = esp01_read_response(m->uart, resp, sizeof(resp), 1000);
    if (r <= 0 || strchr(resp, '>') == NULL) {
        return false;
    }
    // write raw data
    for (int i = 0; i < len; ++i) uart_putc_raw(m->uart, data[i]);
    // after send, wait for SEND OK
    return esp01_expect_ok(m->uart, timeout_ms);
}

int esp01_tcp_read(esp01_t *m, uint8_t *buf, int maxlen, uint32_t timeout_ms) {
    if (!m || !m->uart || !buf || maxlen <= 0) return -1;
    // The ESP sends +IPD,len:payload. We'll read until timeout and try to
    // extract payload occurrences. This is a simple parser for small payloads.
    char tmp[1024];
    int rd = esp01_read_response(m->uart, tmp, sizeof(tmp), timeout_ms);
    if (rd <= 0) return 0;
    // search for +IPD,
    char *p = strstr(tmp, "+IPD,");
    if (!p) {
        // maybe raw data without prefix
        int copy = rd < maxlen ? rd : maxlen;
        memcpy(buf, tmp, copy);
        return copy;
    }
    // parse length
    p += 5;
    int len = atoi(p);
    // find ':' after length
    char *col = strchr(p, ':');
    if (!col) return 0;
    char *payload = col + 1;
    int avail = rd - (payload - tmp);
    int copy = avail < len ? avail : len;
    if (copy > maxlen) copy = maxlen;
    memcpy(buf, payload, copy);
    return copy;
}

bool esp01_tcp_close(esp01_t *m, uint32_t timeout_ms) {
    if (!m || !m->uart) return false;
    esp01_send_cmd(m->uart, "AT+CIPCLOSE");
    return esp01_expect_ok(m->uart, timeout_ms);
}

// global registration
static esp01_t* g_esp = NULL;
void esp01_register_global(esp01_t *m) {
    g_esp = m;
}
esp01_t* esp01_get_global(void) {
    return g_esp;
}

bool esp01_get_ip(esp01_t *m, char *out, int maxlen, uint32_t timeout_ms) {
    if (!m || !m->uart || !out || maxlen <= 0) return false;
    esp01_send_cmd(m->uart, "AT+CIFSR");
    char buf[256];
    int n = esp01_read_response(m->uart, buf, sizeof(buf), timeout_ms);
    if (n <= 0) return false;
    // Look for an IPv4-like sequence
    char *p = strstr(buf, "192.");
    if (!p) p = strstr(buf, "10.");
    if (!p) p = strstr(buf, "172.");
    if (!p) return false;
    // copy until non-printable or newline
    int i = 0;
    while (i < maxlen - 1 && p[i] && p[i] != '\r' && p[i] != '\n') {
        out[i] = p[i]; i++;
    }
    out[i] = '\0';
    return true;
}
