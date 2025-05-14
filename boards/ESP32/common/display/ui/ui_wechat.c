/**
 * @file ui_wechat.c
 * @brief Implementation of the GUI for WeChat-like chat interface
 *
 * This source file provides the implementation for initializing and managing
 * the GUI components of a WeChat-like chat interface. It includes functions
 * to initialize the display, create the chat frame, handle chat messages,
 * and manage various display states such as listening, speaking, and network
 * status.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"

#if defined(ENABLE_GUI_WECHAT) && (ENABLE_GUI_WECHAT == 1)

#include "ui_display.h"

#include "font_awesome_symbols.h"
#include "lvgl.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define MAX_MASSAGE_NUM 20

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    lv_style_t style_avatar;
    lv_style_t style_ai_bubble;
    lv_style_t style_user_bubble;

    lv_obj_t *container;
    lv_obj_t *status_bar;
    lv_obj_t *content;
    lv_obj_t *emotion_label;
    lv_obj_t *chat_message_label;
    lv_obj_t *status_label;
    lv_obj_t *network_label;
    lv_obj_t *notification_label;
    lv_obj_t *mute_label;
} APP_UI_T;

typedef struct {
    APP_UI_T ui;

    UI_FONT_T font;

    lv_timer_t *notification_tm;
} APP_CHATBOT_UI_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static APP_CHATBOT_UI_T sg_ui = {0};

/***********************************************************
***********************function define**********************
***********************************************************/

int __ui_font_init(UI_FONT_T *ui_font)
{
    if (ui_font == NULL) {
        return -1;
    }

    sg_ui.font.text = ui_font->text;
    sg_ui.font.icon = ui_font->icon;
    sg_ui.font.emoji = ui_font->emoji;
    sg_ui.font.emoji_list = ui_font->emoji_list;

    return 0;
}

static void __ui_styles_init(void)
{
    lv_style_init(&sg_ui.ui.style_avatar);
    lv_style_set_radius(&sg_ui.ui.style_avatar, LV_RADIUS_CIRCLE);
    lv_style_set_bg_color(&sg_ui.ui.style_avatar, lv_palette_main(LV_PALETTE_GREY));
    lv_style_set_border_width(&sg_ui.ui.style_avatar, 1);
    lv_style_set_border_color(&sg_ui.ui.style_avatar, lv_palette_darken(LV_PALETTE_GREY, 2));

    lv_style_init(&sg_ui.ui.style_ai_bubble);
    lv_style_set_bg_color(&sg_ui.ui.style_ai_bubble, lv_color_white());
    lv_style_set_radius(&sg_ui.ui.style_ai_bubble, 15);
    lv_style_set_pad_all(&sg_ui.ui.style_ai_bubble, 12);
    lv_style_set_shadow_width(&sg_ui.ui.style_ai_bubble, 12);
    lv_style_set_shadow_color(&sg_ui.ui.style_ai_bubble, lv_color_hex(0xCCCCCC));

    lv_style_init(&sg_ui.ui.style_user_bubble);
    lv_style_set_bg_color(&sg_ui.ui.style_user_bubble, lv_palette_main(LV_PALETTE_GREEN));
    lv_style_set_text_color(&sg_ui.ui.style_user_bubble, lv_color_white());
    lv_style_set_radius(&sg_ui.ui.style_user_bubble, 15);
    lv_style_set_pad_all(&sg_ui.ui.style_user_bubble, 12);
    lv_style_set_shadow_width(&sg_ui.ui.style_user_bubble, 12);
    lv_style_set_shadow_color(&sg_ui.ui.style_user_bubble, lv_palette_darken(LV_PALETTE_GREEN, 2));
}

static void __ui_notification_timeout_cb(lv_timer_t *timer)
{
    lv_timer_del(sg_ui.notification_tm);
    sg_ui.notification_tm = NULL;

    lv_obj_add_flag(sg_ui.ui.notification_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sg_ui.ui.status_label, LV_OBJ_FLAG_HIDDEN);
}

