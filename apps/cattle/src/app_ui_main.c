/**
 * @file app_ui_main.c
 * @brief app_ui_main module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "app_ui_main.h"

#include "ui.h"
#include "screens/ui_ai_chat.h"
#include "screens/ui_setting.h"
#include "screens/ui_SOS.h"
#include "screens/ui_tracker.h"
#include "screens/ui_tracker_close.h"

#include "app_ui_page_manage.h"

#include "tuya_lvgl.h"

#include "tal_api.h"
#include <string.h>

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static bool s_red_ring_visible = false;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
THREAD_HANDLE ui_thrd_hdl;

static app_page_t ai_chat_page = {
    .init = ui_ai_chat_screen_init,
    .deinit = NULL,
    .page_obj = &ui_ai_chat,
};

app_page_t setting_page = {
    .init = ui_setting_screen_init,
    .deinit = NULL,
    .page_obj = &ui_setting,
};

app_page_t sos_page = {
    .init = ui_SOS_screen_init,
    .deinit = ui_SOS_screen_destroy,
    .page_obj = &ui_SOS,
};

app_page_t tracker_page = {
    .init = ui_tracker_screen_init,
    .deinit = NULL,
    .page_obj = &ui_tracker,
};

app_page_t tracker_close_page = {
    .init = ui_tracker_close_screen_init,
    .deinit = NULL,
    .page_obj = &ui_tracker_close,
};

/***********************************************************
***********************function define**********************
***********************************************************/

static void __chat_bot_ui_task(void *args)
{
    // OPERATE_RET rt = OPRT_OK;

    (void)args;

    PR_DEBUG("Initializing UI main...");

    tuya_lvgl_mutex_lock();
    ui_init();
    tuya_lvgl_mutex_unlock();

    PR_DEBUG("ui init success");

    app_pages_init();
    app_page_load_initial(&ai_chat_page);

    for (;;) {
        // Sleep for a while to yield CPU
        tal_system_sleep(100);
    }
}

OPERATE_RET app_ui_main_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tuya_lvgl_init());

    THREAD_CFG_T cfg = {
        .thrdname = "tracker_ui",
        .priority = THREAD_PRIO_4,
        .stackDepth = 1024 * 4,
    };
    TUYA_CALL_ERR_RETURN(tal_thread_create_and_start(&ui_thrd_hdl, NULL, NULL, __chat_bot_ui_task, NULL, &cfg));

    return OPRT_OK;
}

// SOS screen load
static uint8_t s_load = 0;

static void __SOS_screen_load_cb(void *data)
{
    (void)data;

    if (s_load) {
        app_page_load(&sos_page, LV_SCR_LOAD_ANIM_NONE, 1);
    } else {
        /* Call sos_cancel before going back to clean up state */
        tuya_lvgl_mutex_lock();
        sos_cancel();
        tuya_lvgl_mutex_unlock();
        /* app_page_back will call ui_SOS_screen_destroy automatically */
        app_page_back(LV_SCR_LOAD_ANIM_NONE, 1);
    }
}

void app_ui_SOS_screen_load(uint8_t load)
{
    if (s_load == load) {
        return;
    }

    s_load = load;

    tal_workq_schedule(WORKQ_SYSTEM, __SOS_screen_load_cb, NULL);
}

void app_ui_setting_gps_update(bool valid, uint8_t satellite_count)
{
    tuya_lvgl_mutex_lock();
    ui_setting_update_gps_status(valid, satellite_count);
    tuya_lvgl_mutex_unlock();
}

void app_ui_setting_date_time_update(const char *date, const char *time)
{
    tuya_lvgl_mutex_lock();
    ui_setting_set_date(date);
    ui_setting_set_time(time);
    tuya_lvgl_mutex_unlock();
}

void app_ui_setting_volume_update(uint8_t volume)
{
    tuya_lvgl_mutex_lock();
    ui_setting_set_volume(volume);
    tuya_lvgl_mutex_unlock();
}

void app_ui_setting_battery_update(uint8_t percentage, uint8_t is_charging)
{
    tuya_lvgl_mutex_lock();
    ui_setting_set_battery(percentage, is_charging);
    tuya_lvgl_mutex_unlock();
}

/**
 * @brief Set network icon
 * @param network_type Network type (0: 4G, 1: WiFi, 2: Wired)
 * @param status Network status (0: No network, 1: 1 bar, 2: 2 bars, 3: 3 bars, 4: 4 bars)
 */
void app_ui_setting_network_update(uint8_t network_type, uint8_t status)
{
    PR_DEBUG("app_ui_setting_network_update: network_type: %d, status: %d", network_type, status);

    tuya_lvgl_mutex_lock();
    ui_setting_set_network(network_type, status);
    tuya_lvgl_mutex_unlock();
}

