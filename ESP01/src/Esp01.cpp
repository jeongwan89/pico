#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"

// tm1637 library
// This library is used to control a TM1637 7-segment display driver.
#include "tm1637.h"
#include "Esp01.h"
#include "ATmqtt.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"

// I2C defines
// This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments

#include "DHT22.h"
#include <stdio.h>

#include "blink.pio.h"
void blink_pin_forever(PIO pio, uint sm, uint offset, uint pin, uint freq) {
    blink_program_init(pio, sm, offset, pin);
    pio_sm_set_enabled(pio, sm, true);
    printf("Blinking pin %d at %d Hz\n", pin, freq);
    pio->txf[sm] = (125000000 / (2 * freq)) - 3;
}


int main()
{
    stdio_init_all();

    // Initialize ESP-01 UART
    esp01_init();

    // Try to connect to WiFi via ESP-01 (blocking call)
    // SSID: farmmain5G, password: wweerrtt
    if (esp01_wifi_connect("farmmain5G", "wweerrtt", 20000)) {
        printf("ESP01 WiFi connected (attempt returned success)\n");
        // After WiFi is up, configure MQTT and subscribe to topics
        const char *topics[] = {
            "Sensor/GH1/Center/Temp",
            "Sensor/GH1/Center/Hum",
            "Sensor/GH2/Center/Temp",
            "Sensor/GH2/Center/Hum",
            "Sensor/GH3/Center/Temp",
            "Sensor/GH3/Center/Hum",
            "Sensor/GH4/Center/Temp",
            "Sensor/GH4/Center/Hum",
            "Sensor/Spare/One",
            "Sensor/Spare/Two"
        };
        if (esp01_mqtt_setup("192.168.0.24", 1883, "farmmain", "eerrtt", topics, 10)){
            // register incoming message callback to print to terminal
            atmqtt_set_msg_callback(esp01_print_msg_callback);
            printf("MQTT setup completed\n");
        } else {
            printf("MQTT setup failed\n");
        }
    } else {
        printf("ESP01 WiFi connect attempt failed or timed out\n");
    }

    // DHT22센서를 GPIO 20에 연결
    DHT22 dht22(15); // GPIO 15 is used for DHT22 data pin
    printf("DHT22 센서 테스트 시작\n");


    // DHT22 센서 초기화
    if (!dht22.init()) {
        printf("DHT22 initialization failed!\n");
        return -1;
    }
    printf("DHT22 sensor initialized successfully.\n");

    printf("DHT22 센서 테스트 시작\n");
    printf("온도와 습도를 5초마다 읽습니다...\n\n");
    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400*1000);
    
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    // For more examples of I2C use see https://github.com/raspberrypi/pico-examples/tree/master/i2c

    // PIO Blinking example
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &blink_program);
    printf("Loaded program at %d\n", offset);
    
    #ifdef PICO_DEFAULT_LED_PIN
    blink_pin_forever(pio, 0, offset, PICO_DEFAULT_LED_PIN, 2);
    #else
    blink_pin_forever(pio, 0, offset, 6, 3);
    #endif

    // TM1637 Initialisation
    TM1637 display(TM1637_CLK_PIN, TM1637_DIO_PIN);
    TM1637 display2(TM1637_2_CLK_PIN, TM1637_2_DIO_PIN); // Alternative display
    TM1637 display3(TM1637_3_CLK_PIN, TM1637_3_DIO_PIN); // Third display
    display.init();
    display.set_brightness(0x0f); // Set maximum brightness
    display2.init(); // Initialize the second display
    display2.set_brightness(0x0f); // Set maximum brightness for the second display
    display3.init(); // Initialize the third display
    display3.set_brightness(0x0f); // Set maximum brightness for the third display
    
    // Main loop
    int count = 0;
    static float humidity = 0.0f;
    static float temperature_c = 0.0f;
    while (true) {
        printf("Hello, world!\n");
        // ESP-01 폴링: 수신 데이터가 있으면 처리
    esp01_poll();
    // process ATMQTT incoming messages and invoke callback
    atmqtt_process_io();
    // maintain MQTT connection (reconnect if needed)
    esp01_mqtt_maintain();
        display.display_number(count); // 첫 번째 TM1637에 count 표시
        periodic_task(5000, callbackDHT, dht22, temperature_c, humidity); // 5초마다 센서값 갱신
        display2.display_float(humidity, 1); // 두 번째 TM1637에 습도 표시
        display3.display_float(temperature_c, 1); // 세 번째 TM1637에 온도 표시
        count = (count + 1) % 10000;
        sleep_ms(100);
    }
}

// ESP-01 초기화
void esp01_init(){
    // Configure UART1 for ESP-01 on GP4/GP5
    uart_init(ESP01_UART, ESP01_BAUD);
    // Map GP4 -> UART1 TX, GP5 -> UART1 RX
    gpio_set_function(ESP01_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(ESP01_RX_PIN, GPIO_FUNC_UART);
    // Optional: enable FIFO, set format (8N1)
    uart_set_format(ESP01_UART, 8, 1, UART_PARITY_NONE);
    // Disable blocking on stdin/stdout for uart
    uart_set_fifo_enabled(ESP01_UART, true);
}

// ESP-01 폴링: 수신된 바이트를 읽어 표준 출력으로 에코하거나 추가 처리 가능
void esp01_poll(){
    while(uart_is_readable(ESP01_UART)){
        int c = uart_getc(ESP01_UART);
        if(c >= 0){
            // 간단히 stdout으로 에코
            putchar((char)c);
            fflush(stdout);
        }
    }
}