int ui_init(UI_FONT_T *ui_font)
{
    // Style init
    __ui_styles_init();

    // Font init
    __ui_font_init(ui_font);

    lv_obj_t *screen = lv_obj_create(lv_scr_act());
    lv_obj_set_size(screen, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xF0F0F0), 0);
    lv_obj_set_style_pad_all(screen, 0, 0);

    lv_obj_set_style_text_font(screen, sg_ui.font.text, 0);
    lv_obj_set_style_text_color(screen, lv_color_black(), 0);
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(screen, LV_DIR_NONE);

    // Container
    sg_ui.ui.container = lv_obj_create(screen);
    lv_obj_set_size(sg_ui.ui.container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_pad_all(sg_ui.ui.container, 0, 0);
    lv_obj_set_style_border_width(sg_ui.ui.container, 0, 0);
    lv_obj_set_style_pad_row(sg_ui.ui.container, 0, 0);

    // Status bar
    sg_ui.ui.status_bar = lv_obj_create(sg_ui.ui.container);
    lv_obj_set_size(sg_ui.ui.status_bar, LV_HOR_RES, 40);
    lv_obj_set_style_bg_color(sg_ui.ui.status_bar, lv_palette_main(LV_PALETTE_GREEN), 0);

    // Status label
    sg_ui.ui.status_label = lv_label_create(sg_ui.ui.status_bar);
    lv_obj_set_flex_grow(sg_ui.ui.status_label, 1);
    lv_label_set_long_mode(sg_ui.ui.status_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_center(sg_ui.ui.status_label);
    lv_label_set_text(sg_ui.ui.status_label, INITIALIZING);

    // Network status
    sg_ui.ui.network_label = lv_label_create(sg_ui.ui.status_bar);
    lv_obj_set_style_text_font(sg_ui.ui.network_label, sg_ui.font.icon, 0);
    lv_obj_align(sg_ui.ui.network_label, LV_ALIGN_RIGHT_MID, 0, 0);

    // Notification label
    sg_ui.ui.notification_label = lv_label_create(sg_ui.ui.status_bar);
    lv_obj_set_flex_grow(sg_ui.ui.notification_label, 1);
    lv_label_set_long_mode(sg_ui.ui.status_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_center(sg_ui.ui.notification_label);
    lv_label_set_text(sg_ui.ui.notification_label, "");
    lv_obj_add_flag(sg_ui.ui.notification_label, LV_OBJ_FLAG_HIDDEN);

    // Emotion
    sg_ui.ui.emotion_label = lv_label_create(sg_ui.ui.status_bar);
    lv_obj_set_style_text_font(sg_ui.ui.emotion_label, sg_ui.font.icon, 0);
    lv_obj_align(sg_ui.ui.emotion_label, LV_ALIGN_LEFT_MID, 0, 0);
    lv_label_set_text(sg_ui.ui.emotion_label, FONT_AWESOME_AI_CHIP);

    // content
    sg_ui.ui.content = lv_obj_create(sg_ui.ui.container);
    lv_obj_set_size(sg_ui.ui.content, LV_HOR_RES, LV_VER_RES - 40);
    lv_obj_set_flex_flow(sg_ui.ui.content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_ver(sg_ui.ui.content, 8, 0);
    lv_obj_set_style_pad_hor(sg_ui.ui.content, 10, 0);
    lv_obj_align(sg_ui.ui.content, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_move_background(sg_ui.ui.content);

    lv_obj_set_scroll_dir(sg_ui.ui.content, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(sg_ui.ui.content, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(sg_ui.ui.content, LV_OPA_TRANSP, 0);

    return 0;
}

void ui_set_user_msg(const char *text)
{
    if (sg_ui.ui.content == NULL) {
        return;
    }

    // Check if the number of messages exceeds the limit
    uint32_t child_count = lv_obj_get_child_cnt(sg_ui.ui.content);
    if (child_count >= MAX_MASSAGE_NUM) {
        lv_obj_t *first_child = lv_obj_get_child(sg_ui.ui.content, 0);
        if (first_child) {
            lv_obj_del(first_child);
        }
    }

    lv_obj_t *msg_cont = lv_obj_create(sg_ui.ui.content);
    lv_obj_remove_style_all(msg_cont);
    lv_obj_set_size(msg_cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_ver(msg_cont, 6, 0);
    lv_obj_set_style_pad_column(msg_cont, 10, 0);

    lv_obj_t *avatar = lv_obj_create(msg_cont);
    lv_obj_set_style_text_font(avatar, sg_ui.font.icon, 0);
    lv_obj_add_style(avatar, &sg_ui.ui.style_avatar, 0);
    lv_obj_set_size(avatar, 40, 40);
    lv_obj_align(avatar, LV_ALIGN_TOP_RIGHT, 0, 0);

    lv_obj_t *icon = lv_label_create(avatar);
    lv_label_set_text(icon, FONT_AWESOME_USER);
    lv_obj_center(icon);

    lv_obj_t *bubble = lv_obj_create(msg_cont);
    lv_obj_set_width(bubble, LV_PCT(75));
    lv_obj_set_height(bubble, LV_SIZE_CONTENT);
    lv_obj_add_style(bubble, &sg_ui.ui.style_user_bubble, 0);
    lv_obj_align_to(bubble, avatar, LV_ALIGN_OUT_LEFT_TOP, -10, 0);

    lv_obj_set_scrollbar_mode(bubble, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(bubble, LV_DIR_NONE);

    lv_obj_t *text_cont = lv_obj_create(bubble);
    lv_obj_remove_style_all(text_cont);
    lv_obj_set_size(text_cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(text_cont, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *label = lv_label_create(text_cont);
    lv_label_set_text(label, text);
    lv_obj_set_width(label, LV_PCT(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);

    lv_obj_scroll_to_view_recursive(msg_cont, LV_ANIM_ON);
    lv_obj_update_layout(sg_ui.ui.content);
}

void ui_set_assistant_msg(const char *text)
{
    if (sg_ui.ui.content == NULL) {
        return;
    }

    // Check if the number of messages exceeds the limit
    uint32_t child_count = lv_obj_get_child_cnt(sg_ui.ui.content);
    if (child_count >= MAX_MASSAGE_NUM) {
        lv_obj_t *first_child = lv_obj_get_child(sg_ui.ui.content, 0);
        if (first_child) {
            lv_obj_del(first_child);
        }
    }

    lv_obj_t *msg_cont = lv_obj_create(sg_ui.ui.content);
    lv_obj_remove_style_all(msg_cont);
    lv_obj_set_size(msg_cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_ver(msg_cont, 6, 0);
    lv_obj_set_style_pad_column(msg_cont, 10, 0);

    lv_obj_t *avatar = lv_obj_create(msg_cont);
    lv_obj_set_style_text_font(avatar, sg_ui.font.icon, 0);
    lv_obj_add_style(avatar, &sg_ui.ui.style_avatar, 0);
    lv_obj_set_size(avatar, 40, 40);
    lv_obj_align(avatar, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *icon = lv_label_create(avatar);
    lv_label_set_text(icon, FONT_AWESOME_USER_ROBOT);
    lv_obj_center(icon);

    lv_obj_t *bubble = lv_obj_create(msg_cont);
    lv_obj_set_width(bubble, LV_PCT(75));
    lv_obj_set_height(bubble, LV_SIZE_CONTENT);
    lv_obj_add_style(bubble, &sg_ui.ui.style_ai_bubble, 0);
    lv_obj_align_to(bubble, avatar, LV_ALIGN_OUT_RIGHT_TOP, 10, 0);

    lv_obj_set_scrollbar_mode(bubble, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(bubble, LV_DIR_NONE);

    lv_obj_t *text_cont = lv_obj_create(bubble);
    lv_obj_remove_style_all(text_cont);
    lv_obj_set_size(text_cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(text_cont, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *label = lv_label_create(text_cont);
    lv_label_set_text(label, text);
    lv_obj_set_width(label, LV_PCT(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);

    lv_obj_scroll_to_view_recursive(msg_cont, LV_ANIM_ON);
    lv_obj_update_layout(sg_ui.ui.content);
}

void ui_set_system_msg(const char *text)
{
    if (sg_ui.ui.content == NULL) {
        return;
    }
}

void ui_set_emotion(const char *emotion)
{
    if (NULL == sg_ui.ui.emotion_label) {
        return;
    }

    char *emo_icon = sg_ui.font.emoji_list[0].emo_icon;
    for (int i = 0; i < EMO_ICON_MAX_NUM; i++) {
        if (strcmp(emotion, sg_ui.font.emoji_list[i].emo_text) == 0) {
            emo_icon = sg_ui.font.emoji_list[i].emo_icon;
            break;
        }
    }

    lv_obj_set_style_text_font(sg_ui.ui.emotion_label, sg_ui.font.emoji, 0);
    lv_label_set_text(sg_ui.ui.emotion_label, emo_icon);
}

void ui_set_status(const char *status)
{
    if (sg_ui.ui.status_label == NULL) {
        return;
    }

    lv_label_set_text(sg_ui.ui.status_label, status);
    lv_obj_set_style_text_align(sg_ui.ui.status_label, LV_TEXT_ALIGN_CENTER, 0);
}

void ui_set_notification(const char *notification)
{
    if (sg_ui.ui.notification_label == NULL) {
        return;
    }

    lv_label_set_text(sg_ui.ui.notification_label, notification);
    lv_obj_add_flag(sg_ui.ui.status_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sg_ui.ui.notification_label, LV_OBJ_FLAG_HIDDEN);
    if (NULL == sg_ui.notification_tm) {
        sg_ui.notification_tm = lv_timer_create(__ui_notification_timeout_cb, 3000, NULL);
    } else {
        lv_timer_reset(sg_ui.notification_tm);
    }
}

void ui_set_network(char *wifi_icon)
{
    if (sg_ui.ui.network_label == NULL || wifi_icon == NULL) {
        return;
    }

    lv_label_set_text(sg_ui.ui.network_label, wifi_icon);
}

void ui_set_status_bar_pad(int32_t value)
{
    if (sg_ui.ui.status_bar == NULL) {
        return;
    }

    lv_obj_set_style_pad_left(sg_ui.ui.status_bar, value, 0);
    lv_obj_set_style_pad_right(sg_ui.ui.status_bar, value, 0);
}

#endif
