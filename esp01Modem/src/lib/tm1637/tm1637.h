#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t dio_pin;
    uint8_t clk_pin;
    uint8_t brightness;
} tm1637_t;

tm1637_t tm1637_create(uint8_t dio_pin, uint8_t clk_pin);
void tm1637_set_brightness(tm1637_t *d, uint8_t brightness);
void tm1637_show_string(tm1637_t *d, const char *text);

#ifdef __cplusplus
}
#endif
