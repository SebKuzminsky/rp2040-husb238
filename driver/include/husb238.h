#ifndef __HUSB238_H__
#define __HUSB238_H__

#define HUSB238_SRC_PDO_NONE (0x00)
#define HUSB238_SRC_PDO_5V   (0x10)
#define HUSB238_SRC_PDO_9V   (0x20)
#define HUSB238_SRC_PDO_12V  (0x30)
#define HUSB238_SRC_PDO_15V  (0x80)
#define HUSB238_SRC_PDO_18V  (0x90)
#define HUSB238_SRC_PDO_20V  (0xa0)


typedef struct {
    int id;             // One of the HUSB238_SRC_PDO_* values.
    uint8_t reg;        // The corresponding HUSB238_I2C_REG_SRC_PDO_* register address.
    float volts;        // Nominal voltage of this PDO.
    float max_current;  // Max current available at this voltage as
                        // reported by USB PD Source, 0 means the voltage is not available.
} husb238_pdo_t;


int husb238_get_contract(i2c_inst_t * i2c, int & volts, float & max_current);

int husb238_get_pdos(i2c_inst_t * i2c, husb238_pdo_t pdos[6]);

//
// Reads the currently active PDO ID from the HUSB238 and sets *pdo to
// the corresponding HUSB238_SRC_PDO_* constant.
//
// Returns PICO_OK if everything went ok.  Returns one of the PICO_ERROR_*
// constants from "pico/error.h" if there was a problem, in which case
// *pdo is set to HUSB238_SRC_PDO_NONE, even though that value probably
// doesn't match reality.
//
int husb238_get_current_pdo(i2c_inst_t * i2c, int * pdo);

int husb238_connected(i2c_inst_t * i2c);
int husb238_read_register(i2c_inst_t * i2c, uint8_t reg, uint8_t * val);

int husb238_reset(i2c_inst_t * i2c);
int husb238_get_src_cap(i2c_inst_t * i2c);

//
// Read and print all registers on the HUSB238.
//
// Returns PICO_OK if all went well, or one of the PICO_ERROR_* constants
// from "pico/error.h" if there was a problem.
//
int husb238_dump_registers(i2c_inst_t * i2c);

int husb238_read_pd_status0(i2c_inst_t * i2c, uint8_t * val);
int husb238_read_pd_status1(i2c_inst_t * i2c, uint8_t * val);

float husb238_pdo_max_current(uint8_t pdo);

int husb238_select_pdo(i2c_inst_t * i2c, int pdo);


#endif // __HUSB238_H__
