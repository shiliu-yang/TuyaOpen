/**
 * @file app_encoder.c
 * @brief app_encoder module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "app_encoder.h"

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
static THREAD_HANDLE encoder_thrd_hdl = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/

static void __encoder_monitor_task(void *args)
{
    while (1) {
        int32_t current_angle = encoder_get_angle();
        uint8_t button_pressed = encoder_get_pressed();

        PR_DEBUG("encoder angle: %d, button pressed: %d", current_angle, button_pressed);

        tal_system_sleep(300);
    }
}

OPERATE_RET app_encoder_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tkl_encoder_init());

    // create a thread to monitor the encoder
    THREAD_CFG_T thread_cfg = {
        .thrdname = "app_encoder_task",
        .stackDepth = 2048,
        .priority = THREAD_PRIO_2,
    };
    TUYA_CALL_ERR_RETURN(tal_thread_create_and_start(&encoder_thrd_hdl, NULL, NULL, __encoder_monitor_task, NULL, &thread_cfg));

    return rt;
}
