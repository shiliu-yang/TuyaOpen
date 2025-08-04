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
};
/***********************************************************
***********************function define**********************
***********************************************************/

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
        TUYA_CALL_ERR_RETURN(at_module_ml307r_init());
    }

    return OPRT_OK;
}
