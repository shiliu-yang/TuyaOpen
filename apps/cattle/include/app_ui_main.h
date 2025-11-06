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

void app_ui_setting_date_time_update(const char *date, const char *time);

void app_ui_setting_volume_update(uint8_t volume);

/**
 * @brief Set network icon
 * @param network_type Network type (0: 4G, 1: WiFi, 2: Wired)
 * @param status Network status (0: No network, 1: 1 bar, 2: 2 bars, 3: 3 bars, 4: 4 bars)
 */
 void app_ui_setting_network_update(uint8_t network_type, uint8_t status);

/**
 * @brief Set battery icon
 * @param percentage Battery percentage (0-100)
 * @param is_charging Is charging (true: charging, false: not charging)
 */
void app_ui_setting_battery_update(uint8_t percentage, uint8_t is_charging);

void app_ui_ai_chat_set_red_ring_visible(bool visible);

void app_ui_ai_chat_set_user_msg(const char *text);

void app_ui_ai_chat_set_assistant_msg(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* __APP_UI_MAIN_H__ */
