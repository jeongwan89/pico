#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *host;
    uint16_t port;
    const char *client_id;
    const char *username;
    const char *password;
    uint16_t keepalive;
    bool clean_session;
} mqtt_config_t;

typedef struct {
    const mqtt_config_t *cfg;
} mqtt_client_t;

// Attach transport
void mqtt_set_transport(mqtt_client_t *c, void *transport); // transport is esp01_t*

mqtt_client_t mqtt_client_create(const mqtt_config_t *cfg);
bool mqtt_connect(mqtt_client_t *c);
bool mqtt_publish(mqtt_client_t *c, const char *topic, const char *payload, int qos, bool retain);
bool mqtt_subscribe(mqtt_client_t *c, const char *topic, int qos);
void mqtt_loop(mqtt_client_t *c);

#ifdef __cplusplus
}
#endif
