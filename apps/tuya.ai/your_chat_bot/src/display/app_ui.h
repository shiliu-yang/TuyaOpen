/**
 * @file app_ui.h
 * @brief app_ui module is used to
 * @version 0.1
 * @date 2025-04-28
 */

#ifndef __APP_UI_H__
#define __APP_UI_H__

#include "tuya_cloud_types.h"

#include "app_display.h"

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
void ui_frame_init(void);

void ui_set_user_msg(const char *text);

void ui_set_assistant_msg(const char *text);

void ui_set_system_msg(const char *text);

void ui_set_emotion(const char *emotion);

void ui_set_status(const char *status);

void ui_set_notification(const char *notification);

void ui_set_network(DIS_WIFI_STATUS_E status);

#ifdef __cplusplus
}
#endif

#endif /* __APP_UI_H__ */
