/**
 * @file display_common.h
 * @brief display_common module is used to
 * @version 0.1
 * @date 2025-04-23
 */

#ifndef __DISPLAY_COMMON_H__
#define __DISPLAY_COMMON_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
typedef uint8_t CHAT_ROLE_E;
#define CHAT_ROLE_USER      0
#define CHAT_ROLE_ASSISTANT 1
#define CHAT_ROLE_SYSTEM    2

typedef enum {
    TY_DISPLAY_TP_USER_MSG = 0,
    TY_DISPLAY_TP_ASSISTANT_MSG,
    TY_DISPLAY_TP_SYSTEM_MSG,

    TY_DISPLAY_TP_EMOTION,
    TY_DISPLAY_TP_STATUS,
    TY_DISPLAY_TP_NOTIFICATION,
    TY_DISPLAY_TP_NETWORK,

    TY_DISPLAY_TP_MAX,
} TY_DISPLAY_TYPE_E;

// display network status
typedef uint8_t UI_WIFI_STATUS_E;
#define UI_WIFI_STATUS_DISCONNECTED 0
#define UI_WIFI_STATUS_GOOD         1
#define UI_WIFI_STATUS_FAIR         2
#define UI_WIFI_STATUS_WEAK         3

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

#ifdef __cplusplus
}
#endif

#endif /* __DISPLAY_COMMON_H__ */
