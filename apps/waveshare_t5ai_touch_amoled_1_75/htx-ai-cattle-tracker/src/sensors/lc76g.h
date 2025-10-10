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
 * GPS state structure
 */
typedef struct {
    int utc_hour;
    int utc_minute;
    int utc_second;
    int utc_millisecond;
    float latitude_deg;
    float longitude_deg;
    float altitude_m;
    char date_ddmmyy[7];
    int fix_quality;
    int satellites_in_use;
    int connect_state;
    int signal_level_5;
    float speed_kmh;
    float course_deg;
    char last_status;
} lc76g_state_t;

/**
 * Initialize LC76G 
 **/
OPERATE_RET lc76g_init(lc76g_dev_t *dev, uint8_t i2c_addr_wr, uint8_t i2c_addr_r);

/**
 * Get GPS data (reads and parses NMEA sentences)
 **/
OPERATE_RET lc76g_get_data(lc76g_dev_t *dev);

/**
 * Get pointer to current GPS state
 **/
const lc76g_state_t *lc76g_get_state(void);

/**
 * Get UTC time
 **/
void lc76g_get_utc(int *hh, int *mm, int *ss, int *ms);

/**
 * Get position (latitude, longitude, altitude)
 **/
void lc76g_get_position(float *lat_deg, float *lon_deg, float *alt_m);

/**
 * Get date in DDMMYY format
 **/
void lc76g_get_data_ddmmyy(char *out);

/**
 * Get satellite count
 **/
int lc76g_get_sat_count(void);

/**
 * Get fix quality (0=no fix, 1=GPS, 2=DGPS)
 **/
int lc76g_get_fix_quality(void);

/**
 * Get connection state (0=disconnected, 1=connected)
 **/
int lc76g_get_connect_state(void);

/**
 * Get signal level (0-5)
 **/
int lc76g_get_signal_level5(void);

/**
 * Get speed in km/h
 **/
float lc76g_get_speed_kmh(void);

/**
 * Get course in degrees
 **/
float lc76g_get_course_deg(void);

/**
 * Get status character from NMEA sentence
 **/
char lc76g_get_status_char(void);

#endif // LC76G_H
