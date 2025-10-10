/*****************************************************************************
* | File      	:   dev_config.h
* | Author      :   Waveshare team
* | Function    :   Hardware underlying interface
* | Info        :
*----------------
* |	This version:   V1.0
* | Date        :   2025-08-29
* | Info        :   Basic version
*
******************************************************************************/
#ifndef _DEV_CONFIG_H_
#define _DEV_CONFIG_H_

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_gpio.h"
#include "tkl_i2c.h"
#include "tkl_pinmux.h"

#include "tdd_button_gpio.h"
#include "tdl_button_manage.h"

#define EXAMPLE_PWR_BUTTON_NAME "btn_pwr"


// Power Button GPIO
#ifndef EXAMPLE_SYS_PWR_PIN
#define EXAMPLE_SYS_PWR_PIN TUYA_GPIO_NUM_18
#endif

// Power Domain Enable GPIO
#ifndef EXAMPLE_SYS_EN_PIN
#define EXAMPLE_SYS_EN_PIN TUYA_GPIO_NUM_19
#endif

// GPS I2C Pins (I2C Port 0 - GPIO 20/21)
#ifndef GPS_I2C_SCL_PIN
#define GPS_I2C_SCL_PIN TUYA_GPIO_NUM_20
#endif
#ifndef GPS_I2C_SDA_PIN
#define GPS_I2C_SDA_PIN TUYA_GPIO_NUM_21
#endif

// BMM150 Magnetometer I2C Pins (I2C Port 1 - GPIO 24/25)
#ifndef BMM150_I2C_SCL_PIN_NUM
#define BMM150_I2C_SCL_PIN_NUM TUYA_GPIO_NUM_24
#endif
#ifndef BMM150_I2C_SDA_PIN_NUM
#define BMM150_I2C_SDA_PIN_NUM TUYA_GPIO_NUM_25
#endif

// Legacy compatibility - defaults to GPS pins
#ifndef EXAMPLE_I2C_SCL_PIN
#define EXAMPLE_I2C_SCL_PIN GPS_I2C_SCL_PIN
#endif
#ifndef EXAMPLE_I2C_SDA_PIN
#define EXAMPLE_I2C_SDA_PIN GPS_I2C_SDA_PIN
#endif


OPERATE_RET dev_gpio_init(uint8_t pin, uint8_t mode);
OPERATE_RET dev_sys_init();

OPERATE_RET dev_digital_write(uint8_t pin, uint8_t value);
OPERATE_RET dev_digital_read(uint8_t pin, uint8_t *value);

// GPS I2C functions (uses I2C port 0 with GPIO 20/21)
OPERATE_RET dev_i2c_init();
OPERATE_RET dev_i2c_write(uint8_t addr, uint8_t reg, uint8_t value);
OPERATE_RET dev_i2c_write_nbytes(uint8_t addr, uint8_t *pdata, uint32_t len);
OPERATE_RET dev_i2c_read_nbytes(uint8_t addr, uint8_t reg, uint8_t *pdata, uint32_t len);
OPERATE_RET dev_i2c_read_only_nbytes(uint8_t addr, uint8_t *pdata, uint32_t len);

// BMM150 I2C functions (uses I2C port 1 with GPIO 24/25)
OPERATE_RET bmm150_i2c_port_init();
OPERATE_RET bmm150_i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t value);
OPERATE_RET bmm150_i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *buffer, uint8_t length);

#endif
