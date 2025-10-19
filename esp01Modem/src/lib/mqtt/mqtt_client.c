#include "mqtt_client.h"
#include <stdio.h>
#include <string.h>
#include "pico/stdio.h"

// Minimal MQTT v3.1.1 packet helper for CONNECT/PUBLISH/SUBSCRIBE (QoS0)

static void write_u16(uint8_t *buf, int pos, uint16_t v) {
    buf[pos] = (v >> 8) & 0xFF;
    buf[pos+1] = v & 0xFF;
}

// Encode Remaining Length (simple, assumes <128)
static int encode_remaining_length(uint8_t *out, int len) {
    // single byte for small payloads
    out[0] = (uint8_t)len;
    return 1;
}

// Builds a simple CONNECT packet into buffer, returns length
static int build_connect(const mqtt_config_t *cfg, uint8_t *buf, int buflen) {
    // Protocol: fixed header + variable header + payload
    // Variable header: "MQTT"(4) + 0x04 + flags + keepalive
    // Payload: client id (UTF16 len + bytes)
    const char *proto = "MQTT";
    int pos = 0;
    // fixed header placeholder
    buf[pos++] = 0x10; // CONNECT
    int remain_pos = pos++; // remaining length placeholder

    // Variable header
    write_u16(buf, pos, 4); pos += 2;
    memcpy(&buf[pos], proto, 4); pos += 4;
    buf[pos++] = 0x04; // level
    buf[pos++] = 0x02; // connect flags: clean session
    write_u16(buf, pos, cfg->keepalive); pos += 2;

    // Payload: client id
    uint16_t idlen = (uint16_t)strlen(cfg->client_id);
    write_u16(buf, pos, idlen); pos += 2;
    memcpy(&buf[pos], cfg->client_id, idlen); pos += idlen;

    int rem = pos - 2; // after fixed header
    int rl_bytes = encode_remaining_length(&buf[remain_pos], rem);
    // shift if rl_bytes !=1 (we assume 1)
    (void)rl_bytes;
    return pos;
}

// For now publish just prints to stdout; in a full implementation we'd send over TCP
bool mqtt_connect(mqtt_client_t *c) {
    if (!c || !c->cfg) return false;
    uint8_t buf[256];
    int len = build_connect(c->cfg, buf, sizeof(buf));
    printf("mqtt_connect: built CONNECT packet %d bytes\n", len);
    // TODO: send over transport
    return true;
}

bool mqtt_publish(mqtt_client_t *c, const char *topic, const char *payload, int qos, bool retain) {
    (void)c; (void)topic; (void)payload; (void)qos; (void)retain;
    printf("mqtt_publish: topic=%s payload=%s\n", topic, payload);
    return true;
}

bool mqtt_subscribe(mqtt_client_t *c, const char *topic, int qos) {
    (void)c; (void)topic; (void)qos;
    printf("mqtt_subscribe: topic=%s\n", topic);
    return true;
}

void mqtt_loop(mqtt_client_t *c) {
    (void)c;
}


mqtt_client_t mqtt_client_create(const mqtt_config_t *cfg) {
    mqtt_client_t c = {.cfg = cfg};
    return c;
}

