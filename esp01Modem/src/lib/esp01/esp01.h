#pragma once
#include <stdbool.h>
#include "hardware/uart.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uart_inst_t *uart;
} esp01_t;

typedef struct {
    const char *ssid;
    const char *password;
} wifi_config_t;

esp01_t esp01_create(uart_inst_t *uart);
bool esp01_init(esp01_t *m);
bool esp01_join_wifi(esp01_t *m, const wifi_config_t *cfg);

// TCP transport over ESP-01 AT commands
bool esp01_tcp_connect(esp01_t *m, const char *host, int port, uint32_t timeout_ms);
bool esp01_tcp_send(esp01_t *m, const uint8_t *data, int len, uint32_t timeout_ms);
int  esp01_tcp_read(esp01_t *m, uint8_t *buf, int maxlen, uint32_t timeout_ms);
bool esp01_tcp_close(esp01_t *m, uint32_t timeout_ms);

// Global registration (convenience for higher-level modules)
void esp01_register_global(esp01_t *m);
esp01_t* esp01_get_global(void);

// Query IP address: fills out buffer with result (e.g. "192.168.x.y") and
// returns true if an IP was found.
bool esp01_get_ip(esp01_t *m, char *out, int maxlen, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
