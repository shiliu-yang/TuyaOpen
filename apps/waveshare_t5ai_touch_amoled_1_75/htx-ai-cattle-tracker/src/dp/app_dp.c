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

bool app_dp_check_network_ready(void)
{
    netmgr_status_e status = NETMGR_LINK_DOWN;
    netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_STATUS, &status);
    return status == NETMGR_LINK_DOWN ? false : true;
}

OPERATE_RET app_volume_upload(uint8_t volume)
{
    netmgr_status_e status = NETMGR_LINK_DOWN;
    netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_STATUS, &status);

    tuya_iot_client_t *client = tuya_iot_client_get();

    // check client and activation status
    if (client == NULL || client->is_activated == false) {
        PR_ERR("client not ready");
        return OPRT_COM_ERROR;
    }

    // check network status
    if (app_dp_check_network_ready() == false) {
        PR_ERR("network not ready");
        return OPRT_COM_ERROR;
    }

    dp_obj_t dp_obj = {0};

    dp_obj.id = APP_DPID_VOLUME;
    dp_obj.type = PROP_VALUE;
    dp_obj.value.dp_value = volume;

    PR_DEBUG("DP upload volume:%d", volume);

    return tuya_iot_dp_obj_report(client, client->activate.devid, &dp_obj, 1, 0);
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
    return;
}

void app_dp_process(uint8_t id, dp_prop_tp_t type, dp_value_t value)
{
    switch (id) {
    case APP_DPID_BATTERY_PERCENTAGE: {
    } break;
    case APP_DPID_VOLUME: {
        app_volume_set(value.dp_value);
    } break;
    case APP_DPID_CHARGE_STATUS: {
    } break;
    default:
        PR_WARN("Unhandled DP ID: %d", id);
        break;
    }

    return;
}
