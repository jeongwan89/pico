#ifndef DHT22_H
#define DHT22_H

#include "pico/stdlib.h"
#include "hardware/gpio.h"

class DHT22 {
public:
    // 생성자
    explicit DHT22(uint gpio_pin);
    
    // 소멸자
    ~DHT22();
    
    // 센서 초기화
    bool init();
    
    // 센서 데이터 읽기
    bool read();
    
    // 온도 값 반환 (섭씨)
    float getTemperature() const;
    
    // 습도 값 반환 (%)
    float getHumidity() const;
    
    // 마지막 에러 코드 반환
    int getLastError() const;
    
    // 에러 코드 정의
    enum ErrorCode {
        NO_ERROR = 0,
        TIMEOUT_ERROR = 1,
        CHECKSUM_ERROR = 2,
        DATA_ERROR = 3,
        INIT_ERROR = 4
    };

private:
    uint gpio_pin_;
    float temperature_;
    float humidity_;
    int last_error_;
    bool initialized_;
    
    // 내부 함수들
    bool waitForSignal(bool level, uint32_t timeout_us);
    bool readBit();
    void startSignal();
    bool checkResponse();
    uint8_t calculateChecksum(const uint8_t* data) const;
    
    // 타이밍 상수 (마이크로초)
    static const uint32_t START_SIGNAL_LOW_TIME = 18000;    // 18ms
    static const uint32_t START_SIGNAL_HIGH_TIME = 40;      // 40us
    static const uint32_t RESPONSE_TIMEOUT = 100;           // 100us
    static const uint32_t BIT_TIMEOUT = 100;                // 100us
    static const uint32_t BIT_THRESHOLD = 50;               // 50us (0과 1 구분)
};

#endif // DHT22_H