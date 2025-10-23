/**
 * @file netconn_cellular.c
 * @brief netconn_cellular module is used to manage cellular network connections.
 *
 * This file provides the implementation of the netconn_cellular module,
 * which is responsible for managing cellular network connections.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 * 2025-07-10   yangjie     Initial version.
 */

#include "netconn_cellular.h"

#if defined(ENABLE_CELLULAR) && (ENABLE_CELLULAR == 1)
#include "tal_api.h"
#include "netmgr.h"
#include "tal_cellular.h"
#include "mqtt_bind.h"

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
netmgr_conn_cellular_t s_netmgr_cellular = {
    .base =
        {
            .pri = 0,
            .type = NETCONN_CELLULAR,
#if (defined(ENABLE_LIBLWIP) && (ENABLE_LIBLWIP == 1)) || 100 == OPERATING_SYSTEM
            .card_type = TAL_NET_TYPE_POSIX,
#else
            .card_type = TAL_NET_TYPE_PLATFORM,
#endif
            .status = NETMGR_LINK_DOWN,
            .open = netconn_cellular_open,
            .close = netconn_cellular_close,
            .get = netconn_cellular_get,
            .set = netconn_cellular_set,
        },
};

static uint8_t s_cellular_initialized = 0;

/***********************************************************
***********************function define**********************
***********************************************************/
static void __netconn_cellular_event(CELLULAR_STAT_E event)
{
    netmgr_conn_cellular_t *netmgr_cellular = &s_netmgr_cellular;

    if ((event == TAL_CELLULAR_LINK_UP && netmgr_cellular->base.status == NETMGR_LINK_UP) ||
        (event == TAL_CELLULAR_LINK_DOWN && netmgr_cellular->base.status == NETMGR_LINK_DOWN)) {
        // no change
        return;
    }

    PR_NOTICE("cellular status changed to %d, old stat: %d", event, netmgr_cellular->base.status);
    netmgr_cellular->base.status = (event == TAL_CELLULAR_LINK_UP) ? NETMGR_LINK_UP : NETMGR_LINK_DOWN;

    // notify netmgr
    if (netmgr_cellular->base.event_cb) {
        netmgr_cellular->base.event_cb(NETCONN_CELLULAR, netmgr_cellular->base.status);
    }

    return;
}

static TIMER_ID s_cellular_timer;

static void __cellular_init(void)
{
    s_cellular_initialized = 1;
    TAL_CELLULAR_BASE_CFG_T cfg;
    memset(&cfg, 0, sizeof(cfg));
    strcpy(cfg.apn, "");
    tal_cellular_init(&cfg);
    PR_DEBUG("cellular initialized");
}

void __cellular_timer_cb(TIMER_ID timer_id, void *arg)
{
    PR_DEBUG("cellular init timer cb, is activated: %d", tuya_iot_client_get()->is_activated);
    if (tuya_iot_client_get()->is_activated == false) {
        return;
    }

    __cellular_init();

    tal_sw_timer_delete(s_cellular_timer);
    s_cellular_timer = NULL;

    return;
}

static int _activated_timer_count = 0;

void __cellular_activated_timer_cb(TIMER_ID timer_id, void *arg)
{
    _activated_timer_count++;

    netmgr_status_e status;

    netmgr_conn_get(NETCONN_WIFI, NETCONN_CMD_STATUS, &status);

    if (status == NETMGR_LINK_UP) {
        _activated_timer_count = 0;
        tal_sw_timer_delete(s_cellular_timer);
        s_cellular_timer = NULL;
        PR_DEBUG("cellular activated timer cb, wifi connected, skip cellular init");
        return;
    }

    if (_activated_timer_count < 10) {
        return;
    }

    __cellular_init();
}

OPERATE_RET netconn_cellular_open(void *config)
{
    OPERATE_RET rt = OPRT_OK;

    netmgr_conn_cellular_t *netmgr_cellular = &s_netmgr_cellular;

    // init
    // if device is actived, cellular init
    // else wait for active
    PR_DEBUG("cellular open, is activated: %d", tuya_iot_client_get()->is_activated);
    if (tuya_iot_client_get()->is_activated) {
        tal_sw_timer_create((TAL_TIMER_CB)__cellular_activated_timer_cb, NULL, &s_cellular_timer);
        if (s_cellular_timer == NULL) {
            __cellular_init();
            PR_ERR("cellular timer create failed");
            return OPRT_COM_ERROR;
        }
        tal_sw_timer_start(s_cellular_timer, 1000, TAL_TIMER_CYCLE);
    } else {
        tal_sw_timer_create((TAL_TIMER_CB)__cellular_timer_cb, NULL, &s_cellular_timer);
        if (s_cellular_timer == NULL) {
            PR_ERR("cellular timer create failed");
            return OPRT_COM_ERROR;
        }
        tal_sw_timer_start(s_cellular_timer, 3000, TAL_TIMER_CYCLE);
    }

    netmgr_cellular->base.status = NETMGR_LINK_DOWN;

    TUYA_CALL_ERR_RETURN(tal_cellular_set_status_cb(__netconn_cellular_event));

    tuya_iot_token_get_port_register(tuya_iot_client_get(), mqtt_bind_token_get);

    return rt;
}

OPERATE_RET netconn_cellular_close(void)
{
    OPERATE_RET rt = OPRT_OK;

    return rt;
}

OPERATE_RET netconn_cellular_set(netmgr_conn_config_type_e cmd, void *param)
{
    OPERATE_RET rt = OPRT_OK;

    netmgr_conn_cellular_t *netmgr_cellular = &s_netmgr_cellular;

    switch (cmd) {
    case NETCONN_CMD_PRI: {
        netmgr_cellular->base.pri = *(int *)param;
        netmgr_cellular->base.event_cb(NETCONN_CELLULAR, netmgr_cellular->base.status);
    } break;
    default: {
        rt = OPRT_NOT_SUPPORTED;
    } break;
    }

    return rt;
}

OPERATE_RET netconn_cellular_get(netmgr_conn_config_type_e cmd, void *param)
{
    OPERATE_RET rt = OPRT_OK;

    netmgr_conn_cellular_t *netmgr_cellular = &s_netmgr_cellular;

    switch (cmd) {
    case NETCONN_CMD_PRI: {
        *(int *)param = netmgr_cellular->base.pri;
    } break;
    case NETCONN_CMD_STATUS: {
        // PR_DEBUG("cellular initialized: %d", s_cellular_initialized);
        // PR_DEBUG("cellular status: %d", netmgr_cellular->base.status);
        if (!s_cellular_initialized && netmgr_cellular->base.status == NETMGR_LINK_DOWN) {
            *(netmgr_status_e *)param = NETMGR_LINK_DOWN;
            return OPRT_OK;
        }
        *(netmgr_status_e *)param = netmgr_cellular->base.status;
    } break;
    case NETCONN_CMD_IP: {
        if (!s_cellular_initialized) {
            NW_IP_S *ip = (NW_IP_S *)param;
            memset(ip->ip, 0, sizeof(ip->ip));
            return OPRT_OK;
        }

        TUYA_CALL_ERR_RETURN(tal_cellular_get_ip((NW_IP_S *)param));
    } break;
    case NETCONN_CMD_MAC: {
        rt = OPRT_NOT_SUPPORTED;
    } break;
    default: {
        rt = OPRT_NOT_SUPPORTED;
    } break;
    }

    return rt;
}

#endif // defined(ENABLE_CELLULAR) && (ENABLE_CELLULAR == 1)