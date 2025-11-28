#include "mqtt_client.h"
#include <stdio.h>
#include <string.h>
#include "pico/stdio.h"
#include "../esp01/esp01.h"

// Minimal MQTT v3.1.1 packet helper for CONNECT/PUBLISH/SUBSCRIBE (QoS0)

static void write_u16(uint8_t *buf, int pos, uint16_t v) {
    buf[pos] = (v >> 8) & 0xFF;
    buf[pos+1] = v & 0xFF;
}

// Encode Remaining Length (simple, assumes <128)
// Encode Remaining Length (variable bytes up to 4 bytes as per MQTT)
static int encode_remaining_length(uint8_t *out, int len) {
    int idx = 0;
    do {
        uint8_t digit = len % 128;
        len /= 128;
        // if there are more digits to encode, set the top bit of this digit
        if (len > 0) digit |= 0x80;
        out[idx++] = digit;
    } while (len > 0 && idx < 4);
    return idx;
}

// Decode remaining length from buffer (return 1 if decoded, 0 if need more bytes)
static int decode_remaining_length_from_buffer(const uint8_t *buf, int buflen, int *value, int *bytes_consumed) {
    int multiplier = 1;
    int v = 0;
    int i = 0;
    uint8_t encoded;
    do {
        if (i >= buflen) return 0; // need more data
        encoded = buf[i++];
        v += (encoded & 127) * multiplier;
        multiplier *= 128;
        if (multiplier > 128*128*128*128) return 0; // malformed
    } while (encoded & 128);
    *value = v;
    *bytes_consumed = i;
    return 1;
}

// Builds a simple CONNECT packet into buffer, returns length
static int build_connect(const mqtt_config_t *cfg, uint8_t *buf, int buflen) {
    // Protocol: fixed header + variable header + payload
    // Variable header: "MQTT"(4) + 0x04 + flags + keepalive
    // Payload: client id (UTF16 len + bytes)
    const char *proto = "MQTT";
    // build variable header + payload into a temporary buffer first
    uint8_t vb[256];
    int vp = 0;
    // Variable header
    write_u16(vb, vp, 4); vp += 2;
    memcpy(&vb[vp], proto, 4); vp += 4;
    vb[vp++] = 0x04; // level
    vb[vp++] = 0x02; // connect flags: clean session
    write_u16(vb, vp, cfg->keepalive); vp += 2;

    // Payload: client id
    uint16_t idlen = (uint16_t)strlen(cfg->client_id);
    write_u16(vb, vp, idlen); vp += 2;
    memcpy(&vb[vp], cfg->client_id, idlen); vp += idlen;

    // Now assemble final packet: fixed header + remaining length + vb
    int pos = 0;
    buf[pos++] = 0x10; // CONNECT
    uint8_t rl[4];
    int rlbytes = encode_remaining_length(rl, vp);
    // copy remaining length
    for (int i = 0; i < rlbytes; ++i) buf[pos++] = rl[i];
    // copy variable header + payload
    if (pos + vp > buflen) return 0;
    memcpy(&buf[pos], vb, vp); pos += vp;
    return pos;
}

