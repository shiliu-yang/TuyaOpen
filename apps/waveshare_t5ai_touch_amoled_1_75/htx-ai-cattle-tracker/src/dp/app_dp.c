/**
 * @file app_dp.c
 * @brief app_dp module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_config.h"
#include "app_dp.h"

#include "tal_api.h"
#include "ai_audio.h"
#include "netmgr.h"

#include "app_sos.h"

#if defined(ENABLE_BATTERY) && (ENABLE_BATTERY == 1)
#include "app_battery.h"
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define APP_DPID_BATTERY_PERCENTAGE 2
#define APP_DPID_VOLUME             3
#define APP_DPID_CHARGE_STATUS      5

#define APP_DPID_SOS_STATUS 102

#define APP_DPID_GPS_POSITION 105

// start_tracking_id
#define APP_DPID_START_TRACKING 109

// GPS upload criteria
#define GPS_REPORT_MIN_INTERVAL_MS (5 * 60 * 1000) // 5 minutes
#define GPS_POSITION_DELTA_LAT     0.00003         // Latitude change threshold
#define GPS_POSITION_DELTA_LON     0.00004         // Longitude change threshold

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
// gps
static double s_last_latitude = 0.0;
static double s_last_longitude = 0.0;
static SYS_TIME_T s_last_gps_report_time = 0;

// tracking id
static uint8_t s_tracking_id = 1;

/***********************************************************
***********************function define**********************
***********************************************************/

bool app_check_network_ready(void)
{
    tuya_iot_client_t *client = tuya_iot_client_get();

    // check client and activation status
    if (client == NULL || client->is_activated == false) {
        PR_ERR("device not activated");
        return false;
    }

    netmgr_status_e status = NETMGR_LINK_DOWN;
    netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_STATUS, &status);
    return status == NETMGR_LINK_DOWN ? false : true;
}

OPERATE_RET app_volume_upload(uint8_t volume)
{
    if (!app_check_network_ready()) {
        PR_WARN("network not ready, skip volume upload");
        return OPRT_COM_ERROR;
    }

    tuya_iot_client_t *client = tuya_iot_client_get();

    dp_obj_t dp_obj = {0};

    dp_obj.id = APP_DPID_VOLUME;
    dp_obj.type = PROP_VALUE;
    dp_obj.value.dp_value = volume;

    PR_DEBUG("DP upload volume:%d", volume);

    return tuya_iot_dp_obj_report(client, client->activate.devid, &dp_obj, 1, 0);
}

char gps_buffer[32] = {0};

OPERATE_RET app_gps_position_upload(double latitude, double longitude)
{

    if (!app_check_network_ready()) {
        PR_WARN("network not ready, skip gps position upload");
        return OPRT_COM_ERROR;
    }

    // Report under the following conditions:
    // Latitude change: ≥0.00003°
    // Longitude change: ≥0.00004°
    // Force report every 5 minutes
    SYS_TIME_T now = tal_time_get_posix_ms();
    if (latitude == 0.0 && longitude == 0.0) {
        // Not get any gps position
        PR_WARN("gps position invalid, skip upload");
        return OPRT_OK;
    }

    double lat_diff = latitude > s_last_latitude ? (latitude - s_last_latitude) : (s_last_latitude - latitude);
    double lon_diff = longitude > s_last_longitude ? (longitude - s_last_longitude) : (s_last_longitude - longitude);

#if defined(LC76G_ENABLE_NMEA_LOGS) && (LC76G_ENABLE_NMEA_LOGS == 1)
    PR_DEBUG("lat_diff: %.6f, lon_diff: %.6f, time_diff: %u ms", lat_diff, lon_diff,
             (unsigned int)(now - s_last_gps_report_time));
#endif

    if (lat_diff >= GPS_POSITION_DELTA_LAT || lon_diff >= GPS_POSITION_DELTA_LON ||
        (now - s_last_gps_report_time) >= GPS_REPORT_MIN_INTERVAL_MS) {
        s_last_latitude = latitude;
        s_last_longitude = longitude;
        s_last_gps_report_time = now;
    } else {
#if defined(LC76G_ENABLE_NMEA_LOGS) && (LC76G_ENABLE_NMEA_LOGS == 1)
        PR_DEBUG("gps position change too small or report interval too short, skip upload");
#endif
        return OPRT_OK;
    }

    memset(gps_buffer, 0, sizeof(gps_buffer));
    snprintf(gps_buffer, sizeof(gps_buffer), "%.6f,%.6f", latitude, longitude);
    PR_DEBUG("DP upload gps position: %s", gps_buffer);

    tuya_iot_client_t *client = tuya_iot_client_get();

    dp_obj_t dp_obj = {0};

    dp_obj.id = APP_DPID_GPS_POSITION;
    dp_obj.type = PROP_STR;
    dp_obj.value.dp_str = gps_buffer;

    return tuya_iot_dp_obj_report(client, client->activate.devid, &dp_obj, 1, 0);
}

