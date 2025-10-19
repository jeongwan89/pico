#pragma once
#include <string>
extern "C" {
#include "mqtt_client.h"
}

namespace net {
class MqttClient {
public:
    explicit MqttClient(const mqtt_config_t& cfg) : cfg_(cfg), c_(mqtt_client_create(&cfg_)) {}
    bool connect() { return mqtt_connect(&c_); }
    bool publish(const std::string& topic, const std::string& payload, int qos = 0, bool retain = false) {
        return mqtt_publish(&c_, topic.c_str(), payload.c_str(), qos, retain);
    }
    bool subscribe(const std::string& topic, int qos = 0) {
        return mqtt_subscribe(&c_, topic.c_str(), qos);
    }
    void loop() { mqtt_loop(&c_); }
private:
    mqtt_config_t cfg_;
    mqtt_client_t c_;
};
}
