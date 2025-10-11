// Clean implementation for Esp01 utilities
#include "Esp01.h"
#include "ATmqtt.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>
#include "DHT22.h"
#include "tm1637.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/watchdog.h"

// (watchdog_reboot prototype removed; use hardware/watchdog.h and watchdog_enable())

// Stored config for maintain() to use
static char mqtt_conf_host[64] = {0};
static int mqtt_conf_port = 1883;
static char mqtt_conf_user[64] = {0};
static char mqtt_conf_pass[64] = {0};
#define ESP01_MQTT_MAX_TOPICS 12
static char mqtt_conf_topics[ESP01_MQTT_MAX_TOPICS][128] = {{0}};
static int mqtt_conf_topic_count = 0;

bool esp01_wifi_connect(const char *ssid, const char *password, uint32_t timeout_ms){
    atmqtt_init();
    printf("Attempting to connect to WiFi SSID='%s' with timeout %u ms\n", ssid, (unsigned)timeout_ms);
    uint32_t start = to_ms_since_boot(get_absolute_time());
    uint32_t attempt = 0;
    uint32_t backoff_ms = 1000; // start 1s
    while ((to_ms_since_boot(get_absolute_time()) - start) < timeout_ms){
        attempt++;
        printf("WiFi connect attempt %u\n", (unsigned)attempt);
        if (atmqtt_wifi_connect(ssid, password, 5000)){
            printf("AT reported WiFi connected (CWJAP OK)\n");
            return true;
        }
        // Exponential backoff with cap
        sleep_ms(backoff_ms);
        backoff_ms = (backoff_ms * 2 > 16000) ? 16000 : backoff_ms * 2;
    }
    printf("WiFi connect timed out after %u ms\n", (unsigned)(to_ms_since_boot(get_absolute_time()) - start));
    return false;
}

bool esp01_mqtt_setup(const char *host, int port, const char *username, const char *password, const char *topics[], int topic_count){
    char clientid[64];
    uint32_t rnd = (uint32_t)(to_ms_since_boot(get_absolute_time()) & 0xFFFF);
    snprintf(clientid, sizeof(clientid), "Pico-%04X", (unsigned int)rnd);

    strncpy(mqtt_conf_host, host ? host : "", sizeof(mqtt_conf_host)-1);
    mqtt_conf_host[sizeof(mqtt_conf_host)-1] = '\0';
    mqtt_conf_port = port;
    strncpy(mqtt_conf_user, username ? username : "", sizeof(mqtt_conf_user)-1);
    mqtt_conf_user[sizeof(mqtt_conf_user)-1] = '\0';
    strncpy(mqtt_conf_pass, password ? password : "", sizeof(mqtt_conf_pass)-1);
    mqtt_conf_pass[sizeof(mqtt_conf_pass)-1] = '\0';

    mqtt_conf_topic_count = (topic_count > ESP01_MQTT_MAX_TOPICS) ? ESP01_MQTT_MAX_TOPICS : topic_count;
    for (int i = 0; i < mqtt_conf_topic_count; ++i) {
        strncpy(mqtt_conf_topics[i], topics[i] ? topics[i] : "", sizeof(mqtt_conf_topics[i])-1);
        mqtt_conf_topics[i][sizeof(mqtt_conf_topics[i])-1] = '\0';
    }

    if (!atmqtt_mqtt_usercfg(0, clientid, mqtt_conf_user, mqtt_conf_pass)) return false;
    if (!atmqtt_mqtt_connect(0, mqtt_conf_host, mqtt_conf_port)) return false;

    for (int i = 0; i < mqtt_conf_topic_count; ++i) {
        atmqtt_mqtt_subscribe(0, mqtt_conf_topics[i], 0);
    }
    return true;
}

void esp01_print_msg_callback(const char *topic, const char *payload){
    printf("[MQTT RX] topic='%s' payload='%s'\n", topic ? topic : "", payload ? payload : "");
}

bool esp01_mqtt_maintain(){
    if (atmqtt_is_connected()) return true;
    if (mqtt_conf_host[0] == '\0') return false;
    char clientid[64];
    uint32_t rnd = (uint32_t)(to_ms_since_boot(get_absolute_time()) & 0xFFFF);
    snprintf(clientid, sizeof(clientid), "Pico-%04X", (unsigned int)rnd);
    if (!atmqtt_mqtt_usercfg(0, clientid, mqtt_conf_user, mqtt_conf_pass)) return false;
    if (!atmqtt_mqtt_connect(0, mqtt_conf_host, mqtt_conf_port)) return false;
    for (int i = 0; i < mqtt_conf_topic_count; ++i) atmqtt_mqtt_subscribe(0, mqtt_conf_topics[i], 0);
    return atmqtt_is_connected();
}

int periodic_task(uint32_t interval_ms, timer_callback_t callback_DHT, DHT22 &dht, float &temperature_c, float &humidity){
    static absolute_time_t next_time;
    if (time_reached(next_time)){
        callback_DHT(dht, temperature_c, humidity);
        next_time = make_timeout_time_ms(interval_ms);
        return 1;
    }
    return 0;
}

void callbackDHT(DHT22 &dht, float &temperature_c, float &humidity){
    if (dht.read()){
        temperature_c = dht.getTemperature();
        humidity = dht.getHumidity();
        printf("온도: %.1f°C, 습도: %.1f%%\n", temperature_c, humidity);
    } else {
        int error = dht.getLastError();
        printf("센서 읽기 실패! 에러 코드: %d\n", error);
    }
}

// Soft reset helper. We avoid toggling external power; perform a controlled reboot.
// Uses the internal watchdog reboot for a clean reset of the RP2040.
void reset(){
    printf("reset(): initiating soft reboot via watchdog\n");
    // small delay to flush output
    sleep_ms(50);
    // Reboot using watchdog: enable a very short timeout so the chip resets.
    // This avoids calling watchdog_reboot() which may not be visible in this TU.
    watchdog_enable(1, false); // 1 ms timeout
    // wait briefly for the reset to happen
    sleep_ms(100);
    // fallback: infinite loop if reboot does not occur
    while (true) tight_loop_contents();
}

// Hard-reset the attached ESP-01 by toggling the RST pin (active low).
// This requires hardware connection: Pico GP2 -> ESP-01 RST.
// Hold low for reset_hold_ms (ms), then release and wait post_wait_ms for the
// ESP to boot and be ready for AT commands. Returns after waiting post_wait_ms.
void hardReset(uint32_t reset_hold_ms = 200, uint32_t post_wait_ms = 1500){
    // configure the pin as output and drive low
    gpio_init(ESP01_RST_PIN);
    gpio_set_dir(ESP01_RST_PIN, GPIO_OUT);
    gpio_put(ESP01_RST_PIN, 0); // assert reset (active low)
    printf("hardReset: asserted RST (GP%u) for %u ms\n", (unsigned)ESP01_RST_PIN, (unsigned)reset_hold_ms);
    sleep_ms(reset_hold_ms);
    // release reset (high-impedance or pull-up)
    gpio_put(ESP01_RST_PIN, 1);
    printf("hardReset: released RST, waiting %u ms for ESP to boot\n", (unsigned)post_wait_ms);
    sleep_ms(post_wait_ms);
}
