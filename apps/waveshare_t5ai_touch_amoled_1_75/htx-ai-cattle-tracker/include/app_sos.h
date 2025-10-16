/**
 * @file app_sos.h
 * @brief app_sos module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __APP_SOS_H__
#define __APP_SOS_H__

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

void app_sos_set(bool sos_status);

bool app_sos_get(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_SOS_H__ */
