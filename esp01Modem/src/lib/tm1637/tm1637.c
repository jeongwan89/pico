#include "tm1637.h"
#include <stdio.h>
#include "hardware/gpio.h"
#include "pico/stdlib.h"

// 4-digit segment map for 0-9, A-F minimal
static const uint8_t segmap[] = {
    0x3f, //0
    0x06, //1
    0x5b, //2
    0x4f, //3
    0x66, //4
    0x6d, //5
    0x7d, //6
    0x07, //7
    0x7f, //8
    0x6f, //9
    0x77, //A
    0x7c, //b
    0x58, //C
    0x5e, //d
    0x79, //E
    0x71  //F
};

static void tm1637_delay() {
    sleep_us(5);
}

static void tm1637_clk_write(tm1637_t *d, int level) {
    gpio_put(d->clk_pin, level);
}

static void tm1637_dio_write(tm1637_t *d, int level) {
    gpio_put(d->dio_pin, level);
}

static void tm1637_start(tm1637_t *d) {
    tm1637_dio_write(d, 1);
    tm1637_clk_write(d, 1);
    tm1637_delay();
    tm1637_dio_write(d, 0);
}

static void tm1637_stop(tm1637_t *d) {
    tm1637_clk_write(d, 0);
    tm1637_dio_write(d, 0);
    tm1637_delay();
    tm1637_clk_write(d, 1);
    tm1637_dio_write(d, 1);
}

static void tm1637_write_byte(tm1637_t *d, uint8_t b) {
    for (int i = 0; i < 8; ++i) {
        tm1637_clk_write(d, 0);
        tm1637_delay();
        tm1637_dio_write(d, (b >> i) & 1);
        tm1637_delay();
        tm1637_clk_write(d, 1);
        tm1637_delay();
    }
    // ack - read
    tm1637_clk_write(d, 0);
    gpio_set_dir(d->dio_pin, GPIO_IN);
    tm1637_delay();
    tm1637_clk_write(d, 1);
    tm1637_delay();
    // ignore ack value
    gpio_set_dir(d->dio_pin, GPIO_OUT);
}


// Minimal stub implementation. Replace with real bit-banged protocol.

tm1637_t tm1637_create(uint8_t dio_pin, uint8_t clk_pin) {
    tm1637_t d = { .dio_pin = dio_pin, .clk_pin = clk_pin, .brightness = 0 };
    // configure pins
    gpio_init(dio_pin);
    gpio_init(clk_pin);
    gpio_set_dir(dio_pin, GPIO_OUT);
    gpio_set_dir(clk_pin, GPIO_OUT);
    gpio_put(dio_pin, 1);
    gpio_put(clk_pin, 1);
    return d;
}

void tm1637_set_brightness(tm1637_t *d, uint8_t brightness) {
    if (!d) return; d->brightness = brightness & 0x07; // 0..7
}

void tm1637_show_string(tm1637_t *d, const char *text) {
    if (!d || !text) return;
    // Only show up to 4 characters, map to 7-seg
    uint8_t digits[4] = {0,0,0,0};
    int i = 0;
    while (*text && i < 4) {
        char c = *text++;
        if (c >= '0' && c <= '9') digits[i++] = segmap[c - '0'];
        else if (c >= 'A' && c <= 'F') digits[i++] = segmap[10 + (c - 'A')];
        else if (c >= 'a' && c <= 'f') digits[i++] = segmap[10 + (c - 'a')];
        else digits[i++] = 0x00;
    }

    // send command 0x40 (auto-increment)
    tm1637_start(d);
    tm1637_write_byte(d, 0x40);
    tm1637_stop(d);

    // set address 0xC0
    tm1637_start(d);
    tm1637_write_byte(d, 0xC0);
    for (int j = 0; j < 4; ++j) tm1637_write_byte(d, digits[j]);
    tm1637_stop(d);

    // set brightness (display control)
    tm1637_start(d);
    tm1637_write_byte(d, 0x88 | (d->brightness & 0x07));
    tm1637_stop(d);
}
