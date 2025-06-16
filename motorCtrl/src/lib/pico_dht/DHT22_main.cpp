#include <stdio.h>
#include "pico/stdlib.h"
#include "DHT22.h"

int main() {
    // USB 시리얼 초기화
    stdio_init_all();
    
    // DHT22 센서를 GPIO 2에 연결했다고 가정
    DHT22 dht22(2);
    
    // 센서 초기화
    if (!dht22.init()) {
        printf("DHT22 초기화 실패!\n");
        return -1;
    }
    
    printf("DHT22 센서 테스트 시작\n");
    printf("온도와 습도를 5초마다 읽습니다...\n\n");
    
    while (true) {
        // 센서 데이터 읽기
        if (dht22.read()) {
            // 성공적으로 읽었을 때
            float temperature = dht22.getTemperature();
            float humidity = dht22.getHumidity();
            
            printf("온도: %.1f°C, 습도: %.1f%%\n", temperature, humidity);
            
            // 범위 체크 및 경고
            if (temperature < -40.0f || temperature > 80.0f) {
                printf("경고: 온도 값이 범위를 벗어났습니다!\n");
            }
            
            if (humidity < 0.0f || humidity > 100.0f) {
                printf("경고: 습도 값이 범위를 벗어났습니다!\n");
            }
            
        } else {
            // 읽기 실패시 에러 처리
            int error = dht22.getLastError();
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
        
        // 5초 대기 (DHT22는 2초 이상 간격으로 읽어야 함)
        sleep_ms(5000);
    }
    
    return 0;
}