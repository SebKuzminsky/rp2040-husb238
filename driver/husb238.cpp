#include <stdio.h>
#include <hardware/i2c.h>

#include "husb238.h"


// 0: dont say anything
// 1: only say errors
// 2: verbose debug output
#define HUSB238_VERBOSE 1

#define HUSB238_ERROR(fmt, args...) if (HUSB238_VERBOSE >= 1) { printf(fmt, ## args); }
#define HUSB238_PRINT(fmt, args...) if (HUSB238_VERBOSE >= 2) { printf(fmt, ## args); }

#define HUSB238_I2C_SLAVE_ADDRESS (0x08)

#define HUSB238_I2C_REG_PD_STATUS0  (0x00)
#define HUSB238_I2C_REG_PD_STATUS1  (0x01)
#define HUSB238_I2C_REG_SRC_PDO_5V  (0x02)
#define HUSB238_I2C_REG_SRC_PDO_9V  (0x03)
#define HUSB238_I2C_REG_SRC_PDO_12V (0x04)
#define HUSB238_I2C_REG_SRC_PDO_15V (0x05)
#define HUSB238_I2C_REG_SRC_PDO_18V (0x06)
#define HUSB238_I2C_REG_SRC_PDO_20V (0x07)
#define HUSB238_I2C_REG_SRC_PDO     (0x08)
#define HUSB238_I2C_REG_GO_COMMAND  (0x09)

#define HUSB238_CMD_HARD_RESET  (0x10)
#define HUSB238_CMD_GET_SRC_CAP (0x04)
#define HUSB238_CMD_SELECT_PDO  (0x01)


// This is the active voltage, indexed by the PD_SRC_VOLTAGE field of
// register PD_STATUS0.
static int husb238_pd_src_voltage[] = {
    -1, // no PD voltage attached
    5, 9, 12, 15, 18, 20
};

// This is the max current, indexed by the PD_SRC_CURRENT field of
// register PD_STATUS0.
static float husb238_pd_src_current[] = {
    0.5, 0.7,
    1.0, 1.25, 1.5, 1.75,
    2.0, 2.25, 2.5, 2.75,
    3.0, 3.25, 3.5,
    4.0, 4.5,
    5.0
};


//
// 1 for timeouts, 0 for blocking
//
// When running i2c in blocking mode, communications sometimes freezes
// with the data line held low.  Power cycling the HUSB238 un-freezes it.
//

#define I2C_TIMEOUT 1

#ifdef I2C_TIMEOUT
// Running i2c at 100 kHz.  10 µs/bit nominal, 12 µs actual (measured).
// Call it 13 µs to have some margin.
static uint const i2c_bit_time_us = 13;

// 8 bits/byte, plus the Ack/Nack bit.
static uint const i2c_byte_timeout_us = i2c_bit_time_us * 9 * 2;
#endif


static int husb238_read_pdo(i2c_inst_t * i2c, husb238_pdo_t * pdo) {
    int r;
    uint8_t val;

    r = husb238_read_register(i2c, pdo->reg, &val);
    if (r != PICO_OK) return r;
    if (val & 0x80) {
        // SRC PDO detected
        pdo->max_current = husb238_pdo_max_current(val);
    } else {
        pdo->max_current = 0.0;
    }

    return 0;
}


// Reads the presently active voltage and max current.
// Returns PICO_OK if it worked, something else on error.
int husb238_get_contract(i2c_inst_t * i2c, int & volts, float & max_current) {
    int r;
    uint8_t val;

    r = husb238_read_pd_status0(i2c, &val);
    if (r != PICO_OK) {
        return r;
    }

    int voltage_index = val >> 4;
    volts = husb238_pd_src_voltage[voltage_index];

    int current_index = val & 0x0f;
    max_current = husb238_pd_src_current[current_index];

    return PICO_OK;
}


