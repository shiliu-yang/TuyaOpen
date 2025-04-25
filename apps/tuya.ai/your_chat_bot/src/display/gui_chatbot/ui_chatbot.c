/**
 * @file ui_chatbot.c
 * @brief Implementation of the GUI for chat bot interface
 *
 * This source file provides the implementation for initializing and managing
 * the GUI components of a chat bot interface. It includes functions
 * to initialize the display, create the chat frame, handle chat messages,
 * and manage various display states such as listening, speaking, and network
 * status.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include "tuya_cloud_types.h"
#include "tal_api.h"

#include "ui_chatbot.h"

#include "lang_config.h"

#include "font_awesome_symbols.h"
#include "tuya_lvgl.h"
#include "lvgl.h"

/***********************************************************
************************macro define************************
***********************************************************/
LV_FONT_DECLARE(font_puhui_18_2);
LV_FONT_DECLARE(font_awesome_30_4);

#define FONT_TEXT  font_puhui_18_2
#define FONT_ICON  font_awesome_30_4
#define FONT_EMOJI font_awesome_30_4

// Color definitions for dark theme
#define DARK_BACKGROUND_COLOR       lv_color_hex(0x121212) // Dark background
#define DARK_TEXT_COLOR             lv_color_white()       // White text
#define DARK_CHAT_BACKGROUND_COLOR  lv_color_hex(0x1E1E1E) // Slightly lighter than background
#define DARK_USER_BUBBLE_COLOR      lv_color_hex(0x1A6C37) // Dark green
#define DARK_ASSISTANT_BUBBLE_COLOR lv_color_hex(0x333333) // Dark gray
#define DARK_SYSTEM_BUBBLE_COLOR    lv_color_hex(0x2A2A2A) // Medium gray
#define DARK_SYSTEM_TEXT_COLOR      lv_color_hex(0xAAAAAA) // Light gray text
#define DARK_BORDER_COLOR           lv_color_hex(0x333333) // Dark gray border
#define DARK_LOW_BATTERY_COLOR      lv_color_hex(0xFF0000) // Red for dark mode

// Color definitions for light theme
#define LIGHT_BACKGROUND_COLOR       lv_color_white()       // White background
#define LIGHT_TEXT_COLOR             lv_color_black()       // Black text
#define LIGHT_CHAT_BACKGROUND_COLOR  lv_color_hex(0xE0E0E0) // Light gray background
#define LIGHT_USER_BUBBLE_COLOR      lv_color_hex(0x95EC69) // WeChat green
#define LIGHT_ASSISTANT_BUBBLE_COLOR lv_color_white()       // White
#define LIGHT_SYSTEM_BUBBLE_COLOR    lv_color_hex(0xE0E0E0) // Light gray
#define LIGHT_SYSTEM_TEXT_COLOR      lv_color_hex(0x666666) // Dark gray text
#define LIGHT_BORDER_COLOR           lv_color_hex(0xE0E0E0) // Light gray border
#define LIGHT_LOW_BATTERY_COLOR      lv_color_black()       // Black for light mode

/***********************************************************
***********************typedef define***********************
***********************************************************/
// Theme color structure
typedef struct {
    lv_color_t background;
    lv_color_t text;
    lv_color_t chat_background;
    lv_color_t user_bubble;
    lv_color_t assistant_bubble;
    lv_color_t system_bubble;
    lv_color_t system_text;
    lv_color_t border;
    lv_color_t low_battery;
} THEME_COLORS_T;

typedef struct {
    lv_obj_t *container;
    lv_obj_t *status_bar;
    lv_obj_t *content;
    lv_obj_t *emotion_label;
    lv_obj_t *chat_message_label;
    lv_obj_t *status_label;
    lv_obj_t *network_label;
    lv_obj_t *notification_label;
    lv_obj_t *mute_label;
} CHATBOT_UI_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
// Define dark theme colors
static const THEME_COLORS_T dark_theme_colors = {.background = DARK_BACKGROUND_COLOR,
                                                 .text = DARK_TEXT_COLOR,
                                                 .chat_background = DARK_CHAT_BACKGROUND_COLOR,
                                                 .user_bubble = DARK_USER_BUBBLE_COLOR,
                                                 .assistant_bubble = DARK_ASSISTANT_BUBBLE_COLOR,
                                                 .system_bubble = DARK_SYSTEM_BUBBLE_COLOR,
                                                 .system_text = DARK_SYSTEM_TEXT_COLOR,
                                                 .border = DARK_BORDER_COLOR,
                                                 .low_battery = DARK_LOW_BATTERY_COLOR};

