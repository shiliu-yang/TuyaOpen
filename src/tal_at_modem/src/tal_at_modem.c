/**
 * @file tal_at_modem.c
 * @brief tal_at_modem module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tal_at_modem.h"

#include "at_client.h"
#include "at_module_ml307r.h"

#include "tdl_transport_manage.h"

#include "tal_api.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TAL_AT_MODEM_TYPE_T type;

    TDL_TRANSPORT_HANDLE transport_hdl;

    uint8_t at_ready;
    uint8_t sim_ready;
    uint8_t network_ready;

    AT_MODULE_OPS_T ops;
} TAL_AT_MODEM_CFG_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static TAL_AT_MODEM_CFG_T sg_at_modem = {
    .type = TAL_AT_MODEM_TYPE_ML307R, // Default modem type
    .transport_hdl = NULL,
    .at_ready = 0,
    .sim_ready = 0,
    .network_ready = 0,
};
/***********************************************************
***********************function define**********************
***********************************************************/

static void __at_modem_urc_cb(TAL_AT_MODEM_CMD_T cmd, void *args)
{
    switch (cmd) {
    case (TAL_AT_MODEM_CMD_READY): {
        if (!args) {
            return;
        }

        sg_at_modem.at_ready = (*(uint8_t *)(args)) == 1 ? 1 : 0;

        PR_DEBUG("at ready: %d", sg_at_modem.at_ready);
    } break;
    case (TAL_AT_MODEM_CMD_SIM): {
        if (!args) {
            return;
        }

        sg_at_modem.sim_ready = (*(uint8_t *)(args)) == 1 ? 1 : 0;

        PR_DEBUG("sim ready: %d", sg_at_modem.sim_ready);
    } break;
    case (TAL_AT_MODEM_CMD_NETWORK): {
        if (!args) {
            return;
        }

        sg_at_modem.network_ready = (*(uint8_t *)(args)) == 1 ? 1 : 0;

        PR_DEBUG("network read: %d", sg_at_modem.network_ready);
    } break;
    }

    return;
}

OPERATE_RET tal_at_modem_init(const char *transport_name, TAL_AT_MODEM_TYPE_T type)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(transport_name, OPRT_INVALID_PARM);

    TUYA_CALL_ERR_RETURN(tdl_transport_find(transport_name, &sg_at_modem.transport_hdl));
    TUYA_CALL_ERR_RETURN(tdl_transport_open(sg_at_modem.transport_hdl));

    TUYA_CALL_ERR_RETURN(at_client_init(sg_at_modem.transport_hdl));

    // init 4G module
    if (type == TAL_AT_MODEM_TYPE_ML307R) {
        // Initialize ML307R specific settings
        TUYA_CALL_ERR_RETURN(at_module_ml307r_init(&sg_at_modem.ops, __at_modem_urc_cb));
    }

    // AT
    if (0 == sg_at_modem.ops.at_check()) {
        // TODO: wait ok?
        return OPRT_COM_ERROR;
    }

    return OPRT_OK;
}
