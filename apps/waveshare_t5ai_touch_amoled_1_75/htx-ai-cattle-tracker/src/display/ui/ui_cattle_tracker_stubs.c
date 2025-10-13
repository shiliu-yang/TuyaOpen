/**
 * @file ui_cattle_tracker_stubs.c
 * @brief Stub implementations for UI functions when using Cattle AI Tracker UI
 *
 * This file provides placeholder implementations for the standard UI functions
 * that app_display.c calls, but are not used by the cattle_ai_tracker_app.c.
 * This allows the code to compile and run without modifying the cattle tracker UI.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#if defined(ENABLE_GUI_TRACKER) && (ENABLE_GUI_TRACKER == 1)

#include "ui_display.h"
#include "cattle_ai_tracker_app.h"
#include "tal_log.h"
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

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Initialize UI (stub - cattle tracker handles its own init)
 */
int ui_init(UI_FONT_T *ui_font)
{
    (void)ui_font;
    // Cattle tracker UI is initialized via lv_demo_cattle_ai_tracker()
    // This is just a stub to satisfy the interface
    // Note: Encoder zoom control is now integrated into sensor_integration.c
    PR_DEBUG("ui_init stub - cattle tracker UI handles its own initialization");
    return 0;
}

/**
 * @brief Set user message (stub - updates idle bottom text)
 */
void ui_set_user_msg(const char *text)
{
    if (text) {
        PR_DEBUG("User msg: %s", text);
        update_idle_bottom_text(text);
    }
}

/**
 * @brief Set assistant message (stub - updates idle bottom text)
 */
void ui_set_assistant_msg(const char *text)
{
    if (text) {
        PR_DEBUG("Assistant msg: %s", text);
        update_idle_bottom_text(text);
    }
}

/**
 * @brief Set system message (stub - updates idle bottom text)
 */
void ui_set_system_msg(const char *text)
{
    if (text) {
        PR_DEBUG("System msg: %s", text);
        update_idle_bottom_text(text);
    }
}

/**
 * @brief Set emotion (stub - not used in cattle tracker UI)
 */
void ui_set_emotion(const char *emotion)
{
    if (emotion) {
        PR_DEBUG("Emotion: %s (not displayed in tracker UI)", emotion);
    }
}

/**
 * @brief Set status (stub - not used in cattle tracker UI)
 */
void ui_set_status(const char *status)
{
    if (status) {
        PR_DEBUG("Status: %s (not displayed in tracker UI)", status);
    }
}

/**
 * @brief Set notification (stub - not used in cattle tracker UI)
 */
void ui_set_notification(const char *notification)
{
    if (notification) {
        PR_DEBUG("Notification: %s (not displayed in tracker UI)", notification);
    }
}

/**
 * @brief Set network status (stub - not used in cattle tracker UI)
 */
void ui_set_network(char *wifi_icon)
{
    if (wifi_icon) {
        PR_DEBUG("Network: %s (not displayed in tracker UI)", wifi_icon);
    }
}

/**
 * @brief Set chat mode (stub - not used in cattle tracker UI)
 */
void ui_set_chat_mode(const char *chat_mode)
{
    if (chat_mode) {
        PR_DEBUG("Chat mode: %s (not used in tracker UI)", chat_mode);
    }
}

/**
 * @brief Set status bar padding (stub - not used in cattle tracker UI)
 */
void ui_set_status_bar_pad(int32_t value)
{
    PR_DEBUG("Status bar pad: %d (not used in tracker UI)", (int)value);
}

#if defined(ENABLE_GUI_STREAM_AI_TEXT) && (ENABLE_GUI_STREAM_AI_TEXT == 1)
/**
 * @brief Streaming text functions (stubs - not used in cattle tracker UI)
 */
void ui_set_assistant_msg_stream_start(void)
{
    PR_DEBUG("Stream start (not used in tracker UI)");
}

void ui_set_assistant_msg_stream_data(const char *text)
{
    if (text) {
        PR_DEBUG("Stream data: %s (not used in tracker UI)", text);
    }
}

void ui_set_assistant_msg_stream_end(void)
{
    PR_DEBUG("Stream end (not used in tracker UI)");
}

void ui_set_assistant_msg_stream_interrupt(void)
{
    PR_DEBUG("Stream interrupt (not used in tracker UI)");
}
#endif

/***********************************************************
******************Zoom Control Functions********************
***********************************************************/

/* Global variable to track current distance scale */
static int sg_current_distance_scale = 200; /* Default 200m */

/**
 * @brief Set the tracker distance scale (zoom level)
 * @param scale_meters Distance scale in meters
 * 
 * This function directly calls animate_distance_scale() from cattle_ai_tracker_app.c
 * to trigger smooth zoom animations.
 */
void tracker_set_distance_scale(int scale_meters)
{
    /* Directly call the animate_distance_scale API */
    animate_distance_scale(scale_meters);
    sg_current_distance_scale = scale_meters;
    PR_DEBUG("[ZOOM] Distance scale set to %dm", scale_meters);
}

/**
 * @brief Get current tracker distance scale
 * @return Current distance scale in meters
 */
int tracker_get_distance_scale(void)
{
    return sg_current_distance_scale;
}

#endif /* ENABLE_GUI_TRACKER */

