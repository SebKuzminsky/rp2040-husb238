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
            printf("\x1b[2J");  // VT100: clear screen
            printf("HUSB238 connected!\n");
            husb238_dump_registers(i2c);

            husb238_pdo_t pdos[6];
            int r;

            // Show available PDOs.
            r =  husb238_get_pdos(i2c, pdos);
            if (r != PICO_OK) {
                printf("failed to read from husb238\n");
                continue;
            }
            printf("available PDOs:\n");
            for (int i = 0; i < 6; ++i) {
                if (pdos[i].max_current > 0.0) {
                    printf("%f V, %04.2fA\n", pdos[i].volts, pdos[i].max_current);
                } else {
                    printf("%f V (not available from this USB-PD source)\n", pdos[i].volts);
                }
            }

            // Cycle through available PDOs.
            for (int i = 0; i < 6; ++i) {
                if (pdos[i].max_current > 0.0) {
                    printf("\n");
                    printf("selecting PDO (%f V, %04.2fA)\n", pdos[i].volts, pdos[i].max_current);
                    r = husb238_select_pdo(i2c, pdos[i].id);
                    if (r != PICO_OK) {
                        printf("failed to select PDO: %d\n", r);
                        break;
                    }
                    husb238_dump_registers(i2c);
                    sleep_ms(3 * 1000);
                }
            }
        }

        sleep_ms(1000);
    }
}
