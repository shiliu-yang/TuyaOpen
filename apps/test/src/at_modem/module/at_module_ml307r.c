/**
 * @file at_module_ml307r.c
 * @brief at_module_ml307r module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "at_module_ml307r.h"

#include "at_client.h"
#include "at_utils.h"

#include "at_module.h"

#include "tal_api.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    AT_MODULE_CB cb;

    MUTEX_HANDLE mutex;
} AT_MODULE_CONTEXT_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static AT_MODULE_CONTEXT_T sg_ml307r_ctx = {0};

static AT_URC_T sg_ml307r_urc_handler[] = {
    {{NULL}, "+MATREADY", NULL, NULL},  {{NULL}, "+CEREG: ", NULL, NULL},    {{NULL}, "+CPIN: ", NULL, NULL},
    {{NULL}, "+MIPOPEN: ", NULL, NULL}, {{NULL}, "+MIPCLOSE: ", NULL, NULL}, {{NULL}, "+MDNSGIP: ", NULL, NULL},
    {{NULL}, "+MIPURC: ", NULL, NULL},
};

/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __ml307r_urc_register(void)
{
    OPERATE_RET rt = OPRT_OK;

    uint32_t urc_count = sizeof(sg_ml307r_urc_handler) / sizeof(AT_URC_T);

    for (uint32_t i = 0; i < urc_count; i++) {
        TUYA_CALL_ERR_RETURN(at_client_add_urc_handler(&sg_ml307r_urc_handler[i]));
    }

    PR_DEBUG("Registering ML307R URC handler successfully");

    return rt;
}

OPERATE_RET at_module_ml307r_init(AT_MODULE_OPS_T *ops, AT_MODULE_CB cb)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(ops, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(cb, OPRT_INVALID_PARM);

    if (sg_ml307r_ctx.mutex == NULL) {
        TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&sg_ml307r_ctx.mutex));
    }

    sg_ml307r_ctx.cb = cb;

    // register urc handler
    TUYA_CALL_ERR_RETURN(__ml307r_urc_register());

    PR_DEBUG("ML307R module initialized successfully");

    return rt;
}

OPERATE_RET at_module_ml307r_deinit(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL != sg_ml307r_ctx.mutex) {
        TUYA_CALL_ERR_RETURN(tal_mutex_release(sg_ml307r_ctx.mutex));
        sg_ml307r_ctx.mutex = NULL;
    }

    return rt;
}
