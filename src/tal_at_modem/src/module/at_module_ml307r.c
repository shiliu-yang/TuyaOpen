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
#define AT "AT\r"

// rsp
#define OK "OK"
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
static AT_MODEM_CB sg_urc_cb = NULL;

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
    // PR_DEBUG("URC +MATREADY received: %.*s", len, data);

    uint8_t at_ready = 0;

    if (strncmp("+MATREADY", data, len) == 0) {
        at_ready = 1;
    }

    if (sg_urc_cb) {
        sg_urc_cb(TAL_AT_MODEM_CMD_READY, &at_ready);
    }

    return;
}

static void __urc_cereg_cb(char *data, uint32_t len)
{
    PR_DEBUG("URC +CEREG received: %.*s", len, data);
    // +CEREG: 0
    // +CEREG: 5,"58BC","0C03A143",7
    return;
}

static void __urc_cpin_cb(char *data, uint32_t len)
{
    // PR_DEBUG("URC +CPIN received: %.*s", len, data);

    // +CPIN: READY
    uint8_t cpin_ready = 0;

    if (strncmp("+CPIN: READY", data, len) == 0) {
        cpin_ready = 1;
    }

    if (sg_urc_cb) {
        sg_urc_cb(TAL_AT_MODEM_CMD_SIM, &cpin_ready);
    }

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

static uint8_t __ml307r_at_check(void)
{
    // send AT
    OPERATE_RET rt = OPRT_OK;
    AT_LINE_T *rsp_line = NULL;

    TUYA_CALL_ERR_LOG(at_client_send_with_rsp(AT, strlen(AT), &rsp_line, 1000));
    if (OPRT_OK != rt) {
        return 0;
    }

    PR_DEBUG("AT RSP: %.*s", rsp_line->len, rsp_line->line);

    if (strcmp(OK, rsp_line->line) != 0) {
        return 0;
    }

    return 1;
}

OPERATE_RET at_module_ml307r_init(AT_MODULE_OPS_T *ops, AT_MODEM_CB urc_cb)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(ops, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(urc_cb, OPRT_INVALID_PARM);

    sg_urc_cb = urc_cb;

    ops->at_check = __ml307r_at_check;

    // register urc handler
    TUYA_CALL_ERR_RETURN(__ml307r_urc_register());

    PR_DEBUG("ML307R module initialized successfully");

    return rt;
}
