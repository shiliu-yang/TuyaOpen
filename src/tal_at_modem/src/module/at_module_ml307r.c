/**
 * @file at_module_ml307r.c
 * @brief at_module_ml307r module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "at_module_ml307r.h"

#include "at_client.h"

#include "tal_api.h"

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
static AT_URC_T sg_ml307r_urc_handler[] = {
    {{NULL}, AT_RSP_MATCH_PREFIX, "+MATREADY", NULL, NULL},
    {{NULL}, AT_RSP_MATCH_PREFIX, "+CEREG: ", NULL, NULL},
    {{NULL}, AT_RSP_MATCH_PREFIX, "+CPIN: ", NULL, NULL},
};
/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __ml307r_urc_register(void)
{
    OPERATE_RET rt = OPRT_OK;

    uint32_t urc_count = sizeof(sg_ml307r_urc_handler) / sizeof(AT_URC_T);

    for (uint32_t i = 0; i < urc_count; i++) {
        TUYA_CALL_ERR_RETURN(at_client_urc_handler_register(&sg_ml307r_urc_handler[i]));
    }

    PR_DEBUG("Registering ML307R URC handler successfully");

    return rt;
}

OPERATE_RET at_module_ml307r_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    // register urc handler
    TUYA_CALL_ERR_RETURN(__ml307r_urc_register());

    // Send AT

    // Get rsp OK

    PR_DEBUG("ML307R module initialized successfully");

    return rt;
}
