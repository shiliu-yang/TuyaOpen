#ifndef LC76G_H
#define LC76G_H

#include "dev_config.h"

#define LC76G_LIBRARY_VERSION "1.0.0"

#ifndef EXAMPLE_GPS_RESET_PIN
#define EXAMPLE_GPS_RESET_PIN TUYA_GPIO_NUM_39
#endif

#define LC76G_ADDRESS  0x50 
#define DEVICE_ADDRESS_R 0x54

typedef struct {
    uint8_t i2c_addr_wr;
    uint8_t i2c_addr_r;
} lc76g_dev_t;

/**
 * Initialize LC76G 
 **/
OPERATE_RET lc76g_init(lc76g_dev_t *dev, uint8_t i2c_addr_wr, uint8_t i2c_addr_r);

/**
 * Software reset PCF85063A 
 **/
OPERATE_RET lc76g_get_date(lc76g_dev_t *dev);

#endif // PCF85063A_H
