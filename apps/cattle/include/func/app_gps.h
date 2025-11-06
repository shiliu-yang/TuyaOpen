/**
 * @file app_gps.h
 * @brief GPS module driver for LC76G via UART
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __APP_GPS_H__
#define __APP_GPS_H__

#include "tuya_cloud_types.h"
#include <stdbool.h>

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
 * @brief GPS data structure
 */
typedef struct {
    /* Time */
    uint8_t utc_hour;
    uint8_t utc_minute;
    uint8_t utc_second;
    uint16_t utc_millisecond;
    
    /* Position */
    double latitude_deg;      /* Decimal degrees, negative = South */
    double longitude_deg;     /* Decimal degrees, negative = West */
    float altitude_m;         /* Meters above sea level */
    
    /* Quality */
    uint8_t fix_quality;      /* 0=no fix, 1=GPS, 2=DGPS */
    uint8_t satellites_in_use; /* Number of satellites */
    
    /* Motion */
    float speed_kmh;          /* Speed in km/h */
    float course_deg;         /* Course over ground in degrees */
    
    /* Status */
    bool valid;               /* true if fix is valid */
} app_gps_data_t;

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Initialize GPS module
 * 
 * @return OPERATE_RET OPRT_OK on success
 */
OPERATE_RET app_gps_init(void);

/**
 * @brief Start GPS reading task (updates every 10 seconds)
 * 
 * @return OPERATE_RET OPRT_OK on success
 */
OPERATE_RET app_gps_start(void);

/**
 * @brief Get current GPS data
 * 
 * @param data Pointer to GPS data structure
 * @return OPERATE_RET OPRT_OK on success
 */
OPERATE_RET app_gps_get_data(app_gps_data_t *data);

/**
 * @brief Get GPS fix status
 * 
 * @return true if GPS has valid fix
 */
bool app_gps_is_fixed(void);

/**
 * @brief Get GPS status (valid and satellite count)
 * 
 * @param valid Pointer to store GPS valid status (true if has fix)
 * @param satellite_count Pointer to store number of satellites in use
 * @return OPERATE_RET OPRT_OK on success
 */
OPERATE_RET app_gps_get_status(bool *valid, uint8_t *satellite_count);

#ifdef __cplusplus
}
#endif

#endif /* __APP_GPS_H__ */