// Define light theme colors
static const THEME_COLORS_T light_theme_colors = {.background = LIGHT_BACKGROUND_COLOR,
                                                  .text = LIGHT_TEXT_COLOR,
                                                  .chat_background = LIGHT_CHAT_BACKGROUND_COLOR,
                                                  .user_bubble = LIGHT_USER_BUBBLE_COLOR,
                                                  .assistant_bubble = LIGHT_ASSISTANT_BUBBLE_COLOR,
                                                  .system_bubble = LIGHT_SYSTEM_BUBBLE_COLOR,
                                                  .system_text = LIGHT_SYSTEM_TEXT_COLOR,
                                                  .border = LIGHT_BORDER_COLOR,
                                                  .low_battery = LIGHT_LOW_BATTERY_COLOR};

#define CUR_THEME (light_theme_colors)

static CHATBOT_UI_T sg_ui = {
    .container = NULL,
};

/***********************************************************
***********************function define**********************
***********************************************************/

void ui_chatbot_init(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, &FONT_TEXT, 0);
    lv_obj_set_style_text_color(screen, CUR_THEME.text, 0);
    lv_obj_set_style_bg_color(screen, CUR_THEME.background, 0);

    // Container
    sg_ui.container = lv_obj_create(screen);
    lv_obj_set_size(sg_ui.container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(sg_ui.container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(sg_ui.container, 0, 0);
    lv_obj_set_style_border_width(sg_ui.container, 0, 0);
    lv_obj_set_style_pad_row(sg_ui.container, 0, 0);
    lv_obj_set_style_bg_color(sg_ui.container, CUR_THEME.background, 0);
    lv_obj_set_style_border_color(sg_ui.container, CUR_THEME.border, 0);

    // Status bar
    sg_ui.status_bar = lv_obj_create(sg_ui.container);
    lv_obj_set_size(sg_ui.status_bar, LV_HOR_RES, FONT_TEXT.line_height);
    lv_obj_set_style_radius(sg_ui.status_bar, 0, 0);

    // Content
    sg_ui.content = lv_obj_create(sg_ui.container);
    lv_obj_set_scrollbar_mode(sg_ui.content, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(sg_ui.content, 0, 0);
    lv_obj_set_width(sg_ui.content, LV_HOR_RES);
    lv_obj_set_flex_grow(sg_ui.content, 1);
    lv_obj_set_flex_flow(sg_ui.content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(sg_ui.content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_EVENLY);

    // Emotion
    sg_ui.emotion_label = lv_label_create(sg_ui.content);
    lv_obj_set_style_text_font(sg_ui.emotion_label, &FONT_EMOJI, 0);
    lv_label_set_text(sg_ui.emotion_label, FONT_AWESOME_AI_CHIP);

    // Chat message
    sg_ui.chat_message_label = lv_label_create(sg_ui.content);
    lv_label_set_text(sg_ui.chat_message_label, "");
    lv_obj_set_width(sg_ui.chat_message_label, LV_HOR_RES * 0.9);         // Limit width to 90% of screen width
    lv_label_set_long_mode(sg_ui.chat_message_label, LV_LABEL_LONG_WRAP); // Set to automatic line break mode
    lv_obj_set_style_text_align(sg_ui.chat_message_label, LV_TEXT_ALIGN_CENTER, 0); // Set text to center alignment
    lv_label_set_text(sg_ui.chat_message_label, "");

    // Status bar
    lv_obj_set_flex_flow(sg_ui.status_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(sg_ui.status_bar, 0, 0);
    lv_obj_set_style_border_width(sg_ui.status_bar, 0, 0);
    lv_obj_set_style_pad_column(sg_ui.status_bar, 0, 0);
    lv_obj_set_style_pad_left(sg_ui.status_bar, 2, 0);
    lv_obj_set_style_bg_color(sg_ui.status_bar, CUR_THEME.background, 0);

    // Network status
    sg_ui.network_label = lv_label_create(sg_ui.status_bar);
    lv_obj_set_style_text_font(sg_ui.network_label, &FONT_ICON, 0);
    lv_obj_set_style_text_color(sg_ui.network_label, CUR_THEME.text, 0);

    // Notification label
    sg_ui.notification_label = lv_label_create(sg_ui.status_bar);
    lv_obj_set_flex_grow(sg_ui.notification_label, 1);
    lv_obj_set_style_text_align(sg_ui.notification_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(sg_ui.notification_label, CUR_THEME.text, 0);
    lv_label_set_text(sg_ui.notification_label, "");
    lv_obj_add_flag(sg_ui.notification_label, LV_OBJ_FLAG_HIDDEN);

    // Status label
    sg_ui.status_label = lv_label_create(sg_ui.status_bar);
    lv_obj_set_flex_grow(sg_ui.status_label, 1);
    lv_label_set_long_mode(sg_ui.status_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(sg_ui.status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(sg_ui.status_label, CUR_THEME.text, 0);
    lv_label_set_text(sg_ui.status_label, INITIALIZING);

    // Mute label
    sg_ui.mute_label = lv_label_create(sg_ui.status_bar);
    lv_label_set_text(sg_ui.mute_label, "");
    lv_obj_set_style_text_font(sg_ui.mute_label, &FONT_ICON, 0);
    lv_obj_set_style_text_color(sg_ui.mute_label, CUR_THEME.text, 0);
}
