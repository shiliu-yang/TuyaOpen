/**
 * @file at_client.h
 * @brief at_client module is used to
 * @version 0.1
 * @date 2025-11-13
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __AT_CLIENT_H__
#define __AT_CLIENT_H__

#include "tuya_cloud_types.h"

#include "at_line.h"

#include "tdl_transport_manage.h"

#include "tuya_slist.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define LINE_END_SYMBOL_MAX_LEN 8
#define LINE_END_SYMBOL_CRLF    "\r\n"
#define LINE_END_SYMBOL_LF      "\n"
#define LINE_END_SYMBOL_CR      "\r"

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef void (*URC_HANDLER)(char *data, uint32_t len);

typedef struct {
    SLIST_HEAD node;     // Node for single linked list
    char *prefix;        // Prefix of the response
    char *suffix;        // Suffix of the response
    URC_HANDLER handler; // URC handler function
} AT_URC_T;

/***********************************************************
********************function declaration********************
***********************************************************/

OPERATE_RET at_client_init(TDL_TRANSPORT_HANDLE transport_hdl);

OPERATE_RET at_client_deinit(void);

OPERATE_RET at_client_add_urc_handler(AT_URC_T *urc_handler);

OPERATE_RET at_client_send(const char *cmd, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* __AT_CLIENT_H__ */