// For now publish just prints to stdout; in a full implementation we'd send over TCP
bool mqtt_connect(mqtt_client_t *c) {
    if (!c || !c->cfg) return false;
    uint8_t buf[256];
    int len = build_connect(c->cfg, buf, sizeof(buf));
    printf("mqtt_connect: built CONNECT packet %d bytes\n", len);
    // Attempt to use registered esp01 transport
    esp01_t *esp = NULL;
    if (c->transport) esp = (esp01_t *)c->transport;
    if (!esp) esp = esp01_get_global();
    if (!esp) {
        printf("mqtt_connect: no esp01 transport registered\n");
        return false;
    }
    if (!esp01_tcp_connect(esp, c->cfg->host, c->cfg->port, 5000)) {
        printf("mqtt_connect: TCP connect failed\n");
        return false;
    }
    if (!esp01_tcp_send(esp, buf, len, 5000)) {
        printf("mqtt_connect: send failed\n");
        esp01_tcp_close(esp, 2000);
        return false;
    }
    uint8_t rbuf[128];
    int r = esp01_tcp_read(esp, rbuf, sizeof(rbuf), 5000);
    if (r <= 0) {
        printf("mqtt_connect: no response after CONNECT\n");
        return false;
    }
    printf("mqtt_connect: got %d bytes from server\n", r);
    // Expect CONNACK (packet type 0x20). Remaining length should be 2
    if ((rbuf[0] & 0xF0) != 0x20) {
        printf("mqtt_connect: expected CONNACK but got 0x%02x\n", rbuf[0]);
        return false;
    }
    // remaining length decode (simple: assume single-byte RL)
    int rl = rbuf[1];
    if (rl < 2) {
        printf("mqtt_connect: CONNACK remaining length too small (%d)\n", rl);
        return false;
    }
    // session present flag is at rbuf[2], connect return code at rbuf[3]
    uint8_t return_code = rbuf[3];
    if (return_code != 0) {
        printf("mqtt_connect: CONNACK return code %d (connect failed)\n", return_code);
        return false;
    }
    printf("mqtt_connect: CONNACK ok, connected\n");
    return true;
}

bool mqtt_publish(mqtt_client_t *c, const char *topic, const char *payload, int qos, bool retain) {
    (void)c; (void)topic; (void)payload; (void)qos; (void)retain;
    printf("mqtt_publish: topic=%s payload=%s\n", topic, payload);
    return true;
}

bool mqtt_subscribe(mqtt_client_t *c, const char *topic, int qos) {
    if (!c || !c->cfg || !topic) return false;
    esp01_t *esp = NULL;
    if (c->transport) esp = (esp01_t *)c->transport;
    if (!esp) esp = esp01_get_global();
    if (!esp) return false;

    // Build SUBSCRIBE packet (simple, packet id = 1)
    static uint16_t pktid = 1;
    uint8_t payload[512];
    int p = 0;
    // Variable header: packet id
    write_u16(payload, p, pktid); p += 2;
    // Payload: topic filter + requested QoS
    uint16_t tlen = (uint16_t)strlen(topic);
    write_u16(payload, p, tlen); p += 2;
    memcpy(&payload[p], topic, tlen); p += tlen;
    payload[p++] = (uint8_t)(qos & 0xFF);

    // Now assemble full packet with fixed header and remaining length
    uint8_t buf[600];
    int pos = 0;
    buf[pos++] = 0x82; // SUBSCRIBE
    uint8_t rl[4];
    int rlbytes = encode_remaining_length(rl, p);
    // copy remaining length bytes
    for (int i = 0; i < rlbytes; ++i) buf[pos++] = rl[i];
    // copy payload (variable header + payload)
    memcpy(&buf[pos], payload, p); pos += p;

    printf("mqtt_subscribe: sending SUBSCRIBE to topic=%s id=%d\n", topic, pktid);
    if (!esp01_tcp_send(esp, buf, pos, 5000)) {
        printf("mqtt_subscribe: send failed\n");
        return false;
    }
    // read SUBACK and validate
    uint8_t rbuf[128];
    int r = esp01_tcp_read(esp, rbuf, sizeof(rbuf), 5000);
    if (r <= 0) {
        printf("mqtt_subscribe: no response after SUBSCRIBE\n");
        return false;
    }
    printf("mqtt_subscribe: recv %d bytes\n", r);
    // Expect SUBACK (0x90)
    if ((rbuf[0] & 0xF0) != 0x90) {
        printf("mqtt_subscribe: expected SUBACK but got 0x%02x\n", rbuf[0]);
        return false;
    }
    // decode remaining length (assume 1-byte RL)
    int sub_rl = rbuf[1];
    if (sub_rl < 3) {
        printf("mqtt_subscribe: SUBACK too short (%d)\n", sub_rl);
        return false;
    }
    // packet id at rbuf[2..3], return codes start at rbuf[4]
    uint16_t ack_id = (rbuf[2] << 8) | rbuf[3];
    if (ack_id != pktid) {
        printf("mqtt_subscribe: SUBACK packet id mismatch (%d != %d)\n", ack_id, pktid);
        // continue but mark as failure
        pktid++;
        return false;
    }
    uint8_t granted = rbuf[4];
    if (granted == 0x80) {
        printf("mqtt_subscribe: subscription rejected (0x80)\n");
        pktid++;
        return false;
    }
    pktid++;
    return true;
}

