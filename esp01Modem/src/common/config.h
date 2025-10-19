#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// User configuration for WiFi (ESP-01 AT modem) and MQTT

// GPIO pins for TM1637 4-digit display
#ifndef TM1637_DIO_PIN
#define TM1637_DIO_PIN 2
#endif

#ifndef TM1637_CLK_PIN
#define TM1637_CLK_PIN 10
#endif

// ESP-01 reset pin on Pico (active low). User requested GPIO 3.
#ifndef ESP01_RESET_PIN
#define ESP01_RESET_PIN 3
#endif

// WiFi credentials (ESP-01)
#ifndef WIFI_SSID
#define WIFI_SSID "FarmMain5G" // set your SSID
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "wweerrtt" // set your password
#endif

// MQTT settings
#ifndef MQTT_HOST
#define MQTT_HOST "192.168.0.24" // e.g. 192.168.0.10 or mqtt.example.com
#endif

#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif

#ifndef MQTT_CLIENT_ID
#define MQTT_CLIENT_ID "pico-esp01-test-for-connect"
#endif

#ifndef MQTT_USERNAME
#define MQTT_USERNAME "farmmain"
#endif

#ifndef MQTT_PASSWORD
#define MQTT_PASSWORD "wweerrtt"
#endif

#ifndef MQTT_KEEPALIVE
#define MQTT_KEEPALIVE 60
#endif

#ifdef __cplusplus
}
#endif
