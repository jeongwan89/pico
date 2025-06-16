#include "motorCtrl.h"
#include <stdio.h>
#include "DHT22.h"
#include "tm1637.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"

// 주기적 작업 실행 함수
typedef void (*timer_callback_t)(DHT22 &dht, float &temperature_c, float &humidity);
void periodic_task(uint32_t interval_ms, timer_callback_t callback_DHT, DHT22 &dht, float &temperature_c, float &humidity) {
    static absolute_time_t next_time;
    if(time_reached(next_time)) {
        callback_DHT(dht, temperature_c, humidity);
        next_time = make_timeout_time_ms(interval_ms);
    }
}


void callbackDHT(DHT22 &dht, float &temperature_c, float &humidity){
    if (dht.read()) {
        temperature_c = dht.getTemperature();
        humidity = dht.getHumidity();
        printf("온도: %.1f°C, 습도: %.1f%%\n", temperature_c, humidity);
        if (temperature_c < -40.0f || temperature_c > 80.0f) {
            printf("경고: 온도 값이 범위를 벗어났습니다!\n");
        }
        if (humidity < 0.0f || humidity > 100.0f) {
            printf("경고: 습도 값이 범위를 벗어났습니다!\n");
        }
    } else {
        int error = dht.getLastError();
        printf("센서 읽기 실패! 에러 코드: %d ", error);
        switch (error) {
            case DHT22::TIMEOUT_ERROR:
                printf("(타임아웃)\n");
                break;
            case DHT22::CHECKSUM_ERROR:
                printf("(체크섬 오류)\n");
                break;
            case DHT22::DATA_ERROR:
                printf("(데이터 오류)\n");
                break;
            case DHT22::INIT_ERROR:
                printf("(초기화 오류)\n");
                break;
            default:
                printf("(알 수 없는 오류)\n");
                break;
        }
    }
}
