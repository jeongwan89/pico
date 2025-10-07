# Copilot Instructions for motorCtrl (Raspberry Pi Pico Project)

## 프로젝트 개요
- 이 프로젝트는 Raspberry Pi Pico를 기반으로, DHT22 온습도 센서와 TM1637 7-segment 디스플레이, I2C 장치, PIO를 활용한 LED 제어를 포함합니다.
- 주요 기능: 센서 데이터 읽기, 디스플레이 출력, PIO를 통한 핀 제어, I2C 통신.

## 주요 구조 및 컴포넌트
- `src/motorCtrl.cpp`: 메인 엔트리, 센서 초기화/읽기, 디스플레이 제어, PIO LED 블링킹, I2C 설정.
- `src/motorCtrl.h`: 핀/포트 정의, 콜백 및 주기적 작업 함수 선언.
- `src/motorCtrl_util.cpp`: DHT22 센서 읽기 콜백 및 주기적 작업 함수 구현.
- `src/blink.pio`: PIO 어셈블리 프로그램, LED 블링킹용.
- `src/lib/tm1637/`: TM1637 디스플레이 제어 라이브러리.
- `src/lib/pico_dht/`: DHT22 센서 제어 라이브러리.

## 빌드 및 실행 워크플로우
- **빌드**: `Compile Project` 태스크(혹은 ninja 명령어)를 사용해 `/build` 디렉토리에서 빌드.
    - 예시: `${HOME}/.pico-sdk/ninja/v1.12.1/ninja -C build`
- **실행/플래시**: `Run Project` 또는 `Flash` 태스크로 바이너리 업로드 및 실행.
    - 예시: `${HOME}/.pico-sdk/picotool/2.1.1/picotool/picotool load <bin> -fx`
    - OpenOCD를 통한 플래시: `${HOME}/.pico-sdk/openocd/0.12.0+dev/openocd.exe ...`

## 개발/디버깅 팁
- 센서 및 디스플레이 핀은 `motorCtrl.h`에서 정의됨. 변경 시 헤더와 메인 코드 모두 확인.
- PIO 프로그램은 `blink.pio`에서 정의, C에서 `blink_program_init`으로 초기화.
- DHT22 센서 에러 처리 및 경고 메시지는 `motorCtrl_util.cpp`의 콜백에서 관리.
- TM1637 디스플레이는 여러 인스턴스(`display`, `display2`, `display3`)로 사용 가능.

## 프로젝트별 관례 및 패턴
- 모든 하드웨어 핀/포트는 헤더에서 매크로로 정의.
- 센서/디스플레이 초기화는 객체 생성 후 `init()` 호출.
- 주기적 작업은 `periodic_task()` 함수와 콜백 패턴으로 구현.
- 외부 라이브러리(예: pico-sdk)는 CMake에서 `pico_sdk_import.cmake`로 관리.
- 각 하드웨어별 서브디렉토리(`lib/tm1637`, `lib/pico_dht`)는 독립적으로 CMake로 빌드됨.

## 예시 코드 패턴
```cpp
// DHT22 센서 읽기 및 에러 처리
if (dht22.read()) {
    float temp = dht22.getTemperature();
    float hum = dht22.getHumidity();
    // ...
} else {
    int err = dht22.getLastError();
    // 에러 처리
}

// TM1637 디스플레이 사용
TM1637 display(TM1637_CLK_PIN, TM1637_DIO_PIN);
display.init();
display.set_brightness(0x0f);
display.display_number(1234);
```

## 참고 파일/디렉토리
- `src/motorCtrl.cpp`, `src/motorCtrl.h`, `src/motorCtrl_util.cpp`
- `src/blink.pio`, `src/lib/tm1637/`, `src/lib/pico_dht/`
- `CMakeLists.txt`, `pico_sdk_import.cmake`

---
이 문서는 AI 코딩 에이전트가 motorCtrl 프로젝트에서 즉시 생산적으로 작업할 수 있도록 핵심 구조, 워크플로우, 관례를 요약합니다. 추가 정보가 필요하거나 불명확한 부분이 있으면 피드백을 남겨주세요.
