/**
 * @file lcd_display.h
 * @brief lcd_display module is used to 
 * @version 0.1
 * @date 2025-04-30
 */

#ifndef __LCD_DISPLAY_H__
#define __LCD_DISPLAY_H__

#include "display_common.h"

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

void ui_set_network(UI_WIFI_STATUS_E status);

#ifdef __cplusplus
}
#endif

#endif /* __LCD_DISPLAY_H__ */
