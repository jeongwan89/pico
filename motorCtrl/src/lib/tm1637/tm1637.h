#pragma once

#include "pico/stdlib.h"
#define SEG_A   0b00000001
#define SEG_B   0b00000010
#define SEG_C   0b00000100
#define SEG_D   0b00001000
#define SEG_E   0b00010000
#define SEG_F   0b00100000
#define SEG_G   0b01000000
#define SEG_DP  0b10000000

class TM1637 {
public:
    TM1637(uint8_t clk_pin, uint8_t dio_pin);

    void init();
    void set_brightness(uint8_t brightness);
    void display_number(int num);
    void display_digits(uint8_t digits[4]);
    int display_float(float value, uint8_t decimal_point = 0);
    void clear();
    //! Translate a single digit into 7 segment code
    //!
    //! The method accepts a number between 0 - 15 and converts it to the
    //! code required to display the number on a 7 segment display.
    //! Numbers between 10-15 are converted to hexadecimal digits (A-F)
    //!
    //! @param digit A number between 0 to 15
    //! @return A code representing the 7 segment image of the digit (LSB - segment A;
    //!         bit 6 - segment G; bit 7 - always zero)
    static uint8_t encodeDigit(uint8_t digit);
private:
    uint8_t clk, dio;
    uint8_t brightness;

    void start();
    void stop();
    void write_byte(uint8_t b);
    void set_segments(const uint8_t segments[4]);
    void delay();
};