/**
 * @file app_encoder.c
 * @brief app_encoder module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "app_encoder.h"

#include "app_ui_main.h"

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

/***********************************************************
***********************function define**********************
***********************************************************/

void __encoder_timer_cb(TIMER_ID timer_id, void *arg)
{
    (void)timer_id;
    (void)arg;

    int32_t current_angle = encoder_get_angle();
    uint8_t button_pressed = encoder_get_pressed();

    // PR_DEBUG("encoder timer callback, angle: %d, button pressed: %d", current_angle, button_pressed);

    if (button_pressed) {
        app_ui_SOS_screen_load(1);
    } else if (current_angle > 10) {
        app_ui_SOS_screen_load(0);
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