// AI chat screen
static char *s_user_msg_text = NULL;
static char *s_assistant_msg_text = NULL;

static void __ai_chat_red_ring_workq_cb(void *data)
{
    (void)data;
    bool visible = s_red_ring_visible;

    tuya_lvgl_mutex_lock();
    ui_ai_chat_set_red_ring_visible(visible);
    tuya_lvgl_mutex_unlock();

    PR_DEBUG("AI chat red ring visibility set to: %d", visible);
}

void app_ui_ai_chat_set_red_ring_visible(bool visible)
{
    s_red_ring_visible = visible;
    tal_workq_schedule(WORKQ_SYSTEM, __ai_chat_red_ring_workq_cb, NULL);
}

static void __ai_chat_user_msg_workq_cb(void *data)
{
    (void)data;

    if (s_user_msg_text == NULL) {
        return;
    }

    tuya_lvgl_mutex_lock();
    ui_ai_chat_update_text(s_user_msg_text);
    tuya_lvgl_mutex_unlock();

    // PR_DEBUG("AI chat user message updated: %.30s...", s_user_msg_text);

    /* Free the allocated text */
    tal_psram_free(s_user_msg_text);
    s_user_msg_text = NULL;
}

void app_ui_ai_chat_set_user_msg(const char *text)
{
    if (text == NULL || strlen(text) == 0 || strlen(text) > 2 * 1024) {
        return;
    }

    /* Free previous text if exists */
    if (s_user_msg_text != NULL) {
        tal_psram_free(s_user_msg_text);
        s_user_msg_text = NULL;
    }

    /* Allocate and copy text */
    size_t len = strlen(text);
    s_user_msg_text = tal_psram_malloc(len + 1);
    if (s_user_msg_text == NULL) {
        PR_ERR("Failed to allocate memory for user message");
        return;
    }
    memset(s_user_msg_text, 0, len + 1);
    strncpy(s_user_msg_text, text, len);

    /* Schedule async update */
    tal_workq_schedule(WORKQ_SYSTEM, __ai_chat_user_msg_workq_cb, NULL);
}

static void __ai_chat_assistant_msg_workq_cb(void *data)
{
    (void)data;

    if (s_assistant_msg_text == NULL) {
        return;
    }

    tuya_lvgl_mutex_lock();
    ui_ai_chat_update_text(s_assistant_msg_text);
    tuya_lvgl_mutex_unlock();

    // PR_DEBUG("AI chat assistant message updated: %.30s...", s_assistant_msg_text);

    /* Free the allocated text */
    tal_psram_free(s_assistant_msg_text);
    s_assistant_msg_text = NULL;
}

void app_ui_ai_chat_set_assistant_msg(const char *text)
{
    if (text == NULL || strlen(text) == 0 || strlen(text) > 2 * 1024) {
        return;
    }

    /* Free previous text if exists */
    if (s_assistant_msg_text != NULL) {
        tal_psram_free(s_assistant_msg_text);
        s_assistant_msg_text = NULL;
    }

    /* Allocate and copy text */
    size_t len = strlen(text);
    s_assistant_msg_text = tal_psram_malloc(len + 1);
    if (s_assistant_msg_text == NULL) {
        PR_ERR("Failed to allocate memory for assistant message");
        return;
    }
    memset(s_assistant_msg_text, 0, len + 1);
    strncpy(s_assistant_msg_text, text, len);

    /* Schedule async update */
    tal_workq_schedule(WORKQ_SYSTEM, __ai_chat_assistant_msg_workq_cb, NULL);
}

void app_ui_tracker_target_update(uint32_t total_distance, float heading_degrees, float bearing_degrees)
{
    tuya_lvgl_mutex_lock();
    ui_tracker_target_update(total_distance, heading_degrees, bearing_degrees);
    tuya_lvgl_mutex_unlock();
}

static void __tracker_show_workq_cb(void *data)
{
    (void)data;
    app_page_load(&tracker_page, LV_SCR_LOAD_ANIM_MOVE_LEFT, 1);
}

void app_ui_tracker_show(void)
{
    tal_workq_schedule(WORKQ_SYSTEM, __tracker_show_workq_cb, NULL);
}

void app_ui_tracker_zoom_in(void)
{
    tuya_lvgl_mutex_lock();
    ui_tracker_zoom_in();
    tuya_lvgl_mutex_unlock();
}

void app_ui_tracker_zoom_out(void)
{
    tuya_lvgl_mutex_lock();
    ui_tracker_zoom_out();
    tuya_lvgl_mutex_unlock();
}