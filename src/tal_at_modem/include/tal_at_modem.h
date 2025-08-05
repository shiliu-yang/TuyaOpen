/**
 * @file tal_at_modem.h
 * @brief tal_at_modem module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TAL_AT_MODEM_H__
#define __TAL_AT_MODEM_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
typedef uint8_t TAL_AT_MODEM_TYPE_T;
#define TAL_AT_MODEM_TYPE_ML307R 0x01
#define TAL_AT_MODEM_TYPE_MAX    0x02

typedef uint8_t TAL_AT_MODEM_CMD_T;
#define TAL_AT_MODEM_CMD_READY   (0x00)
#define TAL_AT_MODEM_CMD_SIM     (0x01)
#define TAL_AT_MODEM_CMD_NETWORK (0x02)

typedef void (*AT_MODEM_CB)(TAL_AT_MODEM_CMD_T cmd, void *args);

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    uint8_t (*at_check)(void);
} AT_MODULE_OPS_T;

/***********************************************************
********************function declaration********************
***********************************************************/

OPERATE_RET tal_at_modem_init(const char *transport_name, TAL_AT_MODEM_TYPE_T type);

#ifdef __cplusplus
}
#endif

#endif /* __TAL_AT_MODEM_H__ */
