/**
 * @file app_dp.c
 * @brief app_dp module is used to handle the data points
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "app_dp.h"

#include "app_system_info.h"
#include "app_volume.h"
#include "app_ui_main.h"

#include "tal_api.h"

/***********************************************************
************************macro define************************
***********************************************************/
// DP ID
#define DP_ID_SWITCH 1
#define DP_ID_BATTERY_PERCENTAGE 2
#define DP_ID_VOLUME_SET 3
#define DP_ID_CHARGE_STATUS 5
#define DP_ID_GPS_SIGNAL_STRENGTH 101
#define DP_ID_SOS_STATE 102
#define DP_ID_GPS_HEIGHT 103
#define DP_ID_GPS_ACCURACY 104
#define DP_ID_GPS_POSITION 105
#define DP_ID_4G_SIGNAL_STRENGTH 106
#define DP_ID_GPS_TIME 107
#define DP_ID_GPS_DIRECTION 108
#define DP_ID_START_TRACKING_ID 109 

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/
// GPS position upload control
static double sg_last_latitude = 0.0;
static double sg_last_longitude = 0.0;
static SYS_TIME_T sg_last_gps_report_time = 0;

// GPS upload criteria
#define GPS_REPORT_MIN_INTERVAL_MS (5 * 60 * 1000) // 5 minutes
#define GPS_POSITION_DELTA_LAT     0.00003         // ~3.3 meters latitude change
#define GPS_POSITION_DELTA_LON     0.00004         // ~3.3 meters longitude change

// GPS position string buffer
static char sg_gps_position_buffer[32] = {0};

/***********************************************************
***********************function define**********************
***********************************************************/

OPERATE_RET app_dp_volume_upload(uint8_t volume)
{
    if (!app_network_is_ready()) {
        PR_WARN("network not ready, skip volume upload");
        return OPRT_COM_ERROR;
    }

    tuya_iot_client_t *client = tuya_iot_client_get();

    dp_obj_t dp_obj = {0};

    dp_obj.id = DP_ID_VOLUME_SET;
    dp_obj.type = PROP_VALUE;
    dp_obj.value.dp_value = volume;

    PR_DEBUG("DP upload volume:%d", volume);

    return tuya_iot_dp_obj_report(client, client->activate.devid, &dp_obj, 1, 0);
}

OPERATE_RET app_dp_sos_set(bool sos_status)
{
    if (!app_network_is_ready()) {
        PR_WARN("network not ready, skip sos status upload");
        return OPRT_COM_ERROR;
    }

    tuya_iot_client_t *client = tuya_iot_client_get();

    dp_obj_t dp_obj = {0};

    dp_obj.id = DP_ID_SOS_STATE;
    dp_obj.type = PROP_BOOL;
    dp_obj.value.dp_bool = sos_status;

    PR_DEBUG("DP upload sos status:%d", sos_status);

    return tuya_iot_dp_obj_report(client, client->activate.devid, &dp_obj, 1, DP_REPT_NO_FILTER_FLAG);
}

OPERATE_RET app_dp_switch_set(bool switch_status)
{
    if (!app_network_is_ready()) {
        PR_WARN("network not ready, skip switch status upload");
        return OPRT_COM_ERROR;
    }

    tuya_iot_client_t *client = tuya_iot_client_get();

    dp_obj_t dp_obj = {0};

    dp_obj.id = DP_ID_SWITCH;
    dp_obj.type = PROP_BOOL;
    dp_obj.value.dp_bool = switch_status;

    PR_DEBUG("DP upload switch status:%d", switch_status);

    return tuya_iot_dp_obj_report(client, client->activate.devid, &dp_obj, 1, DP_REPT_NO_FILTER_FLAG);
}

OPERATE_RET app_dp_battery_upload(uint8_t is_charging, uint8_t battery_percentage)
{
    if (!app_network_is_ready()) {
        PR_WARN("network not ready, skip battery upload");
        return OPRT_COM_ERROR;
    }

    tuya_iot_client_t *client = tuya_iot_client_get();

    dp_obj_t dp_obj[2] = {0};

    // upload battery percentage
    dp_obj[0].id = DP_ID_BATTERY_PERCENTAGE;
    dp_obj[0].type = PROP_VALUE;
    dp_obj[0].value.dp_value = battery_percentage;

    // upload charge status
    dp_obj[1].id =  DP_ID_CHARGE_STATUS;
    dp_obj[1].type = PROP_ENUM;
    dp_obj[1].value.dp_enum = is_charging ? 1 : 0;

    return tuya_iot_dp_obj_report(client, client->activate.devid, dp_obj, 2, 0);
}

