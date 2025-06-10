#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"

// tm1637 library
// This library is used to control a TM1637 7-segment display driver.
#include "tm1637.h"

#define TM1637_CLK_PIN 18 // GPIO pin for clock
#define TM1637_DIO_PIN 19 // GPIO pin for data input/output

#define TM1637_2_CLK_PIN 20 // GPIO pin for clock (alternative)
#define TM1637_2_DIO_PIN 21 // GPIO pin for data input/output (alternative)

// I2C defines
// This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define I2C_PORT i2c0
#define I2C_SDA 8
#define I2C_SCL 9

#include <dht.h>

#include "blink.pio.h"

void blink_pin_forever(PIO pio, uint sm, uint offset, uint pin, uint freq) {
    blink_program_init(pio, sm, offset, pin);
    pio_sm_set_enabled(pio, sm, true);

    printf("Blinking pin %d at %d Hz\n", pin, freq);

    // PIO counter program takes 3 more cycles in total than we pass as
    // input (wait for n + 1; mov; jmp)
    pio->txf[sm] = (125000000 / (2 * freq)) - 3;
}



int main()
{
    stdio_init_all();

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
    blink_pin_forever(pio, 0, offset, PICO_DEFAULT_LED_PIN, 3);
    #else
    blink_pin_forever(pio, 0, offset, 6, 3);
    #endif
    // For more pio examples see https://github.com/raspberrypi/pico-examples/tree/master/pio

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
        display2.display_float(static_cast<float> (count) /100, 2); // Display count as float with 2 decimal places
        count = (count + 1) /*% 10000*/; // Increment count, wrap around at 10000
        sleep_ms(100);
    }
}
