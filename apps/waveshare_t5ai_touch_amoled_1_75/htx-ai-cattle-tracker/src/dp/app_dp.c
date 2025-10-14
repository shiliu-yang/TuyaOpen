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

#if defined(ENABLE_BATTERY) && (ENABLE_BATTERY == 1)
#include "app_battery.h"
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define APP_DPID_BATTERY_PERCENTAGE 2
#define APP_DPID_VOLUME             3
#define APP_DPID_CHARGE_STATUS      5

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

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
    default:
        PR_WARN("Unhandled DP ID: %d", id);
        break;
    }

    return;
}