OPERATE_RET app_dp_battery_upload(uint8_t is_charging, uint8_t battery_percentage)
{
    if (!app_check_network_ready()) {
        PR_WARN("network not ready, skip battery upload");
        return OPRT_COM_ERROR;
    }

    tuya_iot_client_t *client = tuya_iot_client_get();

    dp_obj_t dp_obj[2] = {0};

    // upload battery percentage
    dp_obj[0].id = APP_DPID_BATTERY_PERCENTAGE;
    dp_obj[0].type = PROP_VALUE;
    dp_obj[0].value.dp_value = battery_percentage;

    // upload charge status
    dp_obj[1].id = APP_DPID_CHARGE_STATUS;
    dp_obj[1].type = PROP_ENUM;
    dp_obj[1].value.dp_enum = is_charging ? 1 : 0;

    return tuya_iot_dp_obj_report(client, client->activate.devid, dp_obj, 2, 0);
}

OPERATE_RET app_volume_set(uint8_t volume)
{
    OPERATE_RET rt = OPRT_OK;

    if (volume > 100) {
        PR_WARN("volume param err: %d", volume);
        volume = 100;
    }

    if (volume < 0) {
        PR_WARN("volume param err: %d", volume);
        volume = 0;
    }

    TUYA_CALL_ERR_RETURN(ai_audio_set_volume(volume));
    app_volume_upload(volume);

    return rt;
}

OPERATE_RET app_dp_sos_set(bool sos_status)
{
    if (!app_check_network_ready()) {
        PR_WARN("network not ready, skip sos status upload");
        return OPRT_COM_ERROR;
    }

    tuya_iot_client_t *client = tuya_iot_client_get();

    dp_obj_t dp_obj = {0};

    dp_obj.id = APP_DPID_SOS_STATUS;
    dp_obj.type = PROP_BOOL;
    dp_obj.value.dp_bool = sos_status;

    PR_DEBUG("DP upload sos status:%d", sos_status);

    return tuya_iot_dp_obj_report(client, client->activate.devid, &dp_obj, 1, DP_REPT_NO_FILTER_FLAG);
}

void app_dp_update_all(void)
{
    app_volume_upload(ai_audio_get_volume());

#if defined(ENABLE_BATTERY) && (ENABLE_BATTERY == 1)
    app_battery_status_refresh();
#endif

    return;
}

void app_dp_process(uint8_t id, dp_prop_tp_t type, dp_value_t value)
{
    switch (id) {
    case APP_DPID_VOLUME: {
        app_volume_set(value.dp_value);
    } break;
    case APP_DPID_START_TRACKING: {
        s_tracking_id = value.dp_value;
    } break;
    case APP_DPID_SOS_STATUS: {
        app_sos_set(value.dp_bool);
    } break;
    default:
        PR_WARN("Unhandled DP ID: %d", id);
        break;
    }

    return;
}

uint8_t app_get_current_tracking_id(void)
{
    return s_tracking_id;
}
