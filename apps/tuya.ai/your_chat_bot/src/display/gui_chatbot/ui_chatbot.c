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

#include "app_ui.h"

#include "lang_config.h"

#include "font_awesome_symbols.h"
#include "tuya_lvgl.h"
#include "lvgl.h"

/***********************************************************
************************macro define************************
***********************************************************/
LV_FONT_DECLARE(font_puhui_18_2);
LV_FONT_DECLARE(font_awesome_16_4);

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
} APP_THEME_COLORS_T;

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
} APP_UI_T;

typedef struct {
    lv_font_t *text;
    lv_font_t *icon;
    lv_font_t *emoji;
} APP_UI_FONT_T;

typedef struct {
    APP_THEME_COLORS_T theme;
    APP_UI_T ui;
    APP_UI_FONT_T font;

    TIMER_ID notification_tm_id;
} APP_CHATBOT_UI_T;

typedef struct {
    char emo_text[32];
    char *emo_icon;
} UI_EMOJI_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

static APP_CHATBOT_UI_T sg_ui = {0};

// emoji list
static UI_EMOJI_T sg_awesome_emo_list[] = {
    {"SAD", FONT_AWESOME_EMOJI_SAD},           {"ANGRY", FONT_AWESOME_EMOJI_ANGRY},
    {"NEUTRAL", FONT_AWESOME_EMOJI_NEUTRAL},   {"SURPRISE", FONT_AWESOME_EMOJI_SURPRISED},
    {"CONFUSED", FONT_AWESOME_EMOJI_CONFUSED}, {"THINKING", FONT_AWESOME_EMOJI_THINKING},
    {"HAPPY", FONT_AWESOME_EMOJI_HAPPY},
};

static UI_EMOJI_T sg_emo_list[] = {
    {"SAD", "😔"},      {"ANGRY", "😠"},    {"NEUTRAL", "😶"}, {"SURPRISE", "😯"},
    {"CONFUSED", "😏"}, {"THINKING", "🤔"}, {"HAPPY", "🙂"},
};

/***********************************************************
***********************function define**********************
***********************************************************/
static void __ui_light_theme_init(APP_THEME_COLORS_T *theme)
{
    if (theme == NULL) {
        return;
    }

    theme->background = lv_color_white();
    theme->text = lv_color_black();
    theme->chat_background = lv_color_hex(0xE0E0E0);
    theme->user_bubble = lv_color_hex(0x95EC69);
    theme->assistant_bubble = lv_color_white();
    theme->system_bubble = lv_color_hex(0xE0E0E0);
    theme->system_text = lv_color_hex(0x666666);
    theme->border = lv_color_hex(0xE0E0E0);
    theme->low_battery = lv_color_black();
}

static void __ui_dark_theme_init(APP_THEME_COLORS_T *theme)
{
    if (theme == NULL) {
        return;
    }

    theme->background = lv_color_hex(0x121212);
    theme->text = lv_color_white();
    theme->chat_background = lv_color_hex(0x1E1E1E);
    theme->user_bubble = lv_color_hex(0x1A6C37);
    theme->assistant_bubble = lv_color_hex(0x333333);
    theme->system_bubble = lv_color_hex(0x2A2A2A);
    theme->system_text = lv_color_hex(0xAAAAAA);
    theme->border = lv_color_hex(0x333333);
    theme->low_battery = lv_color_hex(0x333333);
}

static void __ui_font_init(APP_UI_FONT_T *font)
{
    if (font == NULL) {
        return;
    }

    font->text = &font_puhui_18_2;
    font->icon = &font_awesome_16_4;

    extern const lv_font_t *font_emoji_64_init(void);
    font->emoji = font_emoji_64_init();

    if (NULL == font->emoji) {
        PR_ERR("font_emoji_64_init failed");
    }
}

static void __ui_notification_timeout_cb(TIMER_ID timer_id, void *arg)
{
    lv_obj_add_flag(sg_ui.ui.notification_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sg_ui.ui.status_label, LV_OBJ_FLAG_HIDDEN);
}

