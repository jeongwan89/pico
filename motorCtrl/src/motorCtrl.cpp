#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"

// tm1637 library
// This library is used to control a TM1637 7-segment display driver.
#include "tm1637.h"
#include "motorCtrl.h"

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
    display.init();
    display.set_brightness(0x0f); // Set maximum brightness
    display2.init(); // Initialize the second display
    display2.set_brightness(0x0f); // Set maximum brightness for the second display

    int count = 0;
    while (true) {
        printf("Hello, world!\n");
        display.display_number(count); // Display the count on the TM1637
        //display2.display_float(static_cast<float> (count) /100, 2); // Display count as float with 2 decimal places
        count = (count + 1) /*% 10000*/; // Increment count, wrap around at 10000
        sleep_ms(100);
        float humidity;
        float temperature_c;
        periodic_task(5000, callbackDHT, dht22, temperature_c, humidity); // Call the DHT22 read function every 5 seconds
        // Display temperature and humidity on the second display
        uint8_t digits[4] = {
            static_cast<uint8_t>(temperature_c / 10) % 10, // Tens place of temperature
            static_cast<uint8_t>(temperature_c) % 10,      // Units place of temperature
            static_cast<uint8_t>(humidity / 10) % 10,      // Tens place of humidity
            static_cast<uint8_t>(humidity) % 10             // Units place of humidity
        };
        display2.display_digits(digits, 2); // Display temperature and humidity on the second display
    }
}