// Read all the SRC_PDO registers from the HUSB238, populate the `pdos`
// argument with the data.
// Returns PICO_OK if all went well.
int husb238_get_pdos(i2c_inst_t * i2c, husb238_pdo_t pdos[6]) {
    int r;

    pdos[0] = { HUSB238_SRC_PDO_5V,  HUSB238_I2C_REG_SRC_PDO_5V,   5.0, 0.0 };
    pdos[1] = { HUSB238_SRC_PDO_9V,  HUSB238_I2C_REG_SRC_PDO_9V,   9.0, 0.0 };
    pdos[2] = { HUSB238_SRC_PDO_12V, HUSB238_I2C_REG_SRC_PDO_12V, 12.0, 0.0 };
    pdos[3] = { HUSB238_SRC_PDO_15V, HUSB238_I2C_REG_SRC_PDO_15V, 15.0, 0.0 };
    pdos[4] = { HUSB238_SRC_PDO_18V, HUSB238_I2C_REG_SRC_PDO_18V, 18.0, 0.0 };
    pdos[5] = { HUSB238_SRC_PDO_20V, HUSB238_I2C_REG_SRC_PDO_20V, 20.0, 0.0 };

    for (int i = 0; i < 6; ++i) {
        r = husb238_read_pdo(i2c, &pdos[i]);
        if (r != PICO_OK) return r;
    }

    return PICO_OK;
}


int husb238_get_current_pdo(i2c_inst_t * i2c, int * pdo) {
    int r;
    uint8_t val;

    r = husb238_read_register(i2c, HUSB238_I2C_REG_SRC_PDO, &val);
    if (r != PICO_OK) {
        *pdo = HUSB238_SRC_PDO_NONE;
        return r;
    }
    *pdo = val & 0xf0;
    return PICO_OK;
}


bool husb238_connected(i2c_inst_t * i2c) {
    uint8_t in_data;

#if I2C_TIMEOUT // timeout
    int r = i2c_read_timeout_us(
        i2c,
        HUSB238_I2C_SLAVE_ADDRESS,
        &in_data,
        sizeof(in_data),
        false,
        (sizeof(in_data) + 1) * i2c_byte_timeout_us
    );
#else
    int r = i2c_read_blocking(
        i2c,
        HUSB238_I2C_SLAVE_ADDRESS,
        &in_data,
        sizeof(in_data),
        false
    );
#endif
    sleep_us(100);

    if (r < 0) {
        HUSB238_ERROR("HUSB238 not responding\n");
        return false;
    } else {
        HUSB238_PRINT("HUSB238 found!\n");
        return true;
    }
}


int husb238_read_register(i2c_inst_t * i2c, uint8_t reg, uint8_t * val) {
    int r;
    uint8_t out_data[] = { reg };
    uint8_t in_data[1];

#if I2C_TIMEOUT
    r = i2c_write_timeout_us(
        i2c,
        HUSB238_I2C_SLAVE_ADDRESS,
        out_data,
        sizeof(out_data),
        false,
        (sizeof(out_data) + 1) * i2c_byte_timeout_us
    );
#else
    r = i2c_write_blocking(
        i2c,
        HUSB238_I2C_SLAVE_ADDRESS,
        out_data,
        sizeof(out_data),
        false
    );
#endif
    sleep_us(100);

    if (r != sizeof(out_data)) {
        HUSB238_ERROR("failed to write register address 0x%02x to HUSB238: %d\n", reg, r);
        return PICO_ERROR_GENERIC;
    }
    HUSB238_PRINT("wrote register address 0x%02x\n", out_data[0]);

#if I2C_TIMEOUT
    r = i2c_read_timeout_us(
        i2c,
        HUSB238_I2C_SLAVE_ADDRESS,
        in_data,
        sizeof(in_data),
        false,
        (sizeof(in_data) + 1) * i2c_byte_timeout_us
    );
#else
    r = i2c_read_blocking(
        i2c,
        HUSB238_I2C_SLAVE_ADDRESS,
        in_data,
        sizeof(in_data),
        false
    );
#endif
    sleep_us(100);

    if (r != sizeof(in_data)) {
        HUSB238_ERROR("failed to read register data from HUSB238: %d\n", r);
        return PICO_ERROR_GENERIC;
    }
#if HUSB238_VERBOSE >= 2
    for (size_t i = 0; i < sizeof(in_data); ++i) {
        printf("    0x%02x\n", in_data[i]);
    }
#endif
    *val = in_data[0];
    return PICO_OK;
}


