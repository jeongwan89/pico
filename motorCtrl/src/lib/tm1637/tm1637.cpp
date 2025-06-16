#include "tm1637.h"

#define BLANK_SEGMENT 0x00
//
//      A
//     ---
//  F |   | B
//     -G-
//  E |   | C
//     ---
//      D

static const uint8_t digitToSegment[] = {
    0x3f, // 0
    0x06, // 1
    0x5b, // 2
    0x4f, // 3
    0x66, // 4
    0x6d, // 5
    0x7d, // 6
    0x07, // 7
    0x7f, // 8
    0x6f, // 9
    0b01110111,    // A
    0b01111100,    // b
    0b00111001,    // C
    0b01011110,    // d
    0b01111001,    // E
    0b01110001     // F
};

TM1637::TM1637(uint8_t clk_pin, uint8_t dio_pin)
    : clk(clk_pin), dio(dio_pin), brightness(0x0f) {}

void TM1637::init() {
    gpio_init(clk);
    gpio_init(dio);
    gpio_set_dir(clk, GPIO_OUT);
    gpio_set_dir(dio, GPIO_OUT);
    gpio_put(clk, 1);
    gpio_put(dio, 1);
}

void TM1637::delay() {
    sleep_us(5);
}

void TM1637::start() {
    gpio_put(dio, 1);
    gpio_put(clk, 1);
    delay();
    gpio_put(dio, 0);
    delay();
    gpio_put(clk, 0);
    delay();
}

void TM1637::stop() {
    gpio_put(clk, 0);
    delay();
    gpio_put(dio, 0);
    delay();
    gpio_put(clk, 1);
    delay();
    gpio_put(dio, 1);
    delay();
}

void TM1637::write_byte(uint8_t b) {
    for (int i = 0; i < 8; i++) {
        gpio_put(clk, 0);
        delay();
        gpio_put(dio, (b >> i) & 0x01);
        delay();
        gpio_put(clk, 1);
        delay();
    }
    // ACK
    gpio_put(clk, 0);
    gpio_set_dir(dio, GPIO_IN);
    delay();
    gpio_put(clk, 1);
    delay();
    gpio_set_dir(dio, GPIO_OUT);
    gpio_put(clk, 0);
    delay();
}

void TM1637::set_segments(const uint8_t segments[4]) {
    start();
    write_byte(0x40); // set auto-increment mode
    stop();

    start();
    write_byte(0xc0); // set starting address to 0
    for (int i = 0; i < 4; i++) {
        write_byte(segments[i]);
    }
    stop();

    start();
    write_byte(0x88 | (brightness & 0x0f)); // display control, set brightness
    stop();
}

void TM1637::set_brightness(uint8_t b) {
    brightness = b & 0x0f;
    uint8_t segs[4] = {0, 0, 0, 0};
    set_segments(segs); // refresh display with new brightness
}

void TM1637::display_number(int num) {
    uint8_t segs[4];
    for (int i = 3; i >= 0; i--) {
        if (num > 0) {
            segs[i] = digitToSegment[num % 10];
            num /= 10;
        } else {
            segs[i] = BLANK_SEGMENT; // blank
        }
    }
    set_segments(segs);
}

void TM1637::display_digits(uint8_t digits[4], uint8_t decimal_point = -1) {
    uint8_t segs[4];
    for (int i = 0; i < 4; i++) {
        if (digits[i] <= 9)
            segs[i] = digitToSegment[digits[i]];
        else
            segs[i] = BLANK_SEGMENT; // blank
    }
    if(decimal_point >= 0 && decimal_point < 4) {
        segs[3 - decimal_point] |= SEG_DP; // add decimal point
    } 
    set_segments(segs);
}

int TM1637::display_float(float value, uint8_t decimal_point) {
    // decimal_point: 소수점 위치(0=맨 오른쪽, 3=맨 왼쪽)
    // 예: 12.34, decimal_point=2 -> "12.34" 표시

    if ((decimal_point > 3) || (decimal_point < 0)) {
        clear();
        return -1;
    }

    bool negative = value < 0;
    if (negative) value = -value;

    // 소수점 위치에 맞게 스케일링 (예: decimal_point=2면 100 곱함)
    int scale = 1;
    for (uint8_t i = 0; i < decimal_point; ++i) scale *= 10;
    int int_value = (int)(value * scale + 0.5f); // 반올림

    // 4자리를 넘어가면 표시 불가
    if (((!negative) && int_value > 9999) || (negative && int_value > 999)){
        clear();
        return -1;
    }

    uint8_t segs[4];
    // 각 자리수 추출
    for (int i = 3; i >= 0; --i) {
        segs[i] = encodeDigit(int_value % 10);
        int_value /= 10;
    }

    // 소수점 위치에 점 추가
    if (decimal_point >= 0)
        segs[3 - decimal_point] |= SEG_DP;

    // 음수 부호 처리 (맨 앞자리)
    if (negative) {
        segs[0] = SEG_G;
    }

    set_segments(segs);
    return 0;    
}
void TM1637::clear() {
    uint8_t segs[4] = {0, 0, 0, 0};
    set_segments(segs);
}


uint8_t TM1637::encodeDigit(uint8_t digit)
{
	return digitToSegment[digit & 0x0f];
}