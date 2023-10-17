This is a device driver library for the RP2040 to talk to a HUSB238
USB-PD sink controller, e.g. <https://www.adafruit.com/product/5807>

The example program uses the following i2c wiring:

 Name | Pico GPIO | Pico pin | Direction | HUSB238
------+-----------+----------+-----------+--------
 SDA  | 16        | 21       | <->       | SDA
 SCL  | 17        | 22       |  ->       | SCL
 GND  |           | 23       | <->       | GND/V-
