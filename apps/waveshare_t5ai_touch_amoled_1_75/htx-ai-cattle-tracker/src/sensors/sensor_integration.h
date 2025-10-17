/**
 * @file sensor_integration.h
 * @brief Integration wrapper for BMM150, GPS, and Encoder sensors
 *
 * This header provides unified interfaces for the BMM150 magnetometer, LC76G GPS module,
 * and rotary encoder, making it easy to integrate with the cattle tracker application.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __SENSOR_INTEGRATION_H__
#define __SENSOR_INTEGRATION_H__

#include "tuya_cloud_types.h"
#include "tal_api.h"

// I2C bus coordination mutex for shared I2C Port 0 (GPS + Touch)
extern MUTEX_HANDLE g_i2c_bus_mutex;

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
/**
 * @brief Callback function type for compass recalibration notification
 * 
 * This callback is triggered when the BMM150 sensor detects:
 * - Magnetic turbulence (field strength deviation > threshold)
 * - Large heading deviation (potential magnetic interference)
 * 
 * @param deviation_degrees Heading deviation in degrees (0-180)
 * @param turbulence_detected True if magnetic turbulence was detected
 */
typedef void (*compass_recalibration_cb_t)(float deviation_degrees, bool turbulence_detected);

typedef struct {
    // BMM150 magnetometer data
    float heading_degrees;     /* Compass heading in degrees (0-360) */
    int16_t mag_x;            /* Magnetometer X-axis (calibrated) */
    int16_t mag_y;            /* Magnetometer Y-axis (calibrated) */
    int16_t mag_z;            /* Magnetometer Z-axis (calibrated) */
    bool bmm150_ready;        /* BMM150 sensor initialized and ready */
    bool bmm150_cal_needed;   /* BMM150 calibration needed flag */
    
    // GPS data
    float latitude_deg;       /* GPS latitude in degrees */
    float longitude_deg;      /* GPS longitude in degrees */
    float altitude_m;         /* GPS altitude in meters */
    float speed_kmh;          /* GPS speed in km/h */
    float course_deg;         /* GPS course in degrees */
    int satellites_in_use;    /* Number of GPS satellites in use */
    int fix_quality;          /* GPS fix quality (0=no fix, 1=GPS fix, 2=DGPS fix) */
    bool gps_ready;           /* GPS sensor initialized and ready */
    
    // Encoder data
    int32_t encoder_angle;    /* Encoder rotation angle (incremental) */
    bool encoder_button;      /* Encoder button state (true=pressed) */
    bool encoder_ready;       /* Encoder initialized and ready */
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
 * @brief Initialize rotary encoder input
 *
 * @return OPERATE_RET Initialization result, OPRT_OK indicates success
 */
OPERATE_RET sensor_encoder_init(void);

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

/**
 * @brief Register a callback for compass recalibration notifications
 *
 * This callback will be triggered when the BMM150 sensor detects magnetic
 * interference or turbulence that requires recalibration.
 *
 * @param callback Callback function pointer (NULL to unregister)
 * @return OPERATE_RET Operation result, OPRT_OK indicates success
 */
OPERATE_RET sensor_bmm150_register_recalibration_cb(compass_recalibration_cb_t callback);

#ifdef __cplusplus
}
#endif

#endif /* __SENSOR_INTEGRATION_H__ */

