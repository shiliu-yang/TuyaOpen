/**
 * @file app_battery.h
 * @brief app_battery module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __APP_BATTERY_H__
#define __APP_BATTERY_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

OPERATE_RET app_battery_init(void);

OPERATE_RET app_battery_status_refresh(void);

OPERATE_RET app_battery_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_BATTERY_H__ */