int husb238_write_register(i2c_inst_t * i2c, uint8_t reg, uint8_t val) {
    int r;
    uint8_t out_data[] = { reg, val };

#if I2C_TIMEOUT
    r = i2c_write_timeout_us(
        i2c,
        HUSB238_I2C_SLAVE_ADDRESS,
        out_data,
        sizeof(out_data),
        false,
        (sizeof(out_data) + 1) * i2c_byte_timeout_us
    );
#else
    r = i2c_write_blocking(
        i2c,
        HUSB238_I2C_SLAVE_ADDRESS,
        out_data,
        sizeof(out_data),
        false
    );
#endif
    sleep_us(100);

    if (r != sizeof(out_data)) {
        HUSB238_ERROR("failed to write 0x%02x to register address 0x%02x of HUSB238: %d\n", val, reg, r);
        return PICO_ERROR_GENERIC;
    }
    HUSB238_PRINT("wrote 0x%02x to register address 0x%02x\n", out_data[1], out_data[0]);
    return PICO_OK;
}


int husb238_reset(i2c_inst_t * i2c) {
    int r = husb238_write_register(i2c, HUSB238_I2C_REG_GO_COMMAND, HUSB238_CMD_HARD_RESET);
    if (r != PICO_OK) {
        return r;
    }

    // It takes the HUSB238 about 1500 ms to come out of reset.  1000 ms
    // is not enough.
    sleep_ms(1500);
    return PICO_OK;
}


int husb238_get_src_cap(i2c_inst_t * i2c) {
    return husb238_write_register(i2c, HUSB238_I2C_REG_GO_COMMAND, HUSB238_CMD_GET_SRC_CAP);
}


// Select the PDO with the specified PDO ID (one of the HUSB238_SRC_PDO_* constants).
// Returns PICO_OK if all went well, some PICO_* error on failure.
int husb238_select_pdo(i2c_inst_t * i2c, int pdo) {
    int r;

    r = husb238_write_register(i2c, HUSB238_I2C_REG_SRC_PDO, pdo);
    if (r != PICO_OK) return r;

    r = husb238_write_register(i2c, HUSB238_I2C_REG_GO_COMMAND, HUSB238_CMD_SELECT_PDO);
    if (r != PICO_OK) return r;

    // It takes the HUSB238 a little while to update its registers to
    // match this command.
    sleep_ms(5);

    return PICO_OK;
}


float husb238_pdo_max_current(uint8_t pdo) {
    int detected = pdo & 0x80;
    if (!detected) {
        return -1.0;
    }

    float max_current[] = {
        0.50,
        0.70,
        1.00,
        1.25,
        1.50,
        1.75,
        2.00,
        2.25,
        2.50,
        2.75,
        3.00,
        3.25,
        3.50,
        4.00,
        4.50,
        5.00
    };

    int c = pdo & 0x0f;
    return max_current[c];
}


