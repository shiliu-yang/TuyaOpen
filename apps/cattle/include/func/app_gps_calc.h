/**
 * @file app_gps_calc.h
 * @brief GPS calculation utilities for distance and bearing
 * 
 * This module provides accurate GPS calculations using proper geodetic formulas:
 * - Haversine formula for distance calculation
 * - Forward azimuth formula for bearing calculation
 * 
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __APP_GPS_CALC_H__
#define __APP_GPS_CALC_H__

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
 * @brief GPS calculation result structure
 */
typedef struct {
    float distance_meters;      /* Distance in meters */
    float bearing_degrees;      /* Bearing angle in degrees (0-360, 0=North, clockwise) */
    bool valid;                 /* true if calculation is valid */
} app_gps_calc_result_t;

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Calculate distance between two GPS coordinates using Haversine formula
 * 
 * The Haversine formula calculates the great-circle distance between two points
 * on a sphere given their longitudes and latitudes. This is accurate for most
 * practical purposes on Earth.
 * 
 * @param tracker_lat Tracker latitude in decimal degrees (-90 to 90)
 * @param tracker_lon Tracker longitude in decimal degrees (-180 to 180)
 * @param target_lat Target latitude in decimal degrees (-90 to 90)
 * @param target_lon Target longitude in decimal degrees (-180 to 180)
 * @return Distance in meters, or -1.0f if input is invalid
 */
float app_gps_calc_distance(float tracker_lat, float tracker_lon, 
                            float target_lat, float target_lon);

/**
 * @brief Calculate bearing angle from tracker to target
 * 
 * Calculates the forward azimuth (bearing) from tracker position to target position.
 * The bearing is the angle measured clockwise from true north.
 * 
 * Formula: θ = atan2(sin(Δλ)·cos(φ2), cos(φ1)·sin(φ2) − sin(φ1)·cos(φ2)·cos(Δλ))
 * 
 * @param tracker_lat Tracker latitude in decimal degrees (-90 to 90)
 * @param tracker_lon Tracker longitude in decimal degrees (-180 to 180)
 * @param target_lat Target latitude in decimal degrees (-90 to 90)
 * @param target_lon Target longitude in decimal degrees (-180 to 180)
 * @return Bearing in degrees (0-360, 0=North, 90=East, 180=South, 270=West),
 *         or -1.0f if input is invalid
 */
float app_gps_calc_bearing(float tracker_lat, float tracker_lon,
                           float target_lat, float target_lon);

/**
 * @brief Calculate both distance and bearing in one call (optimized)
 * 
 * This function calculates both distance and bearing between tracker and target
 * in a single call, which is more efficient than calling the two functions separately
 * as it reuses intermediate calculations.
 * 
 * @param tracker_lat Tracker latitude in decimal degrees (-90 to 90)
 * @param tracker_lon Tracker longitude in decimal degrees (-180 to 180)
 * @param target_lat Target latitude in decimal degrees (-90 to 90)
 * @param target_lon Target longitude in decimal degrees (-180 to 180)
 * @param result Pointer to result structure
 * @return OPERATE_RET OPRT_OK on success, error code otherwise
 */
OPERATE_RET app_gps_calc_distance_and_bearing(float tracker_lat, float tracker_lon,
                                               float target_lat, float target_lon,
                                               app_gps_calc_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* __APP_GPS_CALC_H__ */

