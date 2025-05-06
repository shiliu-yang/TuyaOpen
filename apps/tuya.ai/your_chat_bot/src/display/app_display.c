/**
 * @file app_display.c
 * @brief Handle display initialization and message processing
 *
 * This source file provides the implementation for initializing the display system,
 * creating a message queue, and handling display messages in a separate task.
 * It includes functions to initialize the display, send messages to the display,
 * and manage the display task.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "app_display.h"

#include "app_ui.h"

#include "tuya_lvgl.h"
#include "lvgl.h"

#include "tal_api.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef enum {
    UI_DISPLAY_TYPE_USER_MSG = 0,
    UI_DISPLAY_TYPE_ASSISTANT_MSG,
    UI_DISPLAY_TYPE_SYSTEM_MSG,

    UI_DISPLAY_TYPE_EMOTION,

    // status bar
    UI_DISPLAY_TYPE_STATUS,
    UI_DISPLAY_TYPE_NOTIFICATION,
    UI_DISPLAY_TYPE_NETWORK,

    UI_DISPLAY_TYPE_MAX
} UI_DISPLAY_TYPE_E;

typedef struct {
    UI_DISPLAY_TYPE_E type;
    int len;
    char *data;
} DISP_CHAT_MSG_T;

typedef struct {
    THREAD_HANDLE thrd_hdl;
    QUEUE_HANDLE queue_hdl;
} APP_DISPLAY_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static APP_DISPLAY_T sg_app_display = {
    .thrd_hdl = NULL,
    .queue_hdl = NULL,
};

/***********************************************************
***********************function define**********************
***********************************************************/

static void __app_display_homepage(void)
{
    tuya_lvgl_mutex_lock();

    extern const lv_img_dsc_t TuyaOpen_img;
    lv_obj_t *homepage_img = lv_image_create(lv_scr_act());
    lv_image_set_src(homepage_img, &TuyaOpen_img);

    lv_obj_center(homepage_img);

    tuya_lvgl_mutex_unlock();

    tal_system_sleep(1000);
}

static void __app_display_msg_handle(DISP_CHAT_MSG_T *msg_data)
{
    if (msg_data == NULL) {
        return;
    }

    tuya_lvgl_mutex_lock();

    switch (msg_data->type) {
    case UI_DISPLAY_TYPE_USER_MSG: {
        ui_set_user_msg(msg_data->data);
    } break;
    case UI_DISPLAY_TYPE_ASSISTANT_MSG: {
        ui_set_assistant_msg(msg_data->data);
    } break;
    case UI_DISPLAY_TYPE_SYSTEM_MSG: {
        ui_set_system_msg(msg_data->data);
    } break;
    case UI_DISPLAY_TYPE_EMOTION: {
        ui_set_emotion(msg_data->data);
    } break;
    case UI_DISPLAY_TYPE_STATUS: {
        ui_set_status(msg_data->data);
    } break;
    case UI_DISPLAY_TYPE_NOTIFICATION: {
        ui_set_notification(msg_data->data);
    } break;
    case UI_DISPLAY_TYPE_NETWORK: {
        DIS_WIFI_STATUS_E status = ((DIS_WIFI_STATUS_E *)msg_data->data)[0];
        ui_set_network(status);
    } break;
    default: {
        PR_ERR("Invalid display type: %d", msg_data->type);
    } break;
    }

    tuya_lvgl_mutex_unlock();
}

static void __chat_bot_ui_task(void *args)
{
    DISP_CHAT_MSG_T msg_data;

    (void)args;

    __app_display_homepage();

    ui_frame_init();

    ui_set_network(DIS_WIFI_STATUS_DISCONNECTED);
    ui_set_emotion("NATURAL");

    while (1) {
        memset(&msg_data, 0, sizeof(DISP_CHAT_MSG_T));
        tkl_queue_fetch(sg_app_display.queue_hdl, &msg_data, 0xFFFFFFFF);

        __app_display_msg_handle(&msg_data);

        if (msg_data.data) {
            tkl_system_psram_free(msg_data.data);
        }
        msg_data.data = NULL;
    }
}

/**
 * @brief Initialize the display system
 *
 * @param None
 * @return OPERATE_RET Initialization result, OPRT_OK indicates success
 */
OPERATE_RET app_display_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tuya_lvgl_init());

    TUYA_CALL_ERR_RETURN(tkl_queue_create_init(&sg_app_display.queue_hdl, sizeof(DISP_CHAT_MSG_T), 8));

    THREAD_CFG_T cfg = {
        .thrdname = "chat_ui",
        .priority = THREAD_PRIO_1,
        .stackDepth = 1024 * 4,
    };

    TUYA_CALL_ERR_RETURN(
        tal_thread_create_and_start(&sg_app_display.thrd_hdl, NULL, NULL, __chat_bot_ui_task, NULL, &cfg));

    return OPRT_OK;
}

/**
 * @brief Send display message to the display system
 *
 * @param tp Type of the display message
 * @param data Pointer to the message data
 * @param len Length of the message data
 * @return OPERATE_RET Result of sending the message, OPRT_OK indicates success
 */
OPERATE_RET app_display_send_msg(UI_DISPLAY_TYPE_E tp, const uint8_t *data, int len)
{
    DISP_CHAT_MSG_T chat_msg;

    chat_msg.type = tp;
    chat_msg.len = len;
    if (len && data != NULL) {
        chat_msg.data = (char *)tkl_system_psram_malloc(len + 1);
        if (NULL == chat_msg.data) {
            return OPRT_MALLOC_FAILED;
        }
        memcpy(chat_msg.data, data, len);
        chat_msg.data[len] = 0; //"\0"
    } else {
        chat_msg.data = NULL;
    }

    tkl_queue_post(sg_app_display.queue_hdl, &chat_msg, 0xFFFFFFFF);

    return OPRT_OK;
}
