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
static void __urc_matready_cb(char *data, uint32_t len);
static void __urc_cereg_cb(char *data, uint32_t len);
static void __urc_cpin_cb(char *data, uint32_t len);

/***********************************************************
***********************variable define**********************
***********************************************************/
static AT_URC_T sg_ml307r_urc_handler[] = {
    {{NULL}, "+MATREADY", NULL, __urc_matready_cb},
    {{NULL}, "+CEREG: ", NULL, __urc_cereg_cb},
    {{NULL}, "+CPIN: ", NULL, __urc_cpin_cb},
};
/***********************************************************
***********************function define**********************
***********************************************************/
static void __urc_matready_cb(char *data, uint32_t len)
{
    PR_DEBUG("URC +MATREADY received: %.*s", len, data);
    return;
}

static void __urc_cereg_cb(char *data, uint32_t len)
{
    PR_DEBUG("URC +CEREG received: %.*s", len, data);
    return;
}

static void __urc_cpin_cb(char *data, uint32_t len)
{
    PR_DEBUG("URC +CPIN received: %.*s", len, data);
    return;
}

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
