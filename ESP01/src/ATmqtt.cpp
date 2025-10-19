#include "ATmqtt.h"
#include "Esp01.h"
#include "pico/stdio.h"
#include "hardware/uart.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static atmqtt_msg_cb_t g_msg_cb = NULL;
static bool g_connected = false;

// Debug macro: compile-time control via ATMQTT_DEBUG
#ifdef ATMQTT_DEBUG
#define ATM_DEBUG(fmt, ...) do { printf(fmt "\n", ##__VA_ARGS__); } while(0)
#else
#define ATM_DEBUG(fmt, ...) do { } while(0)
#endif

void atmqtt_init(){
    // ensure UART is configured (esp01_init should have been called)
    // nothing to do here for now
}

static bool read_line_timeout(char *buf, size_t len, uint32_t timeout_ms){
    uint32_t start = to_ms_since_boot(get_absolute_time());
    size_t idx = 0;
    while ((to_ms_since_boot(get_absolute_time()) - start) < timeout_ms){
        if (uart_is_readable(ESP01_UART)){
            int c = uart_getc(ESP01_UART);
            if (c == '\r') continue;
            if (c == '\n'){
                if (idx == 0) continue; // skip empty
                buf[idx] = '\0';
                // low-level RX log
                ATM_DEBUG("[ATRX] %s", buf);
                return true;
            }
            if (idx + 1 < len) {
                buf[idx++] = (char)c;
            } else {
                // buffer full: terminate and return to avoid overflow
                buf[len-1] = '\0';
                printf("[ATRX] <truncated> %s\n", buf);
                return true;
            }
        } else {
            sleep_ms(1);
        }
    }
    if (idx > 0){ buf[idx] = '\0'; return true; }
    return false;
}

bool atmqtt_send_wait_ok(const char *cmd, uint32_t timeout_ms){
    // send with CRLF
    uart_puts(ESP01_UART, cmd);
    uart_puts(ESP01_UART, "\r\n");
    // low-level TX log
    ATM_DEBUG("[ATTX] %s", cmd);
    char line[256];
    uint32_t start = to_ms_since_boot(get_absolute_time());
    while ((to_ms_since_boot(get_absolute_time()) - start) < timeout_ms){
        if (!read_line_timeout(line, sizeof(line), 200)) continue;
        if (strcmp(line, "OK") == 0) return true;
        if (strncmp(line, "ERROR", 5) == 0) {
            // mark disconnected on errors
            g_connected = false;
            return false;
        }
        // ignore other lines
    }
    return false;
}

bool atmqtt_wifi_connect(const char *ssid, const char *password, uint32_t timeout_ms){
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, password);
    return atmqtt_send_wait_ok(cmd, timeout_ms);
}

bool atmqtt_mqtt_usercfg(int id, const char *clientid, const char *username, const char *password){
    // AT+MQTTUSERCFG=<id>,<clean_session>,<client_id>,<username>,<password>,<keepalive>,<auto_connect>,<will_topic>,<will_msg>
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "AT+MQTTUSERCFG=%d,1,\"%s\",\"%s\",\"%s\",120,0,\"\",\"\"", id, clientid, username, password);
    return atmqtt_send_wait_ok(cmd, 5000);
}

bool atmqtt_mqtt_connect(int id, const char *host, int port){
    // AT+MQTTCONN=<id>,<host>,<port>,<keepalive>
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "AT+MQTTCONN=%d,\"%s\",%d,60", id, host, port);
    bool ok = atmqtt_send_wait_ok(cmd, 5000);
    if (ok) g_connected = true;
    return ok;
}

bool atmqtt_mqtt_subscribe(int id, const char *topic, int qos){
    // AT+MQTTSUB=<id>,<topic>,<qos>
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "AT+MQTTSUB=%d,\"%s\",%d", id, topic, qos);
    return atmqtt_send_wait_ok(cmd, 5000);
}