void mqtt_loop(mqtt_client_t *c) {
    if (!c) return;
    esp01_t *esp = NULL;
    if (c->transport) esp = (esp01_t *)c->transport;
    if (!esp) esp = esp01_get_global();
    if (!esp) return;

    uint8_t tmp[512];
    int r = esp01_tcp_read(esp, tmp, sizeof(tmp), 100);
    if (r <= 0) return;
    // append to client rx buffer
    if (c->rxlen + r >= (int)sizeof(c->rxbuf)) {
        // overflow: drop buffer
        c->rxlen = 0;
    }
    memcpy(&c->rxbuf[c->rxlen], tmp, r);
    c->rxlen += r;

    // try to parse as many full MQTT packets as possible
    int idx = 0;
    while (idx < c->rxlen) {
        // need at least 2 bytes: fixed header + at least 1 remaining length byte
        if (idx + 2 > c->rxlen) break;
        uint8_t fh = c->rxbuf[idx];
        // decode remaining length
        int rl_value = 0, rl_bytes = 0;
        if (!decode_remaining_length_from_buffer(&c->rxbuf[idx+1], c->rxlen - (idx+1), &rl_value, &rl_bytes)) break;
        int header_len = 1 + rl_bytes;
        int total_len = header_len + rl_value;
        if (idx + total_len > c->rxlen) break; // wait for more data

        // full packet available at &c->rxbuf[idx]
        if ((fh & 0xF0) == 0x30) {
            // PUBLISH
            int pos = idx + header_len;
            // topic length
            if (pos + 2 > idx + total_len) goto skip_packet;
            uint16_t tlen = (c->rxbuf[pos] << 8) | c->rxbuf[pos+1]; pos += 2;
            if (pos + tlen > idx + total_len) goto skip_packet;
            char topic[256];
            int tcopy = tlen < (int)sizeof(topic)-1 ? tlen : (int)sizeof(topic)-1;
            memcpy(topic, &c->rxbuf[pos], tcopy); topic[tcopy] = '\0'; pos += tlen;
            // QoS handling: if QoS>0 there would be packet id here; assume QoS0 for now
            int paylen = idx + total_len - pos;
            char payload[512];
            int pcopy = paylen < (int)sizeof(payload)-1 ? paylen : (int)sizeof(payload)-1;
            memcpy(payload, &c->rxbuf[pos], pcopy); payload[pcopy] = '\0';
            // print and callback
            printf("MQTT PUBLISH -> topic='%s' payload='%s'\n", topic, payload);
            if (c->on_message) c->on_message(topic, payload);
        }
skip_packet:
        idx += total_len;
    }

    // remove processed bytes
    if (idx > 0) {
        if (idx < c->rxlen) memmove(c->rxbuf, &c->rxbuf[idx], c->rxlen - idx);
        c->rxlen -= idx;
    }
}


mqtt_client_t mqtt_client_create(const mqtt_config_t *cfg) {
    mqtt_client_t c = {.cfg = cfg};
    return c;
}

void mqtt_set_transport(mqtt_client_t *c, void *transport) {
    if (!c) return;
    c->transport = transport;
}

void mqtt_set_message_callback(mqtt_client_t *c, void (*cb)(const char *, const char *)) {
    if (!c) return;
    c->on_message = cb;
}

