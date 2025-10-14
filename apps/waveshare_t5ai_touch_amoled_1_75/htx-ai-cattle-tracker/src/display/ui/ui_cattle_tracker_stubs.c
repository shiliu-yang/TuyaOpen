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
#include "tal_time_service.h"
#include "netmgr.h"
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
    
    /* Initialize UI elements with current system values */
    /* Get current local time and date */
    TIME_T posix_time = tal_time_get_posix();
    POSIX_TM_S tm_time;
    OPERATE_RET ret = tal_time_get_local_time_custom(posix_time, &tm_time);
    if (ret == OPRT_OK) {
        /* Set initial time from system (local time) */
        set_settings_time(tm_time.tm_hour, tm_time.tm_min);
        
        /* Set initial date from system */
        set_settings_date(tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday);
        
        PR_DEBUG("Initial UI time set: %04d/%02d/%02d %02d:%02d", 
                 tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
                 tm_time.tm_hour, tm_time.tm_min);
    } else {
        /* Fallback to default values if time not available */
        set_settings_time(12, 0);
        set_settings_date(2024, 1, 1);
        PR_WARN("Failed to get system time, using defaults");
    }
    
    /* Set initial volume to 0% */
    set_volume(0);
    
    /* Set initial GPS satellite count to 0 */
    set_gps_satellite_count(0);
    
    /* Update network connection status */
    ui_update_network_status();
    
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
 * @brief Set recording indicator (red ring and microphone icon)
 * @param is_recording true to show recording indicator, false to hide
 */
void ui_set_recording_indicator(bool is_recording)
{
    PR_DEBUG("Recording indicator: %s", is_recording ? "ON" : "OFF");
    set_idle_red_ring(is_recording);
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

/***********************************************************
******************Volume Control Functions*******************
***********************************************************/

/* Volume change callback for system integration */
static void (*sg_volume_change_handler)(int volume) = NULL;

/**
 * @brief Internal callback for volume changes from UI
 * @param volume New volume value (0-100)
 */
static void internal_volume_change_callback(int volume)
{
    PR_DEBUG("[VOLUME] Volume changed via UI to: %d%%", volume);
    
    /* Forward to registered handler if set */
    if (sg_volume_change_handler) {
        sg_volume_change_handler(volume);
    }
}

/**
 * @brief Register a volume change handler
 * @param handler Function to call when volume changes from UI
 * 
 * This allows the main application to be notified when the user
 * changes volume via the UI slider.
 */
void ui_register_volume_change_handler(void (*handler)(int volume))
{
    sg_volume_change_handler = handler;
    
    /* Set the callback in the cattle tracker app */
    set_volume_change_callback(internal_volume_change_callback);
    
    PR_DEBUG("[VOLUME] Volume change handler registered");
}

/**
 * @brief Set system volume (from external source)
 * @param volume Volume value (0-100)
 * 
 * This updates the UI slider to reflect volume changes from
 * external sources (e.g., hardware controls, system events).
 */
void ui_set_system_volume(int volume)
{
    set_volume(volume);
    PR_DEBUG("[VOLUME] System volume set to: %d%%", volume);
}

/**
 * @brief Get current volume from UI
 * @return Current volume value (0-100)
 */
int ui_get_current_volume(void)
{
    return get_volume();
}

/***********************************************************
******************Time/Date Control Functions***************
***********************************************************/

/**
 * @brief Set settings panel time
 * @param hour Hour value (0-23)
 * @param minute Minute value (0-59)
 */
void ui_set_settings_time(int hour, int minute)
{
    set_settings_time(hour, minute);
    PR_DEBUG("[TIME] Settings time set to: %02d:%02d", hour, minute);
}

/**
 * @brief Set settings panel date
 * @param year Year value (e.g., 2024)
 * @param month Month value (1-12)
 * @param day Day value (1-31)
 */
void ui_set_settings_date(int year, int month, int day)
{
    set_settings_date(year, month, day);
    PR_DEBUG("[DATE] Settings date set to: %04d/%02d/%02d", year, month, day);
}

/***********************************************************
*************Network Connection Status Functions*************
***********************************************************/

/**
 * @brief Update network icon based on active connection type
 * 
 * Queries the network manager to determine which connection type
 * is currently active (WiFi, Cellular, Wired) and updates the
 * settings panel network icon accordingly.
 */
void ui_update_network_status(void)
{
    bool use_4g = false;
    bool is_enabled = false;
    
    /* Check WiFi status */
    netmgr_status_e wifi_status = NETMGR_LINK_DOWN;
    if (netmgr_conn_get(NETCONN_WIFI, NETCONN_CMD_STATUS, &wifi_status) == OPRT_OK) {
        if (wifi_status == NETMGR_LINK_UP) {
            use_4g = false;
            is_enabled = true;
            PR_DEBUG("[NETWORK] WiFi is active");
        }
    }
    
    /* Check Cellular (4G) status */
    netmgr_status_e cellular_status = NETMGR_LINK_DOWN;
    if (netmgr_conn_get(NETCONN_CELLULAR, NETCONN_CMD_STATUS, &cellular_status) == OPRT_OK) {
        if (cellular_status == NETMGR_LINK_UP) {
            use_4g = true;
            is_enabled = true;
            PR_DEBUG("[NETWORK] Cellular (4G) is active");
        }
    }
    
    /* Check Wired status - treat as WiFi icon */
    netmgr_status_e wired_status = NETMGR_LINK_DOWN;
    if (netmgr_conn_get(NETCONN_WIRED, NETCONN_CMD_STATUS, &wired_status) == OPRT_OK) {
        if (wired_status == NETMGR_LINK_UP) {
            use_4g = false;
            is_enabled = true;
            PR_DEBUG("[NETWORK] Wired connection is active");
        }
    }
    
    /* Update UI with connection status */
    set_network_icon(use_4g, is_enabled);
    
    if (!is_enabled) {
        PR_WARN("[NETWORK] No active network connection");
    }
}

/**
 * @brief Get active connection type
 * @return netmgr_type_e: The active connection type (NETCONN_WIFI, NETCONN_CELLULAR, etc.)
 * 
 * Returns the currently active network connection type by checking
 * status of all available connection types.
 */
netmgr_type_e ui_get_active_connection_type(void)
{
    netmgr_status_e status;
    
    /* Check in priority order: Cellular, WiFi, Wired */
    if (netmgr_conn_get(NETCONN_CELLULAR, NETCONN_CMD_STATUS, &status) == OPRT_OK) {
        if (status == NETMGR_LINK_UP) {
            return NETCONN_CELLULAR;
        }
    }
    
    if (netmgr_conn_get(NETCONN_WIFI, NETCONN_CMD_STATUS, &status) == OPRT_OK) {
        if (status == NETMGR_LINK_UP) {
            return NETCONN_WIFI;
        }
    }
    
    if (netmgr_conn_get(NETCONN_WIRED, NETCONN_CMD_STATUS, &status) == OPRT_OK) {
        if (status == NETMGR_LINK_UP) {
            return NETCONN_WIRED;
        }
    }
    
    return NETCONN_AUTO;  /* No active connection */
}

#endif /* ENABLE_GUI_TRACKER */

