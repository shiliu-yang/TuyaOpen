/**
 * @file app_ui_func.c
 * @brief app_ui_func module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "app_ui_func.h"

#include "app_volume.h"
#include "app_ui_main.h"
#include "app_battery.h"

#include "tal_api.h"
#include "tal_event_info.h"
#include "netmgr.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static TIMER_ID setting_load_timer_hdl = NULL;

static uint8_t setting_last_volume = 255;

static POSIX_TM_S setting_last_tm = {0};

static bool network_event_subscribed = false;

static bool volume_synced = false;

static bool battery_synced = false;

/***********************************************************
***********************function define**********************
***********************************************************/
/*  UI Setting Screen Function Start  */

static void __setting_get_date_time(void)
{
    OPERATE_RET rt = OPRT_OK;
    POSIX_TM_S tm = {0};

    TUYA_CALL_ERR_LOG(tal_time_get_local_time_custom(0, &tm));
    // compare tm and setting_last_tm
    if (tm.tm_year == setting_last_tm.tm_year && \
        tm.tm_mon == setting_last_tm.tm_mon && \
        tm.tm_mday == setting_last_tm.tm_mday && \
        tm.tm_hour == setting_last_tm.tm_hour && \
        tm.tm_min == setting_last_tm.tm_min && \
        tm.tm_sec == setting_last_tm.tm_sec) {
        return;
    }
    memcpy(&setting_last_tm, &tm, sizeof(POSIX_TM_S));

    // PR_DEBUG("__setting_get_date_time: tm: %04d/%02d/%02d %02d:%02d:%02d", tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

    char date[16] = {0};
    char time[16] = {0};
    snprintf(date, sizeof(date), "%04d/%02d/%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    snprintf(time, sizeof(time), "%02d:%02d", tm.tm_hour, tm.tm_min);
    app_ui_setting_date_time_update(date, time);
    return;
}

/**
 * @brief Network link type/status change event callback
 * @param data Event data (netmgr_type_e or netmgr_status_e)
 * @return OPERATE_RET
 */
static OPERATE_RET __network_link_change_cb(void *data)
{
    PR_DEBUG("Network link changed - updating UI");
    
    extern netmgr_type_e __get_active_conn();
    netmgr_type_e net_type = __get_active_conn();
    netmgr_status_e net_status = NETMGR_LINK_DOWN;

    if (net_type == NETCONN_CELLULAR) {
        // TODO: Handle cellular network
    } else if (net_type == NETCONN_WIFI) {
        if (netmgr_conn_get(NETCONN_WIFI, NETCONN_CMD_STATUS, &net_status) == OPRT_OK) {
            app_ui_setting_network_update(1, net_status);
        } else {
            app_ui_setting_network_update(1, NETMGR_LINK_DOWN);
        }
    } else if (net_type == NETCONN_AUTO) {
        // Network is not registered
        app_ui_setting_network_update(1, NETMGR_LINK_DOWN);
    } else {
        // Nothing to do
    }

    return OPRT_OK;
}

static void __setting_battery_update(void)
{
    // update battery
    uint8_t percentage = 0;
    bool is_charging = false;
    app_battery_get_status(&percentage, &is_charging);
    app_ui_setting_battery_update(percentage, is_charging);
    return;
}

void setting_load_timer_cb(TIMER_ID timer_id, void *arg)
{
    OPERATE_RET rt = OPRT_OK;

    // Update gps
    // update datetime
    __setting_get_date_time();

    // Update battery, only once
    if (!battery_synced) {
        __setting_battery_update();
        battery_synced = true;
    }
    
    /* Sync volume from hardware to UI (only once) */
    if (!volume_synced) {
        uint8_t volume = 0;
        app_volume_get(&volume);
        app_ui_setting_volume_update(volume);
        setting_last_volume = volume;
        volume_synced = true;
        PR_INFO("Volume synced to UI: %d", volume);
    }

    /* Subscribe to network events (only once) */
    if (!network_event_subscribed) {
        /* Trigger initial network update only once after subscription */
        __network_link_change_cb(NULL);

        rt = tal_event_subscribe(EVENT_LINK_TYPE_CHG, "ui_setting", __network_link_change_cb, SUBSCRIBE_TYPE_NORMAL);
        if (rt == OPRT_OK) {
            PR_INFO("Network link type change event subscribed");
        } else {
            PR_ERR("Failed to subscribe EVENT_LINK_TYPE_CHG: %d", rt);
        }
        
        rt = tal_event_subscribe(EVENT_LINK_STATUS_CHG, "ui_setting", __network_link_change_cb, SUBSCRIBE_TYPE_NORMAL);
        if (rt == OPRT_OK) {
            PR_INFO("Network link status change event subscribed");
        } else {
            PR_ERR("Failed to subscribe EVENT_LINK_STATUS_CHG: %d", rt);
        }
        
        network_event_subscribed = true;
    }
    
    PR_DEBUG("Setting screen load timer callback");
}

void app_ui_func_setting_load_callback(void)
{
    PR_DEBUG("Setting screen loaded");

    if (setting_load_timer_hdl == NULL) {
        tal_sw_timer_create(setting_load_timer_cb, NULL, &setting_load_timer_hdl);
    }
    tal_sw_timer_start(setting_load_timer_hdl, 5 * 1000, TAL_TIMER_CYCLE);
    tal_sw_timer_trigger(setting_load_timer_hdl);
}

void app_ui_func_setting_update_trigger(void)
{
    if (setting_load_timer_hdl != NULL) {
        tal_sw_timer_trigger(setting_load_timer_hdl);
    }
}

void app_ui_func_setting_unload_callback(void)
{
    PR_DEBUG("Setting screen unloaded");
    if (setting_load_timer_hdl != NULL) {
        tal_sw_timer_stop(setting_load_timer_hdl);
    }
}
/*  UI Setting Screen Function End  */