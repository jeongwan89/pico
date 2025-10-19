#pragma once
#include <string>
extern "C" {
#include "esp01.h"
}

namespace modem {
class Esp01 {
public:
    explicit Esp01(uart_inst_t* uart) : m_(esp01_create(uart)) {}
    bool init() { return esp01_init(&m_); }
    bool joinWifi(const std::string& ssid, const std::string& pass) {
        wifi_config_t cfg{ssid.c_str(), pass.c_str()};
        return esp01_join_wifi(&m_, &cfg);
    }
private:
    esp01_t m_;
};
}
