#pragma once

#include <stdint.h>
#include <stdbool.h>

// Lightweight AT→MQTT wrapper API
// Provides: wifi connect, mqtt connect, subscribe, publish (basic) and a
// callback for incoming messages.

typedef void (*atmqtt_msg_cb_t)(const char *topic, const char *payload);

// Initialize AT MQTT module (uses ESP01_UART defined in Esp01.h)
void atmqtt_init();

// Simple blocking helper: send AT command and wait for OK or ERROR (timeout ms)
bool atmqtt_send_wait_ok(const char *cmd, uint32_t timeout_ms);

// High level operations
bool atmqtt_wifi_connect(const char *ssid, const char *password, uint32_t timeout_ms);
bool atmqtt_mqtt_usercfg(int id, const char *clientid, const char *username, const char *password);
bool atmqtt_mqtt_connect(int id, const char *host, int port);
bool atmqtt_mqtt_subscribe(int id, const char *topic, int qos);
bool atmqtt_mqtt_publish(int id, const char *topic, const char *payload, int qos, int retain);

// Process incoming UART data and call message callback when a publish arrives
void atmqtt_process_io();

// Set callback for incoming messages
void atmqtt_set_msg_callback(atmqtt_msg_cb_t cb);

// Simple connection status helpers
void atmqtt_set_connected(bool connected);
bool atmqtt_is_connected();
