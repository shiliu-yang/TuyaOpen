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

#ifndef EXAMPLE_SYS_PWR_PIN
#define EXAMPLE_SYS_PWR_PIN TUYA_GPIO_NUM_18
#endif

#ifndef EXAMPLE_SYS_EN_PIN
#define EXAMPLE_SYS_EN_PIN TUYA_GPIO_NUM_19
#endif

#ifndef EXAMPLE_I2C_SCL_PIN
#define EXAMPLE_I2C_SCL_PIN TUYA_GPIO_NUM_20
#endif

#ifndef EXAMPLE_I2C_SDA_PIN
#define EXAMPLE_I2C_SDA_PIN TUYA_GPIO_NUM_21
#endif

#ifndef EXAMPLE_UART_TX_PIN
#define EXAMPLE_UART_TX_PIN TUYA_GPIO_NUM_41
#endif

#ifndef EXAMPLE_UART_RX_PIN
#define EXAMPLE_UART_RX_PIN TUYA_GPIO_NUM_40
#endif

#ifndef EXAMPLE_UART_PORT
#define EXAMPLE_UART_PORT TUYA_UART_NUM_2
#endif

#ifndef EXAMPLE_UART_BAUDRATE
#define EXAMPLE_UART_BAUDRATE 115200
#endif

OPERATE_RET dev_gpio_init(uint8_t pin, uint8_t mode);
OPERATE_RET dev_sys_init();

OPERATE_RET dev_digital_write(uint8_t pin, uint8_t value);
OPERATE_RET dev_digital_read(uint8_t pin, uint8_t *value);

/* I2C functions */
OPERATE_RET dev_i2c_init();
OPERATE_RET dev_i2c_write(uint8_t addr, uint8_t reg, uint8_t value);
OPERATE_RET dev_i2c_write_nbytes(uint8_t addr, uint8_t *pdata, uint32_t len);
OPERATE_RET dev_i2c_read_nbytes(uint8_t addr, uint8_t reg, uint8_t *pdata, uint32_t len);
OPERATE_RET dev_i2c_read_only_nbytes(uint8_t addr, uint8_t *pdata, uint32_t len);

/* UART functions */
OPERATE_RET dev_uart_init(TUYA_UART_NUM_E port, uint32_t baudrate);
OPERATE_RET dev_uart_deinit(TUYA_UART_NUM_E port);
int dev_uart_write(TUYA_UART_NUM_E port, const uint8_t *data, uint16_t len);
int dev_uart_read(TUYA_UART_NUM_E port, uint8_t *data, uint16_t len, uint32_t timeout_ms);
OPERATE_RET dev_uart_wait_for_data(TUYA_UART_NUM_E port, uint32_t timeout_ms);

#endif
