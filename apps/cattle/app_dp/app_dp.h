/**
 * @file app_dp.h
 * @brief app_dp module is used to handle the data points
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __APP_DP_H__
#define __APP_DP_H__

#include "tuya_cloud_types.h"
#include "tuya_iot.h"
#include "tuya_iot_dp.h"

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

void app_dp_process(uint8_t id, dp_prop_tp_t type, dp_value_t value);

// DP update functions
OPERATE_RET app_dp_volume_upload(uint8_t volume);
OPERATE_RET app_dp_sos_set(bool sos_status);
OPERATE_RET app_dp_switch_set(bool switch_status);
OPERATE_RET app_dp_battery_upload(uint8_t is_charging, uint8_t battery_percentage);

#ifdef __cplusplus
}
#endif

#endif /* __APP_DP_H__ */
