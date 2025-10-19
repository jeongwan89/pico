#pragma once
#include <string>
extern "C" {
#include "tm1637.h"
}

namespace display {
class TM1637 {
public:
    TM1637(uint8_t dio, uint8_t clk) : d_(tm1637_create(dio, clk)) {}
    void brightness(uint8_t b) { tm1637_set_brightness(&d_, b); }
    void show(const std::string& s) { tm1637_show_string(&d_, s.c_str()); }
private:
    tm1637_t d_;
};
}
