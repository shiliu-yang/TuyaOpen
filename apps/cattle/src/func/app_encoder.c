/**
 * @file app_encoder.c
 * @brief app_encoder module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "app_encoder.h"

#include "app_ui_main.h"
#include "app_dp.h"

#include "tal_api.h"

#include "drv_encoder.h"

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/
static TIMER_ID encoder_timer_id = NULL;

static SYS_TIME_T sg_press_start_time = 0;

static int32_t sg_last_angle = 0;

/***********************************************************
***********************function define**********************
***********************************************************/

void app_sos_start(void)
{
    // DP update
    app_dp_sos_set(true);

    return;
}

void app_sos_stop(void)
{
    // DP update
    app_dp_sos_set(false);
    app_ui_SOS_screen_load(0);

    sg_press_start_time = 0;

    return;
}

void __encoder_timer_cb(TIMER_ID timer_id, void *arg)
{
    (void)timer_id;
    (void)arg;

    int32_t current_angle = encoder_get_angle();
    uint8_t button_pressed = encoder_get_pressed();

    // PR_INFO("encoder current_angle: %d, button_pressed: %d", current_angle, button_pressed);

    if (button_pressed) {
        if (sg_press_start_time == 0) {
            sg_press_start_time = tal_system_get_millisecond();
        }
        app_ui_SOS_screen_load(1);
    } else {
        if (tal_system_get_millisecond() - sg_press_start_time < 3000 && sg_press_start_time != 0) {
            app_ui_SOS_screen_load(0);
            sg_press_start_time = 0;
        }
    }

    if (current_angle != sg_last_angle) {
        int32_t angle_delta = current_angle - sg_last_angle;
        if (angle_delta > 0) {
            app_ui_tracker_zoom_out();
            PR_INFO("encoder zoom out");
        } else {
            app_ui_tracker_zoom_in();
            PR_INFO("encoder zoom in");
        }
        sg_last_angle = current_angle;
    }
}

OPERATE_RET app_encoder_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tkl_encoder_init());

    TUYA_CALL_ERR_RETURN(tal_sw_timer_create(__encoder_timer_cb, NULL, &encoder_timer_id));
    TUYA_CALL_ERR_RETURN(tal_sw_timer_start(encoder_timer_id, 200, TAL_TIMER_CYCLE));

    return rt;
}
