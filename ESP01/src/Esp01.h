#pragma once

#include "DHT22.h"
#include "tm1637.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"

// I2C 및 TM1637 핀 정의
#define TM1637_CLK_PIN 18
#define TM1637_DIO_PIN 19
#define TM1637_2_CLK_PIN 20
#define TM1637_2_DIO_PIN 21
#define TM1637_3_CLK_PIN 26
#define TM1637_3_DIO_PIN 27
#define I2C_PORT i2c0
#define I2C_SDA 8
#define I2C_SCL 9

// 타입 정의
typedef void (*timer_callback_t)(DHT22 &dht, float &temperature_c, float &humidity);

// 함수 선언
int periodic_task(uint32_t interval_ms, timer_callback_t callback_DHT, DHT22 &dht, float &temperature_c, float &humidity);
void blink_pin_forever(PIO pio, uint sm, uint offset, uint pin, uint freq);
void callbackDHT(DHT22 &dht, float &temperature_c, float &humidity);

// ESP-01 (ESP8266) 연결 설정
// 권장: UART1 사용 — 전원/부트 안정성 우려로 GP0/GP1 대신 GP4/GP5 사용
// 하드웨어 연결: ESP-01 RX -> Pico GP4 (UART1 TX), ESP-01 TX -> Pico GP5 (UART1 RX)
// (즉, ESP RX <--- Pico TX(GP4), ESP TX ---> Pico RX(GP5))
#define ESP01_UART uart1
#define ESP01_TX_PIN 4
#define ESP01_RX_PIN 5
#define ESP01_BAUD 57600

// ESP-01 초기화 및 폴링 함수
void esp01_init();
void esp01_poll();
