#ifndef I2C_SCANNER_H
#define I2C_SCANNER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Scan I2C bus for devices
 *
 * Scans all addresses from 0x00 to 0x7F and reports which devices respond.
 * Helpful for debugging I2C sensor issues.
 *
 * @param scl_pin I2C SCL GPIO pin
 * @param sda_pin I2C SDA GPIO pin
 */
void i2c_scanner_scan(int scl_pin, int sda_pin);

#ifdef __cplusplus
}
#endif

#endif // I2C_SCANNER_H
