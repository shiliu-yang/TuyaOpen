/**
 * @file app_display.h
 * @brief Header file for Tuya Display System
 *
 * This header file provides the declarations for initializing the display system
 * and sending messages to the display. It includes the necessary data types and
 * function prototypes for interacting with the display functionality.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __APP_DISPLAY_H__
#define __APP_DISPLAY_H__

#include "tuya_cloud_types.h"

#include "lang_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
typedef uint8_t CHAT_ROLE_E;
#define CHAT_ROLE_USER      0
#define CHAT_ROLE_ASSISTANT 1
#define CHAT_ROLE_SYSTEM    2

// display network status
typedef uint8_t DIS_WIFI_STATUS_E;
#define DIS_WIFI_STATUS_DISCONNECTED 0
#define DIS_WIFI_STATUS_GOOD         1
#define DIS_WIFI_STATUS_FAIR         2
#define DIS_WIFI_STATUS_WEAK         3

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Initialize the display system
 *
 * @param None
 * @return OPERATE_RET Initialization result, OPRT_OK indicates success
 */
OPERATE_RET app_display_init(void);

/**
 * @brief Sets the display status of the application.
 *
 * This function updates the display status with the provided string.
 * It is used to reflect the current state or status of the application
 * on the display.
 *
 * @param status A pointer to a null-terminated string representing the
 *               status to be displayed. The string should not be NULL.
 */
void app_display_set_status(const char *status);

/**
 * @brief Sets the display notification of the application.
 *
 * This function updates the display notification with the provided string.
 * It is used to show notifications or alerts on the display.
 *
 * @param notification A pointer to a null-terminated string representing
 *                     the notification to be displayed. The string should
 *                     not be NULL.
 */
void app_display_show_notification(const char *notification);

/**
 * @brief Sets the display emotion of the application.
 *
 * This function updates the display emotion with the provided string.
 * It is used to reflect the current emotion or mood on the display.
 *
 * @param emotion A pointer to a null-terminated string representing the
 *                emotion to be displayed. The string should not be NULL.
 */
void app_display_set_emotion(const char *emotion);

/**
 * @brief Sets the display chat message of the application.
 *
 * This function updates the display chat message with the provided string.
 * It is used to show chat messages on the display.
 *
 * @param role A CHAT_ROLE_E value indicating the role of the sender (user,
 *             assistant, or system).
 * @param content A pointer to a null-terminated string representing the
 *                chat message to be displayed. The string should not be NULL.
 */
void app_display_set_chat_massage(CHAT_ROLE_E role, const char *content);

/**
 * @brief Sets the display Wi-Fi status of the application.
 *
 * This function updates the display Wi-Fi status with the provided status
 * value. It is used to reflect the current Wi-Fi connection status on the
 * display.
 *
 * @param status A DIS_WIFI_STATUS_E value indicating the Wi-Fi connection
 *               status (disconnected, good, fair, or weak).
 */
void app_display_set_network_status(DIS_WIFI_STATUS_E status);

#ifdef __cplusplus
}
#endif

#endif /* __APP_DISPLAY_H__ */
