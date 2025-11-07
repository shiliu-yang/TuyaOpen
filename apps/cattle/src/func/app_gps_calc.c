/**
 * @file app_gps_calc.c
 * @brief GPS calculation utilities for distance and bearing
 * 
 * This module implements accurate GPS calculations:
 * 
 * 1. Distance Calculation (Haversine Formula):
 *    - Calculates great-circle distance between two points on Earth
 *    - Accounts for Earth's spherical shape
 *    - Accuracy: ~0.5% error for distances up to several hundred kilometers
 * 
 * 2. Bearing Calculation (Forward Azimuth):
 *    - Calculates the initial bearing (forward azimuth) from point A to point B
 *    - Result is angle measured clockwise from true north (0-360°)
 *    - Formula accounts for Earth's curvature and longitude convergence
 * 
 * Key Formulas:
 * 
 * Distance (Haversine):
 *   a = sin²(Δφ/2) + cos(φ1)·cos(φ2)·sin²(Δλ/2)
 *   c = 2·atan2(√a, √(1-a))
 *   d = R·c
 * 
 * Bearing (Forward Azimuth):
 *   θ = atan2(sin(Δλ)·cos(φ2), cos(φ1)·sin(φ2) - sin(φ1)·cos(φ2)·cos(Δλ))
 * 
 * Where:
 *   φ1, λ1 = latitude and longitude of point 1 (tracker)
 *   φ2, λ2 = latitude and longitude of point 2 (target)
 *   Δφ = φ2 - φ1
 *   Δλ = λ2 - λ1
 *   R = Earth's radius (6371000 meters)
 * 
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "app_gps_calc.h"
#include "tal_api.h"
#include "tal_log.h"
#include <math.h>
#include <stdbool.h>

/***********************************************************
************************macro define************************
***********************************************************/
#define EARTH_RADIUS_METERS 6371000.0f  /* Mean Earth radius in meters */
#define DEG_TO_RAD(deg) ((deg) * M_PI / 180.0f)
#define RAD_TO_DEG(rad) ((rad) * 180.0f / M_PI)

/* Input validation ranges */
#define MIN_LATITUDE  -90.0f
#define MAX_LATITUDE   90.0f
#define MIN_LONGITUDE -180.0f
#define MAX_LONGITUDE  180.0f

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/
static bool validate_gps_coordinates(float lat, float lon);
static float normalize_angle_0_360(float angle_deg);

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Validate GPS coordinates
 * 
 * @param lat Latitude in decimal degrees
 * @param lon Longitude in decimal degrees
 * @return true if coordinates are valid, false otherwise
 */
static bool validate_gps_coordinates(float lat, float lon)
{
    /* Check for NaN */
    if (isnan(lat) || isnan(lon)) {
        PR_ERR("[GPS_CALC] Invalid coordinates: NaN detected");
        return false;
    }
    
    /* Check for infinite values */
    if (isinf(lat) || isinf(lon)) {
        PR_ERR("[GPS_CALC] Invalid coordinates: Infinity detected");
        return false;
    }
    
    /* Check latitude range */
    if (lat < MIN_LATITUDE || lat > MAX_LATITUDE) {
        PR_ERR("[GPS_CALC] Invalid latitude: %.6f (must be -90 to 90)", lat);
        return false;
    }
    
    /* Check longitude range */
    if (lon < MIN_LONGITUDE || lon > MAX_LONGITUDE) {
        PR_ERR("[GPS_CALC] Invalid longitude: %.6f (must be -180 to 180)", lon);
        return false;
    }
    
    return true;
}

/**
 * @brief Normalize angle to 0-360 degree range
 * 
 * @param angle_deg Angle in degrees
 * @return Normalized angle in 0-360 range
 */
static float normalize_angle_0_360(float angle_deg)
{
    /* Reduce angle to 0-360 range */
    angle_deg = fmodf(angle_deg, 360.0f);
    
    /* Handle negative angles */
    if (angle_deg < 0.0f) {
        angle_deg += 360.0f;
    }
    
    return angle_deg;
}

/**
 * @brief Calculate distance between two GPS coordinates using Haversine formula
 */
float app_gps_calc_distance(float tracker_lat, float tracker_lon, 
                            float target_lat, float target_lon)
{
    /* Validate input coordinates */
    if (!validate_gps_coordinates(tracker_lat, tracker_lon) ||
        !validate_gps_coordinates(target_lat, target_lon)) {
        return -1.0f;
    }
    
    /* Convert degrees to radians */
    float lat1_rad = DEG_TO_RAD(tracker_lat);
    float lon1_rad = DEG_TO_RAD(tracker_lon);
    float lat2_rad = DEG_TO_RAD(target_lat);
    float lon2_rad = DEG_TO_RAD(target_lon);
    
    /* Calculate differences */
    float dlat = lat2_rad - lat1_rad;
    float dlon = lon2_rad - lon1_rad;
    
    /* Haversine formula:
     * a = sin²(Δφ/2) + cos(φ1)·cos(φ2)·sin²(Δλ/2)
     * c = 2·atan2(√a, √(1-a))
     * d = R·c
     */
    float sin_dlat_2 = sinf(dlat / 2.0f);
    float sin_dlon_2 = sinf(dlon / 2.0f);
    
    float a = sin_dlat_2 * sin_dlat_2 + 
              cosf(lat1_rad) * cosf(lat2_rad) * sin_dlon_2 * sin_dlon_2;
    
    /* Clamp 'a' to [0, 1] to avoid numerical errors with asin/acos */
    if (a < 0.0f) a = 0.0f;
    if (a > 1.0f) a = 1.0f;
    
    float c = 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));
    float distance = EARTH_RADIUS_METERS * c;
    
    PR_DEBUG("[GPS_CALC] Distance: %.2f meters (from %.6f,%.6f to %.6f,%.6f)",
             distance, tracker_lat, tracker_lon, target_lat, target_lon);
    
    return distance;
}

