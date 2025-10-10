/**
 * @file sensor_integration.h
 * @brief Integration wrapper for BMM150 and GPS sensors
 *
 * This header provides unified interfaces for the BMM150 magnetometer and LC76G GPS module,
 * making it easy to integrate with the cattle tracker application.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __SENSOR_INTEGRATION_H__
#define __SENSOR_INTEGRATION_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    // BMM150 magnetometer data
    float heading_degrees;     /* Compass heading in degrees (0-360) */
    int16_t mag_x;            /* Magnetometer X-axis (calibrated) */
    int16_t mag_y;            /* Magnetometer Y-axis (calibrated) */
    int16_t mag_z;            /* Magnetometer Z-axis (calibrated) */
    bool bmm150_ready;        /* BMM150 sensor initialized and ready */
    
    // GPS data
    float latitude_deg;       /* GPS latitude in degrees */
    float longitude_deg;      /* GPS longitude in degrees */
    float altitude_m;         /* GPS altitude in meters */
    float speed_kmh;          /* GPS speed in km/h */
    float course_deg;         /* GPS course in degrees */
    int satellites_in_use;    /* Number of GPS satellites in use */
    int fix_quality;          /* GPS fix quality (0=no fix, 1=GPS fix, 2=DGPS fix) */
    bool gps_ready;           /* GPS sensor initialized and ready */
} sensor_data_t;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Initialize BMM150 magnetometer sensor
 *
 * @return OPERATE_RET Initialization result, OPRT_OK indicates success
 */
OPERATE_RET sensor_bmm150_init(void);

/**
 * @brief Initialize LC76G GPS module
 *
 * @return OPERATE_RET Initialization result, OPRT_OK indicates success
 */
OPERATE_RET sensor_gps_init(void);

/**
 * @brief Start sensor reading tasks
 *
 * This starts background tasks for both BMM150 and GPS reading.
 *
 * @return OPERATE_RET Operation result, OPRT_OK indicates success
 */
OPERATE_RET sensor_tasks_start(void);

/**
 * @brief Get current sensor data
 *
 * @param data Pointer to sensor_data_t structure to fill with current readings
 * @return OPERATE_RET Operation result, OPRT_OK indicates success
 */
OPERATE_RET sensor_get_data(sensor_data_t *data);

/**
 * @brief Print sensor readings to console (for debugging)
 *
 * This prints BMM150 and GPS readings to the console in a formatted way.
 */
void sensor_print_readings(void);

#ifdef __cplusplus
}
#endif

#endif /* __SENSOR_INTEGRATION_H__ */

