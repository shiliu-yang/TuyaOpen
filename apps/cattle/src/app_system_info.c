/**
 * @file app_system_info.c
 * @brief app_system_info module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "app_system_info.h"

#include "tuya_iot.h"
#include "netmgr.h"
#include "tal_api.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define FREE_HEAP_TM      (10 * 1000)

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TIMER_ID heap_tm;
} APP_SYSTEM_INFO_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static APP_SYSTEM_INFO_T system_info = {0};

/***********************************************************
***********************function define**********************
***********************************************************/
static void __app_free_heap_tm_cb(TIMER_ID timer_id, void *arg)
{
    PR_INFO("Free heap size:%d, psram: %d", tal_system_get_free_heap_size(), tal_system_get_psram_free_heap_size());
}

void app_system_info(void)
{
    // Free heap size
    tal_sw_timer_create(__app_free_heap_tm_cb, NULL, &system_info.heap_tm);
    tal_sw_timer_start(system_info.heap_tm, FREE_HEAP_TM, TAL_TIMER_CYCLE);

    return;
}

bool app_network_is_ready(void)
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