bool atmqtt_mqtt_publish(int id, const char *topic, const char *payload, int qos, int retain){
    // AT+MQTTPUB=<id>,<topic>,<qos>,<retain>,<len>[,timeout]
    // Many esp-at variants use: AT+MQTTPUB=<id>,<topic>,<payload>,<qos>,<retain>
    // We'll attempt a common variant: AT+MQTTPUB=<id>,"topic","payload",<qos>,<retain>
    char cmd[1024];
    // escape quotes in payload? keep simple
    snprintf(cmd, sizeof(cmd), "AT+MQTTPUB=%d,\"%s\",\"%s\",%d,%d", id, topic, payload, qos, retain);
    return atmqtt_send_wait_ok(cmd, 5000);
}

void atmqtt_set_connected(bool connected){ g_connected = connected; }
bool atmqtt_is_connected(){ return g_connected; }

void atmqtt_set_msg_callback(atmqtt_msg_cb_t cb){
    g_msg_cb = cb;
}

// Parse incoming unsolicited lines to detect publish messages.
// Expected patterns vary; we'll look for +MQTTPUB or +MQTTSUBRECV or similar.
void atmqtt_process_io(){
    char line[512];
    while (read_line_timeout(line, sizeof(line), 10)){
    // debug print of unsolicited line (already logged in read_line_timeout)
        // simple disconnect detection: if device reports CLOSED or DISCONNECT or ERROR, mark disconnected
        if (strstr(line, "CLOSED") || strstr(line, "DISCONNECT") || strstr(line, "MQTTDISCONN") ){
            g_connected = false;
            continue;
        }
        if (strncmp(line, "ERROR", 5) == 0){
            g_connected = false;
            continue;
        }
        // handle several possible unsolicited formats
        if (strstr(line, "+MQTTPUB:") || strstr(line, "+MQTTSUBRECV:") || strstr(line, "+MQTTSUB:") || strstr(line, "+MQTTMSG:") || strstr(line, "+MQTTPUBLISH:")){
            // Try several possible formats. We'll look for quoted topic and then quoted payload.
            // Examples seen in the wild:
            // +MQTTPUB: <id>,"topic","payload"
            // +MQTTSUBRECV: <id>,"topic","payload"
            // +MQTTMSG: "topic",<len>,<payload>
            const char *p1 = strchr(line, '"');
            if (!p1){
                // sometimes topic is unquoted after a comma
                char *comma1 = strchr(line, ',');
                if (!comma1) { printf("[ATPARSE] unhandled format: %s\n", line); continue; }
                // skip comma and whitespace
                char *tstart = comma1 + 1; while (*tstart == ' ' || *tstart == '\t') tstart++;
                // topic until next comma
                char *tend = strchr(tstart, ',');
                if (!tend) { printf("[ATPARSE] unhandled format: %s\n", line); continue; }
                *tend = '\0';
                const char *topic = tstart;
                const char *payload = tend + 1;
                if (g_msg_cb) g_msg_cb(topic, payload);
                continue;
            }
            char *q1 = strchr((char*)p1+1, '"');
            if (!q1) { printf("[ATPARSE] malformed topic in: %s\n", line); continue; }
            *q1 = '\0';
            const char *topic = p1 + 1;
            // look for payload as next quoted string
            char *p2 = strchr(q1+1, '"');
            if (p2){
                char *q2 = strchr(p2+1, '"');
                if (!q2) { printf("[ATPARSE] malformed payload in: %s\n", line); continue; }
                *q2 = '\0';
                const char *payload = p2 + 1;
                if (g_msg_cb) g_msg_cb(topic, payload);
                continue;
            }
            // fallback: payload may be after a comma or numeric length; take remainder
            char *comma = strchr(q1+1, ',');
            if (comma){
                const char *payload = comma + 1;
                if (g_msg_cb) g_msg_cb(topic, payload);
                continue;
            }
            printf("[ATPARSE] could not extract payload from: %s\n", line);
        }
        // ignore other lines
    }
}
