#ifndef LC76G_H
#define LC76G_H

#include "dev_config.h"

#define LC76G_LIBRARY_VERSION "1.0.0"

#ifndef EXAMPLE_GPS_RESET_PIN
#define EXAMPLE_GPS_RESET_PIN TUYA_GPIO_NUM_39
#endif

#define LC76G_ADDRESS  0x50 
#define DEVICE_ADDRESS_R 0x54

typedef enum {
    LC76G_INTERFACE_I2C,
    LC76G_INTERFACE_UART
} lc76g_interface_t;

typedef struct {
    lc76g_interface_t interface;
    union {
        struct {
            uint8_t addr_wr;
            uint8_t addr_r;
        } i2c;
        struct {
            TUYA_UART_NUM_E port;
            uint32_t baudrate;
        } uart;
    } config;
} lc76g_dev_t;

typedef struct {
    int utc_hour;
    int utc_minute;
    int utc_second;
    int utc_millisecond;
    double latitude_deg;
    double longitude_deg;
    double altitude_m;
    char date_ddmmyy[7];
    int fix_quality;
    int satellites_in_use;
    int connect_state;
    int signal_level_5;
    double speed_kmh;
    double course_deg;
    char last_status;
} lc76g_state_t;

/**
 * Initialize LC76G with I2C
 **/
OPERATE_RET lc76g_init_i2c(lc76g_dev_t *dev, uint8_t i2c_addr_wr, uint8_t i2c_addr_r);

/**
 * Initialize LC76G with UART
 **/
OPERATE_RET lc76g_init_uart(lc76g_dev_t *dev, TUYA_UART_NUM_E port, uint32_t baudrate);

/**
 * Initialize LC76G (legacy function for backward compatibility)
 **/
OPERATE_RET lc76g_init(lc76g_dev_t *dev, uint8_t i2c_addr_wr, uint8_t i2c_addr_r);

/**
 * Get GPS data from LC76G
 **/
OPERATE_RET lc76g_get_data(lc76g_dev_t *dev);

/**
 * Get current GPS state
 **/
const lc76g_state_t *lc76g_get_state(void);

/**
 * Get UTC time
 **/
void lc76g_get_utc(int *hh, int *mm, int *ss, int *ms);

/**
 * Get position data
 **/
void lc76g_get_position(double *lat_deg, double *lon_deg, double *alt_m);

/**
 * Get date in DDMMYY format
 **/
void lc76g_get_data_ddmmyy(char out[7]);

/**
 * Get satellite count
 **/
int lc76g_get_sat_count(void);

/**
 * Get fix quality
 **/
int lc76g_get_fix_quality(void);

/**
 * Get connection state
 **/
int lc76g_get_connect_state(void);

/**
 * Get signal level (0-5)
 **/
int lc76g_get_signal_level5(void);

/**
 * Get speed in km/h
 **/
double lc76g_get_speed_kmh(void);

/**
 * Get course in degrees
 **/
double lc76g_get_course_deg(void);

/**
 * Get status character
 **/
char lc76g_get_status_char(void);

#endif // LC76G_H
