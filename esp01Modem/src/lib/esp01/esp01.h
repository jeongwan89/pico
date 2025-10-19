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

#ifdef __cplusplus
}
#endif
