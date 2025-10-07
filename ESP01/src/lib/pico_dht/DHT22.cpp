#include "DHT22.h"
#include "pico/time.h"
#include "hardware/timer.h"

DHT22::DHT22(uint gpio_pin) 
    : gpio_pin_(gpio_pin), 
      temperature_(0.0f), 
      humidity_(0.0f), 
      last_error_(NO_ERROR),
      initialized_(false) {
}

DHT22::~DHT22() {
    // GPIO 정리는 필요시 여기서 수행
}

bool DHT22::init() {
    // GPIO 핀 초기화
    gpio_init(gpio_pin_);
    gpio_set_dir(gpio_pin_, GPIO_OUT);
    gpio_put(gpio_pin_, 1);  // 초기 상태 HIGH
    
    // 초기화 완료 대기 (DHT22는 파워온 후 2초 대기 필요)
    sleep_ms(2000);
    
    initialized_ = true;
    last_error_ = NO_ERROR;
    return true;
}

bool DHT22::read() {
    if (!initialized_) {
        last_error_ = INIT_ERROR;
        return false;
    }
    
    uint8_t data[5] = {0};  // 40비트 데이터 + 8비트 체크섬
    
    // 1. 시작 신호 전송
    startSignal();
    
    // 2. DHT22 응답 확인
    if (!checkResponse()) {
        return false;
    }
    
    // 3. 40비트 데이터 읽기
    for (int i = 0; i < 40; i++) {
        if (!readBit()) {
            last_error_ = DATA_ERROR;
            return false;
        }
        
        // 비트를 해당 바이트에 저장
        if (i % 8 == 0) {
            data[i / 8] = 0;
        }
        data[i / 8] <<= 1;
        
        // HIGH 신호 길이로 0/1 판단
        uint32_t start_time = time_us_32();
        if (!waitForSignal(false, BIT_TIMEOUT)) {
            last_error_ = TIMEOUT_ERROR;
            return false;
        }
        uint32_t high_time = time_us_32() - start_time;
        
        if (high_time > BIT_THRESHOLD) {
            data[i / 8] |= 1;
        }
    }
    
    // 4. 체크섬 검증
    uint8_t checksum = calculateChecksum(data);
    if (checksum != data[4]) {
        last_error_ = CHECKSUM_ERROR;
        return false;
    }
    
    // 5. 데이터 변환
    // 습도: 상위 16비트
    uint16_t humidity_raw = (data[0] << 8) | data[1];
    humidity_ = humidity_raw / 10.0f;
    
    // 온도: 하위 16비트 (최상위 비트는 부호)
    uint16_t temperature_raw = (data[2] << 8) | data[3];
    if (temperature_raw & 0x8000) {
        temperature_ = -((temperature_raw & 0x7FFF) / 10.0f);
    } else {
        temperature_ = temperature_raw / 10.0f;
    }
    
    last_error_ = NO_ERROR;
    return true;
}

float DHT22::getTemperature() const {
    return temperature_;
}

float DHT22::getHumidity() const {
    return humidity_;
}

int DHT22::getLastError() const {
    return last_error_;
}

void DHT22::startSignal() {
    // 시작 신호: 18ms LOW, 40us HIGH
    gpio_set_dir(gpio_pin_, GPIO_OUT);
    gpio_put(gpio_pin_, 0);
    sleep_us(START_SIGNAL_LOW_TIME);
    gpio_put(gpio_pin_, 1);
    sleep_us(START_SIGNAL_HIGH_TIME);
    
    // 입력 모드로 변경
    gpio_set_dir(gpio_pin_, GPIO_IN);
}

bool DHT22::checkResponse() {
    // DHT22 응답: 80us LOW, 80us HIGH
    if (!waitForSignal(false, RESPONSE_TIMEOUT)) {
        last_error_ = TIMEOUT_ERROR;
        return false;
    }
    
    if (!waitForSignal(true, RESPONSE_TIMEOUT)) {
        last_error_ = TIMEOUT_ERROR;
        return false;
    }
    
    return true;
}

bool DHT22::readBit() {
    // 각 비트는 50us LOW로 시작
    if (!waitForSignal(false, BIT_TIMEOUT)) {
        return false;
    }
    
    // 그 다음 HIGH 신호 (26-28us = 0, 70us = 1)
    if (!waitForSignal(true, BIT_TIMEOUT)) {
        return false;
    }
    
    return true;
}

bool DHT22::waitForSignal(bool level, uint32_t timeout_us) {
    uint32_t start_time = time_us_32();
    
    while (gpio_get(gpio_pin_) != level) {
        if (time_us_32() - start_time > timeout_us) {
            return false;
        }
    }
    
    return true;
}

uint8_t DHT22::calculateChecksum(const uint8_t* data) const {
    return data[0] + data[1] + data[2] + data[3];
}