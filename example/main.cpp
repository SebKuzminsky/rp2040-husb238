#include <cstdio>
#include <string.h>
#include <stdlib.h>

#include <hardware/i2c.h>

#include <pico/stdlib.h>

#include "husb238.h"


int main() {
    stdio_init_all();


    //
    // Initialize i2c.
    //

    i2c_inst_t * i2c;

    const uint sda_gpio = 16;  // pin 21
    const uint scl_gpio = 17;  // pin 22

    i2c = i2c0;
    i2c_init(i2c, 400*1000);  // run i2c at 400 kHz

    gpio_set_function(sda_gpio, GPIO_FUNC_I2C);
    gpio_set_function(scl_gpio, GPIO_FUNC_I2C);

    gpio_pull_up(sda_gpio);
    gpio_pull_up(scl_gpio);


    while (1) {
        if (!husb238_connected(i2c)) {
            printf("HUSB238 not connected, check I2C wiring and USB-C connection.\n");
        } else {
            printf("HUSB238 connected!\n");
            husb238_dump_registers(i2c);
        }
        sleep_ms(1000);
    }
}