/**
 * @brief Upload GPS position to cloud
 * 
 * Uploads GPS position under the following conditions:
 * - Latitude change ≥ 0.00003° (~3.3 meters)
 * - Longitude change ≥ 0.00004° (~3.3 meters)
 * - Force upload every 5 minutes
 * 
 * @param latitude Latitude in decimal degrees
 * @param longitude Longitude in decimal degrees
 * @return OPERATE_RET OPRT_OK on success
 */
OPERATE_RET app_dp_gps_position_upload(double latitude, double longitude)
{
    if (!app_network_is_ready()) {
        PR_WARN("network not ready, skip gps position upload");
        return OPRT_COM_ERROR;
    }

    /* Validate GPS coordinates */
    if (latitude == 0.0 && longitude == 0.0) {
        PR_WARN("GPS position invalid (0,0), skip upload");
        return OPRT_OK;
    }

    /* Check if position changed enough or time interval exceeded */
    SYS_TIME_T now = tal_time_get_posix_ms();
    
    double lat_diff = (latitude > sg_last_latitude) ? 
                      (latitude - sg_last_latitude) : (sg_last_latitude - latitude);
    double lon_diff = (longitude > sg_last_longitude) ? 
                      (longitude - sg_last_longitude) : (sg_last_longitude - longitude);
    
    PR_DEBUG("GPS diff - lat: %.6f, lon: %.6f, time: %u ms", 
             lat_diff, lon_diff, (unsigned int)(now - sg_last_gps_report_time));
    
    /* Check upload criteria */
    if (lat_diff < GPS_POSITION_DELTA_LAT && 
        lon_diff < GPS_POSITION_DELTA_LON &&
        (now - sg_last_gps_report_time) < GPS_REPORT_MIN_INTERVAL_MS) {
        PR_DEBUG("GPS position change too small or interval too short, skip upload");
        return OPRT_OK;
    }
    
    /* Update last position and time */
    sg_last_latitude = latitude;
    sg_last_longitude = longitude;
    sg_last_gps_report_time = now;
    
    /* Format GPS position string */
    memset(sg_gps_position_buffer, 0, sizeof(sg_gps_position_buffer));
    snprintf(sg_gps_position_buffer, sizeof(sg_gps_position_buffer), 
             "%.6f,%.6f", latitude, longitude);
    
    PR_INFO("DP upload GPS position: %s", sg_gps_position_buffer);
    
    /* Upload to cloud */
    tuya_iot_client_t *client = tuya_iot_client_get();
    
    dp_obj_t dp_obj = {0};
    dp_obj.id = DP_ID_GPS_POSITION;
    dp_obj.type = PROP_STR;
    dp_obj.value.dp_str = sg_gps_position_buffer;
    
    return tuya_iot_dp_obj_report(client, client->activate.devid, &dp_obj, 1, 0);
}

/**
 * @brief Upload GPS altitude to cloud
 * 
 * @param height_m Altitude in meters
 * @return OPERATE_RET OPRT_OK on success
 */
static int sg_last_gps_height = 0;
OPERATE_RET app_dp_gps_height_upload(int height_m)
{
    if (!app_network_is_ready()) {
        PR_WARN("network not ready, skip gps height upload");
        return OPRT_COM_ERROR;
    }

    if (height_m == sg_last_gps_height) {
        PR_DEBUG("GPS height not changed: %d m, skip upload", height_m);
        return OPRT_OK;
    }

    sg_last_gps_height = height_m;
    
    PR_DEBUG("DP upload GPS height: %d m", height_m);
    
    tuya_iot_client_t *client = tuya_iot_client_get();
    
    dp_obj_t dp_obj = {0};
    dp_obj.id = DP_ID_GPS_HEIGHT;
    dp_obj.type = PROP_VALUE;
    dp_obj.value.dp_value = height_m;
    
    return tuya_iot_dp_obj_report(client, client->activate.devid, &dp_obj, 1, 0);
}

void app_dp_process(uint8_t id, dp_prop_tp_t type, dp_value_t value)
{
    // all downlink dp will be handled here
    switch (id) {
    case DP_ID_SWITCH: {
        PR_DEBUG("DP_ID_SWITCH: %d", value.dp_value);
    } break;
    case DP_ID_VOLUME_SET: {
        PR_DEBUG("DP_ID_VOLUME_SET: %d", value.dp_value);
        app_volume_set(value.dp_value);
        app_ui_setting_volume_update(value.dp_value);
    } break;
    case DP_ID_START_TRACKING_ID: {
        PR_DEBUG("DP_ID_START_TRACKING_ID: %d", value.dp_value);
    } break;
    default: {
        PR_ERR("Invalid DP ID: %d", id);
    } break;
    }

    return;
}
