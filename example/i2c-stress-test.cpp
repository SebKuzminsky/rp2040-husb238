#include <cstdio>
#include <string.h>
#include <stdlib.h>

#include <hardware/i2c.h>

#include <pico/stdlib.h>

#include "husb238.h"


#define DEBUG 0


int main() {
    stdio_init_all();


    //
    // Initialize i2c.
    //

    i2c_inst_t * i2c;

    const uint sda_gpio = 16;  // pin 21
    const uint scl_gpio = 17;  // pin 22

    i2c = i2c0;
    uint i2c_freq = 400*1000;  // run i2c at 400 kHz

    i2c_init(i2c, i2c_freq);

    gpio_set_function(sda_gpio, GPIO_FUNC_I2C);
    gpio_set_function(scl_gpio, GPIO_FUNC_I2C);

    gpio_pull_up(sda_gpio);
    gpio_pull_up(scl_gpio);


    int comm_errors = 0;
    int passes = 0;
    int disconnects = 0;
    bool connected = false;

    while (1) {
        if (!husb238_connected(i2c)) {
            printf("HUSB238 not connected, check I2C wiring and USB-C connection.\n");
            if (connected) {
                disconnects++;
                comm_errors = 0;
                passes = 0;
            }
            connected = false;

            // This does not reset the HUSB238 out of its crash and disconnect :-(
            i2c_deinit(i2c);
            sleep_ms(100);
            i2c_init(i2c, i2c_freq);
            sleep_ms(100);
        } else {
            husb238_pdo_t pdos[6];
            int current_pdo;
            int r;

            // printf("\x1b[2J");  // VT100: clear screen
            // printf("HUSB238 connected!\n");

            connected = true;
            passes ++;

#if 0
            r = husb238_dump_registers(i2c);
            if (r != PICO_OK) {
                printf("failed to dump registers\n");
                comm_errors++;
                continue;
            }
#endif

            // Get available PDOs.
            r =  husb238_get_pdos(i2c, pdos);
            if (r != PICO_OK) {
                printf("failed to get PDOs\n");
                comm_errors++;
                continue;
            }
#if DEBUG
            printf("available PDOs:\n");
            for (int i = 0; i < 6; ++i) {
                if (pdos[i].max_current > 0.0) {
                    printf("%f V, %04.2fA\n", pdos[i].volts, pdos[i].max_current);
                } else {
                    printf("%f V (not available from this USB-PD source)\n", pdos[i].volts);
                }
            }
#endif

            // Get current PDO.
            r =  husb238_get_current_pdo(i2c, &current_pdo);
            if (r != PICO_OK) {
                printf("failed to get current PDO\n");
                comm_errors++;
                continue;
            }
#if DEBUG
            printf("current PDO: 0x%02x\n", current_pdo);
#endif

            // Get current contract.
            {
                int volts;
                float max_current;
                r =  husb238_get_contract(i2c, volts, max_current);
                if (r != PICO_OK) {
                    printf("failed to get contract\n");
                    comm_errors++;
                    continue;
                }
#if DEBUG
                printf("current contract: %d V, %.2f max current\n", volts, max_current);
#endif
            }
        }

        printf("%d disconnects, %d passes and %d comm errors on this connection\n", disconnects, passes, comm_errors);
        // sleep_ms(100);
    }
}