/**
 * @brief Calculate bearing angle from tracker to target
 */
float app_gps_calc_bearing(float tracker_lat, float tracker_lon,
                           float target_lat, float target_lon)
{
    /* Validate input coordinates */
    if (!validate_gps_coordinates(tracker_lat, tracker_lon) ||
        !validate_gps_coordinates(target_lat, target_lon)) {
        return -1.0f;
    }
    
    /* Convert degrees to radians */
    float lat1_rad = DEG_TO_RAD(tracker_lat);
    float lon1_rad = DEG_TO_RAD(tracker_lon);
    float lat2_rad = DEG_TO_RAD(target_lat);
    float lon2_rad = DEG_TO_RAD(target_lon);
    
    /* Calculate longitude difference */
    float dlon = lon2_rad - lon1_rad;
    
    /* Forward azimuth formula:
     * θ = atan2(sin(Δλ)·cos(φ2), cos(φ1)·sin(φ2) - sin(φ1)·cos(φ2)·cos(Δλ))
     * 
     * This gives the initial bearing (forward azimuth) from point 1 to point 2.
     * 
     * Note: We use atan2(y, x) where:
     * - y = sin(Δλ)·cos(φ2)
     * - x = cos(φ1)·sin(φ2) - sin(φ1)·cos(φ2)·cos(Δλ)
     */
    float sin_dlon = sinf(dlon);
    float cos_dlon = cosf(dlon);
    float cos_lat1 = cosf(lat1_rad);
    float sin_lat1 = sinf(lat1_rad);
    float cos_lat2 = cosf(lat2_rad);
    float sin_lat2 = sinf(lat2_rad);
    
    float y = sin_dlon * cos_lat2;
    float x = cos_lat1 * sin_lat2 - sin_lat1 * cos_lat2 * cos_dlon;
    
    /* Calculate bearing in radians */
    float bearing_rad = atan2f(y, x);
    
    /* Convert to degrees */
    float bearing_deg = RAD_TO_DEG(bearing_rad);
    
    /* Normalize to 0-360 range (0=North, 90=East, 180=South, 270=West) */
    bearing_deg = normalize_angle_0_360(bearing_deg);
    
    PR_DEBUG("[GPS_CALC] Bearing: %.2f degrees (from %.6f,%.6f to %.6f,%.6f)",
             bearing_deg, tracker_lat, tracker_lon, target_lat, target_lon);
    
    return bearing_deg;
}

/**
 * @brief Calculate both distance and bearing in one call (optimized)
 */
OPERATE_RET app_gps_calc_distance_and_bearing(float tracker_lat, float tracker_lon,
                                               float target_lat, float target_lon,
                                               app_gps_calc_result_t *result)
{
    if (!result) {
        return OPRT_INVALID_PARM;
    }
    
    /* Initialize result */
    result->distance_meters = 0.0f;
    result->bearing_degrees = 0.0f;
    result->valid = false;
    
    /* Validate input coordinates */
    if (!validate_gps_coordinates(tracker_lat, tracker_lon) ||
        !validate_gps_coordinates(target_lat, target_lon)) {
        return OPRT_INVALID_PARM;
    }
    
    /* Convert degrees to radians */
    float lat1_rad = DEG_TO_RAD(tracker_lat);
    float lon1_rad = DEG_TO_RAD(tracker_lon);
    float lat2_rad = DEG_TO_RAD(target_lat);
    float lon2_rad = DEG_TO_RAD(target_lon);
    
    /* Calculate differences */
    float dlat = lat2_rad - lat1_rad;
    float dlon = lon2_rad - lon1_rad;
    
    /* Pre-calculate trigonometric values (reused for both distance and bearing) */
    float cos_lat1 = cosf(lat1_rad);
    float sin_lat1 = sinf(lat1_rad);
    float cos_lat2 = cosf(lat2_rad);
    float sin_lat2 = sinf(lat2_rad);
    float sin_dlat_2 = sinf(dlat / 2.0f);
    float sin_dlon_2 = sinf(dlon / 2.0f);
    float sin_dlon = sinf(dlon);
    float cos_dlon = cosf(dlon);
    
    /* ===== Calculate Distance (Haversine) ===== */
    float a = sin_dlat_2 * sin_dlat_2 + 
              cos_lat1 * cos_lat2 * sin_dlon_2 * sin_dlon_2;
    
    /* Clamp 'a' to [0, 1] to avoid numerical errors */
    if (a < 0.0f) a = 0.0f;
    if (a > 1.0f) a = 1.0f;
    
    float c = 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));
    result->distance_meters = EARTH_RADIUS_METERS * c;
    
    /* ===== Calculate Bearing (Forward Azimuth) ===== */
    float y = sin_dlon * cos_lat2;
    float x = cos_lat1 * sin_lat2 - sin_lat1 * cos_lat2 * cos_dlon;
    
    float bearing_rad = atan2f(y, x);
    float bearing_deg = RAD_TO_DEG(bearing_rad);
    result->bearing_degrees = normalize_angle_0_360(bearing_deg);
    
    /* Mark result as valid */
    result->valid = true;
    
    PR_DEBUG("[GPS_CALC] Distance: %.2f m, Bearing: %.2f° (from %.6f,%.6f to %.6f,%.6f)",
             result->distance_meters, result->bearing_degrees,
             tracker_lat, tracker_lon, target_lat, target_lon);
    
    return OPRT_OK;
}

