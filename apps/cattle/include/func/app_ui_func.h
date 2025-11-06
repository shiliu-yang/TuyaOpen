/**
 * @file app_ui_func.h
 * @brief app_ui_func module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __APP_UI_FUNC_H__
#define __APP_UI_FUNC_H__

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

void app_ui_func_setting_load_callback(void);
void app_ui_func_setting_unload_callback(void);

/**
 * @brief Trigger the setting screen update.
 * @return void
 */
void app_ui_func_setting_update_trigger(void);


#ifdef __cplusplus
}
#endif

#endif /* __APP_UI_FUNC_H__ */
