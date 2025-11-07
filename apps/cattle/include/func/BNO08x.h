/**
 * @file BNO08x.h
 * @brief BNO08x module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __BNO08X_H__
#define __BNO08X_H__

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

OPERATE_RET bno08x_init(void);

float bno08x_get_yaw_degree(void);

#ifdef __cplusplus
}
#endif

#endif /* __BNO08X_H__ */