int husb238_dump_registers(i2c_inst_t * i2c) {
    uint8_t val;
    int r;

    r = husb238_read_register(i2c, HUSB238_I2C_REG_PD_STATUS0, &val);
    if (r != PICO_OK) return r;
    printf("PD_STATUS0: 0x%02x\n", val);

    int voltage_index = val >> 4;
    printf("    PD source providing %d V\n", husb238_pd_src_voltage[voltage_index]);

    int current_index = val & 0x0f;
    printf("    PD source max current %0.2f A\n", husb238_pd_src_current[current_index]);

    r = husb238_read_register(i2c, HUSB238_I2C_REG_PD_STATUS1, &val);
    if (r != PICO_OK) return r;
    printf("PD_STATUS1: 0x%02x\n", val);

    if (val & 0x80) {
        printf("    CC_DIR: CC1 is connected to CC, or unattached mode\n");
    } else {
        printf("    CC_DIR: CC2 is connected to CC\n");
    }

    if (val & 0x40) {
        printf("    ATTACH: unattached mode\n");
    } else {
        printf("    ATTACH: attached mode\n");
    }

    int pd_response_index = (val & 0x38) >> 3;
    char const * const pd_response_str[] = {
        "no response",
        "success",
        "(reserved)",
        "invalid command or argument",
        "command not supported",
        "transaction fail, no GoodCRC received after sending",
        "(reserved)",
        "(reserved)"
    };
    printf("    PD response: %s\n", pd_response_str[pd_response_index]);

    if (val & 0x04) {
        printf("    5V contract voltage: 5V\n");
    } else {
        printf("    5V contract voltage: unknown voltage, not 5V\n");
    }

    int current_5v_index = val & 0x03;
    float current_5v[] = { 0.0, 1.5, 2.4, 3.0 };
    printf("    5V contract max current: %0.2f A\n", current_5v[current_5v_index]);


    r = husb238_read_register(i2c, HUSB238_I2C_REG_SRC_PDO_5V, &val);
    if (r != PICO_OK) return r;
    printf("SRC_PDO_5V: 0x%02x (%s, %0.2fA max)\n", val, val & 0x80 ? "detected" : "not detected", husb238_pdo_max_current(val));

    r = husb238_read_register(i2c, HUSB238_I2C_REG_SRC_PDO_9V, &val);
    if (r != PICO_OK) return r;
    printf("SRC_PDO_9V: 0x%02x (%s, %0.2fA max)\n", val, val & 0x80 ? "detected" : "not detected", husb238_pdo_max_current(val));

    r = husb238_read_register(i2c, HUSB238_I2C_REG_SRC_PDO_12V, &val);
    if (r != PICO_OK) return r;
    printf("SRC_PDO_12V: 0x%02x (%s, %0.2fA max)\n", val, val & 0x80 ? "detected" : "not detected", husb238_pdo_max_current(val));

    r = husb238_read_register(i2c, HUSB238_I2C_REG_SRC_PDO_15V, &val);
    if (r != PICO_OK) return r;
    printf("SRC_PDO_15V: 0x%02x (%s, %0.2fA max)\n", val, val & 0x80 ? "detected" : "not detected", husb238_pdo_max_current(val));

    r = husb238_read_register(i2c, HUSB238_I2C_REG_SRC_PDO_18V, &val);
    if (r != PICO_OK) return r;
    printf("SRC_PDO_18V: 0x%02x (%s, %0.2fA max)\n", val, val & 0x80 ? "detected" : "not detected", husb238_pdo_max_current(val));

    r = husb238_read_register(i2c, HUSB238_I2C_REG_SRC_PDO_20V, &val);
    if (r != PICO_OK) return r;
    printf("SRC_PDO_20V: 0x%02x (%s, %0.2fA max)\n", val, val & 0x80 ? "detected" : "not detected", husb238_pdo_max_current(val));

    r = husb238_read_register(i2c, HUSB238_I2C_REG_SRC_PDO, &val);
    if (r != PICO_OK) return r;
    printf("SRC_PDO: 0x%02x\n", val);
    switch (val & 0xf0) {
        case HUSB238_SRC_PDO_NONE:
            printf("    no PDO selected\n");
            break;
        case HUSB238_SRC_PDO_5V:
            printf("    5V PDO selected\n");
            break;
        case HUSB238_SRC_PDO_9V:
            printf("    9V PDO selected\n");
            break;
        case HUSB238_SRC_PDO_12V:
            printf("    12V PDO selected\n");
            break;
        case HUSB238_SRC_PDO_15V:
            printf("    15V PDO selected\n");
            break;
        case HUSB238_SRC_PDO_18V:
            printf("    18V PDO selected\n");
            break;
        case HUSB238_SRC_PDO_20V:
            printf("    20V PDO selected\n");
            break;
        default:
            printf("    unknown PDO selected\n");
            break;
    }

    r = husb238_read_register(i2c, HUSB238_I2C_REG_GO_COMMAND, &val);
    if (r != PICO_OK) return r;
    printf("GO_COMMAND: 0x%02x\n", val);

    return PICO_OK;
}


int husb238_read_pd_status0(i2c_inst_t * i2c, uint8_t * val) {
    return husb238_read_register(i2c, HUSB238_I2C_REG_PD_STATUS0, val);
}


int husb238_read_pd_status1(i2c_inst_t * i2c, uint8_t * val) {
    return husb238_read_register(i2c, HUSB238_I2C_REG_PD_STATUS1, val);
}
