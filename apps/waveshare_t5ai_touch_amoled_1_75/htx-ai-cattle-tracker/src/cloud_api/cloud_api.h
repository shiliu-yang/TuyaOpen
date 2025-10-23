/**
 * @file cloud_api.h
 * @brief Cloud API module for cattle location tracking
 *
 * This module provides cloud API functionality for querying cattle location data
 * from the Tuya IoT platform. The functionality can be enabled/disabled through
 * the ENABLE_CLOUD_API configuration option.
 *
 * When ENABLE_CLOUD_API is enabled:
 * - Provides real-time cattle location queries from cloud
 * - Includes full API implementation with authentication and data parsing
 *
 * When ENABLE_CLOUD_API is disabled:
 * - Provides stub implementations that return mock data
 * - Reduces binary size and removes cloud dependencies
 *
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __CLOUD_API_H__
#define __CLOUD_API_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define CATTLE_ID_LEN   32
#define CATTLE_NAME_LEN 64

// cattle location data structure
typedef struct {
    int accuracy;                     // accuracy in meters
    char cattleId[CATTLE_ID_LEN];     // unique identifier for the cattle
    char cattleName[CATTLE_NAME_LEN]; // name of the cattle
    uint32_t direction;               // direction in degrees
    double lat;                       // latitude (double precision for better accuracy)
    double lon;                       // longitude (double precision for better accuracy)
    uint64_t locationTime;            // timestamp of the location data
    uint32_t speed;                   // speed in meters/second
} cattle_location_t;

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

OPERATE_RET cloud_api_init(void);

/**
 * @brief Get current error count
 * @return Current consecutive error count
 */
uint8_t cloud_api_get_error_count(void);

/**
 * @brief Get current request interval
 * @return Current request interval in milliseconds
 */
uint32_t cloud_api_get_request_interval(void);

/**
 * @brief Reset error state manually
 * @note This can be used to reset the backoff state when network conditions improve
 */
void cloud_api_reset_error_state(void);

void cloud_api_cattle_id_set(int cattle_id);

void cloud_api_update_cattle_location_ui(uint8_t force_update);

#ifdef __cplusplus
}
#endif

#endif /* __CLOUD_API_H__ */
