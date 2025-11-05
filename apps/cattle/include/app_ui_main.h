/**
 * @file app_ui_main.h
 * @brief app_ui_main module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __APP_UI_MAIN_H__
#define __APP_UI_MAIN_H__

#include "tuya_cloud_types.h"
#include "app_ui_page_manage.h"

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

extern app_page_t setting_page;
extern app_page_t sos_page;
extern app_page_t tracker_page;
extern app_page_t tracker_close_page;

OPERATE_RET app_ui_main_init(void);

void app_ui_SOS_screen_load(uint8_t load);

#ifdef __cplusplus
}
#endif

#endif /* __APP_UI_MAIN_H__ */