void ui_frame_init(void)
{
    // Theme init
    __ui_light_theme_init(&sg_ui.theme);

    // Font init
    __ui_font_init(&sg_ui.font);

    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, sg_ui.font.text, 0);
    lv_obj_set_style_text_color(screen, sg_ui.theme.text, 0);
    lv_obj_set_style_bg_color(screen, sg_ui.theme.background, 0);

    // Container
    sg_ui.ui.container = lv_obj_create(screen);
    lv_obj_set_size(sg_ui.ui.container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(sg_ui.ui.container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(sg_ui.ui.container, 0, 0);
    lv_obj_set_style_border_width(sg_ui.ui.container, 0, 0);
    lv_obj_set_style_pad_row(sg_ui.ui.container, 0, 0);
    lv_obj_set_style_bg_color(sg_ui.ui.container, sg_ui.theme.background, 0);
    lv_obj_set_style_border_color(sg_ui.ui.container, sg_ui.theme.border, 0);

    // Status bar
    sg_ui.ui.status_bar = lv_obj_create(sg_ui.ui.container);
    lv_obj_set_size(sg_ui.ui.status_bar, LV_HOR_RES, sg_ui.font.text->line_height);
    lv_obj_set_style_radius(sg_ui.ui.status_bar, 0, 0);

    // Content
    sg_ui.ui.content = lv_obj_create(sg_ui.ui.container);
    lv_obj_set_scrollbar_mode(sg_ui.ui.content, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(sg_ui.ui.content, 0, 0);
    lv_obj_set_width(sg_ui.ui.content, LV_HOR_RES);
    lv_obj_set_flex_grow(sg_ui.ui.content, 1);
    lv_obj_set_flex_flow(sg_ui.ui.content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(sg_ui.ui.content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_EVENLY);

    // Emotion
    sg_ui.ui.emotion_label = lv_label_create(sg_ui.ui.content);
    lv_obj_set_style_text_font(sg_ui.ui.emotion_label, sg_ui.font.emoji, 0);
    // lv_label_set_text(sg_ui.ui.emotion_label, FONT_AWESOME_AI_CHIP);

    // Chat message
    sg_ui.ui.chat_message_label = lv_label_create(sg_ui.ui.content);
    lv_label_set_text(sg_ui.ui.chat_message_label, "");
    lv_obj_set_width(sg_ui.ui.chat_message_label, LV_HOR_RES * 0.9);         // Limit width to 90% of screen width
    lv_label_set_long_mode(sg_ui.ui.chat_message_label, LV_LABEL_LONG_WRAP); // Set to automatic line break mode
    lv_obj_set_style_text_align(sg_ui.ui.chat_message_label, LV_TEXT_ALIGN_CENTER, 0); // Set text to center alignment
    lv_label_set_text(sg_ui.ui.chat_message_label, "");

    // Status bar
    lv_obj_set_flex_flow(sg_ui.ui.status_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(sg_ui.ui.status_bar, 0, 0);
    lv_obj_set_style_border_width(sg_ui.ui.status_bar, 0, 0);
    lv_obj_set_style_pad_column(sg_ui.ui.status_bar, 0, 0);
    lv_obj_set_style_pad_left(sg_ui.ui.status_bar, 2, 0);
    lv_obj_set_style_bg_color(sg_ui.ui.status_bar, sg_ui.theme.background, 0);

    // Network status
    sg_ui.ui.network_label = lv_label_create(sg_ui.ui.status_bar);
    lv_obj_set_style_text_font(sg_ui.ui.network_label, sg_ui.font.icon, 0);
    lv_obj_set_style_text_color(sg_ui.ui.network_label, sg_ui.theme.text, 0);

    // Notification label
    sg_ui.ui.notification_label = lv_label_create(sg_ui.ui.status_bar);
    lv_obj_set_flex_grow(sg_ui.ui.notification_label, 1);
    lv_obj_set_style_text_align(sg_ui.ui.notification_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(sg_ui.ui.notification_label, sg_ui.theme.text, 0);
    lv_label_set_text(sg_ui.ui.notification_label, "");
    lv_obj_add_flag(sg_ui.ui.notification_label, LV_OBJ_FLAG_HIDDEN);
    tal_sw_timer_create(__ui_notification_timeout_cb, NULL, &sg_ui.notification_tm_id);

    // Status label
    sg_ui.ui.status_label = lv_label_create(sg_ui.ui.status_bar);
    lv_obj_set_flex_grow(sg_ui.ui.status_label, 1);
    lv_label_set_long_mode(sg_ui.ui.status_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(sg_ui.ui.status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(sg_ui.ui.status_label, sg_ui.theme.text, 0);
    lv_label_set_text(sg_ui.ui.status_label, INITIALIZING);

    // Mute label
    sg_ui.ui.mute_label = lv_label_create(sg_ui.ui.status_bar);
    lv_label_set_text(sg_ui.ui.mute_label, "");
    lv_obj_set_style_text_font(sg_ui.ui.mute_label, sg_ui.font.icon, 0);
    lv_obj_set_style_text_color(sg_ui.ui.mute_label, sg_ui.theme.text, 0);
}

void ui_set_user_msg(const char *text)
{
    if (sg_ui.ui.chat_message_label == NULL) {
        return;
    }

    lv_label_set_text(sg_ui.ui.chat_message_label, text);
    lv_obj_set_style_bg_color(sg_ui.ui.chat_message_label, sg_ui.theme.user_bubble, 0);
    lv_obj_set_style_text_color(sg_ui.ui.chat_message_label, sg_ui.theme.text, 0);
}

void ui_set_assistant_msg(const char *text)
{
    if (sg_ui.ui.chat_message_label == NULL) {
        return;
    }

    lv_label_set_text(sg_ui.ui.chat_message_label, text);
    lv_obj_set_style_bg_color(sg_ui.ui.chat_message_label, sg_ui.theme.assistant_bubble, 0);
    lv_obj_set_style_text_color(sg_ui.ui.chat_message_label, sg_ui.theme.text, 0);
}

void ui_set_system_msg(const char *text)
{
    if (sg_ui.ui.chat_message_label == NULL) {
        return;
    }

    lv_label_set_text(sg_ui.ui.chat_message_label, text);
    lv_obj_set_style_bg_color(sg_ui.ui.chat_message_label, sg_ui.theme.system_bubble, 0);
    lv_obj_set_style_text_color(sg_ui.ui.chat_message_label, sg_ui.theme.system_text, 0);
}

void ui_set_emotion(const char *emotion)
{
    if (NULL == sg_ui.ui.emotion_label) {
        return;
    }

    char *emo_icon = "😶";
    for (int i = 0; i < sizeof(sg_emo_list) / sizeof(UI_EMOJI_T); i++) {
        if (strcmp(emotion, sg_emo_list[i].emo_text) == 0) {
            emo_icon = sg_emo_list[i].emo_icon;
            break;
        }
    }

    lv_label_set_text(sg_ui.ui.emotion_label, emo_icon);
}

void ui_set_status(const char *status)
{
    if (sg_ui.ui.status_label == NULL) {
        return;
    }

    lv_label_set_text(sg_ui.ui.status_label, status);
    lv_obj_set_style_text_color(sg_ui.ui.status_label, sg_ui.theme.text, 0);
    lv_obj_set_style_text_align(sg_ui.ui.status_label, LV_TEXT_ALIGN_CENTER, 0);
}

void ui_set_notification(const char *notification)
{
    if (sg_ui.ui.notification_label == NULL) {
        return;
    }

    lv_label_set_text(sg_ui.ui.notification_label, notification);
    lv_obj_set_style_text_color(sg_ui.ui.notification_label, sg_ui.theme.text, 0);
    lv_obj_add_flag(sg_ui.ui.status_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sg_ui.ui.notification_label, LV_OBJ_FLAG_HIDDEN);
    tal_sw_timer_start(sg_ui.notification_tm_id, 3 * 1000, TAL_TIMER_ONCE);
}

void ui_set_network(DIS_WIFI_STATUS_E status)
{
    char *wifi_icon = NULL;

    if (sg_ui.ui.network_label == NULL) {
        return;
    }

    switch (status) {
    case DIS_WIFI_STATUS_DISCONNECTED:
        wifi_icon = FONT_AWESOME_WIFI_OFF;
        break;
    case DIS_WIFI_STATUS_GOOD:
        wifi_icon = FONT_AWESOME_WIFI;
        break;
    case DIS_WIFI_STATUS_FAIR:
        wifi_icon = FONT_AWESOME_WIFI_FAIR;
        break;
    case DIS_WIFI_STATUS_WEAK:
        wifi_icon = FONT_AWESOME_WIFI_WEAK;
        break;
    default:
        wifi_icon = FONT_AWESOME_WIFI_OFF;
        break;
    }

    lv_label_set_text(sg_ui.ui.network_label, wifi_icon);
}
