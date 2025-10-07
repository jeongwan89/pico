# Copilot Instructions for motorCtrl (Raspberry Pi Pico Project)

## 프로젝트 개요

## 주요 구조 및 컴포넌트

## 빌드 및 실행 워크플로우
    - 예시: `${HOME}/.pico-sdk/ninja/v1.12.1/ninja -C build`
    - 예시: `${HOME}/.pico-sdk/picotool/2.1.1/picotool/picotool load <bin> -fx`
    - OpenOCD를 통한 플래시: `${HOME}/.pico-sdk/openocd/0.12.0+dev/openocd.exe ...`

## 개발/디버깅 팁

## 프로젝트별 관례 및 패턴

## 예시 코드 패턴
```cpp
// DHT22 센서 읽기 및 에러 처리
if (dht22.read()) {
    # Copilot Instructions for Esp01 (Raspberry Pi Pico Project)

    ## 프로젝트 개요
    - 이 프로젝트는 Raspberry Pi Pico 기반의 임베디드 예제입니다. 원래 `motorCtrl`에서 복사되어 새 이름 `Esp01`으로 변경되었습니다.
    - 주요 기능: DHT22 온습도 센서 읽기, TM1637 7-segment 디스플레이 제어, PIO 기반 LED 블링킹, I2C 사용.
    - 주요 소스는 `src/`에 있으며, 하드웨어 관련 라이브러리는 `src/lib/` 내에 위치합니다.

    ## 주요 구조 및 컴포넌트
    - `src/motorCtrl.cpp` (메인 엔트리): 센서 초기화/읽기 루프, 디스플레이 업데이트, PIO 초기화.
    - `src/motorCtrl.h`: 핀 정의, 콜백 시그니처, 유틸 함수 선언.
    - `src/motorCtrl_util.cpp`: DHT22 콜백 및 주기적 작업 구현.
    - `src/blink.pio`: PIO 어셈블리 프로그램 (LED 블링킹).
    - `src/lib/tm1637/`: TM1637 제어 라이브러리 (CMake 하위 프로젝트).
    - `src/lib/pico_dht/`: DHT22 제어 라이브러리 (CMake 하위 프로젝트).

    ## 빌드 및 실행 워크플로우
    - CMake/Ninja 빌드 사용. 루트에서 configure/build:

    ```bash
    cmake -B build -S . -G Ninja
    cmake --build build
    ```

    - VS Code용 제공된 태스크:
        - `Compile Project` (ninja -C build)
        - `Run Project` (picotool load ...)
        - `Flash` (OpenOCD) — `.vscode/tasks.json` 대신 `CMakePresets.json`/환경 의존 설정 사용.

    - 산출물: `build/`에 `.elf`, `.bin`, `.hex`, `.uf2` 등이 생성됩니다.

    ## CMake에서 주의할 점 (프로젝트 특정)
    - 루트 `CMakeLists.txt`에서 프로젝트 이름과 실행 타깃 이름을 `Esp01`로 설정합니다. (이미 업데이트됨)
    - 타깃 이름은 CMake 타깃 심볼로 사용되므로 `pico_generate_pio_header`, `pico_set_program_name`, `pico_enable_stdio_uart` 등 모든 타깃 관련 호출에서 동일한 심볼을 사용해야 합니다.
    - 외부/내부 라이브러리(`src/lib/*`)는 `add_subdirectory()`로 포함되어 있으며, 타깃 링크에 `tm1637`, `dht22_lib` 같은 라이브러리 이름을 사용합니다. 라이브러리 이름이 변경되면 `target_link_libraries()`를 업데이트하세요.
    - PIO 헤더는 `pico_generate_pio_header(<target> ${CMAKE_CURRENT_LIST_DIR}/src/blink.pio)`로 생성됩니다.

    ## 프로젝트별 관례 및 패턴
    - 하드웨어 핀/포트는 `src/motorCtrl.h`에서 매크로로 정의되어 있습니다. 핀 변경 시 이 파일을 먼저 확인하세요.
    - 센서/디스플레이 인스턴스화 후 `init()` 호출 패턴을 따릅니다.
    - 주기적 작업은 `periodic_task()` 형태의 유틸 함수를 사용합니다(프로젝트 내 구현 참조).
    - 코드 스타일은 간단한 C/C++11/17 스타일로, Raspberry Pi Pico SDK 규칙을 따릅니다.

    ## 통합 포인트
    - Raspberry Pi Pico SDK는 `pico_sdk_import.cmake`를 통해 찾습니다. 환경 변수 `PICO_SDK_PATH` 또는 `PICO_SDK_FETCH_FROM_GIT` 사용 가능.
    - 빌드 후 업로드는 `picotool` 또는 OpenOCD를 사용합니다. 사용자의 홈 디렉토리에 설치된 pico 툴 버전을 `CMakeLists.txt` 주석처럼 참조합니다.

    ## 예시 코드 패턴(프로젝트 내 발췌)
    ```cpp
    // PIO blink 초기화 예
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &blink_program);
    blink_program_init(pio, 0, offset, PICO_DEFAULT_LED_PIN);
    pio_sm_set_enabled(pio, 0, true);

    // TM1637 사용
    TM1637 display(TM1637_CLK_PIN, TM1637_DIO_PIN);
    display.init();
    display.set_brightness(0x0f);
    ```

    ## 변경 권장 위치(체크리스트)
    - `CMakeLists.txt`: 프로젝트명/타깃명 (`Esp01`) 확인
    - `.github/copilot-instructions.md`: (이 파일) 프로젝트명 반영 완료
    - `src/` 내부: 파일명(`motorCtrl.cpp` 등)은 원본 유지 가능(원하시면 파일명 변경 및 CMake 업데이트 가능)

    ---
    필요하면 원본 `motorCtrl` 파일명도 `Esp01.cpp` 등으로 리네임하고 CMake를 더 깔끔하게 정리해드릴게요. 어떤 수준까지 리네임/리팩터링 하길 원하시요?
