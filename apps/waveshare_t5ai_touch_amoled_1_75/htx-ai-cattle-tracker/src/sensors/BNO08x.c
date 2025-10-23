/**
 * @file BNO08x.c
 * @brief BNO08x module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "BNO08x.h"
#include "tal_api.h"

#include "tuya_cloud_types.h"
#include "dev_config.h"

#include "cattle_ai_tracker_app.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define BNO08X_ENABLE_PIN TUYA_GPIO_NUM_44

#define BNO08X_OFFSET_YAW (-180)

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static volatile float sg_yaw_degree = 0;
static volatile BOOL_T sg_bno08x_enable = FALSE;

/***********************************************************
***********************function define**********************
***********************************************************/
OPERATE_RET bno08x_init(void)
{
    TUYA_GPIO_BASE_CFG_T pin_cfg;

    pin_cfg.mode = TUYA_GPIO_PUSH_PULL;
    pin_cfg.direct = TUYA_GPIO_OUTPUT;
    pin_cfg.level = TUYA_GPIO_LEVEL_LOW;
    tkl_gpio_init(BNO08X_ENABLE_PIN, &pin_cfg);
    tkl_gpio_write(BNO08X_ENABLE_PIN, TUYA_GPIO_LEVEL_LOW);

    PR_INFO("bno08x I2C initialized on GPIO44");
    return OPRT_OK;
}

OPERATE_RET bno08x_start(void)
{
    PR_DEBUG("bno08x_start");
    return tkl_gpio_write(BNO08X_ENABLE_PIN, TUYA_GPIO_LEVEL_HIGH);
}

OPERATE_RET bno08x_stop(void)
{
    PR_DEBUG("bno08x_stop");
    return tkl_gpio_write(BNO08X_ENABLE_PIN, TUYA_GPIO_LEVEL_LOW);
}

void bno08x_work_task(void *data)
{
    if (sg_bno08x_enable == TRUE) {
        bno08x_start();
    } else {
        bno08x_stop();
    }

    return;
}

void bno08x_enable(BOOL_T enable)
{
    if (sg_bno08x_enable == enable) {
        return;
    }

    sg_bno08x_enable = enable;
    tal_workq_schedule(WORKQ_SYSTEM, bno08x_work_task, NULL);
    return;
}

void bno08x_set_yaw_degree(int yaw_degree)
{
    // yaw_degree 0~359

    yaw_degree += BNO08X_OFFSET_YAW;
    if (yaw_degree < 0) {
        yaw_degree += 360;
    } else if (yaw_degree >= 360) {
        yaw_degree -= 360;
    }

    sg_yaw_degree = (float)yaw_degree;
    PR_DEBUG("BNO08x : %.2f, input: %d", sg_yaw_degree, yaw_degree);
    return;
}

void bno08x_refresh_ui(void)
{
    // Update tracker compass with real BMM150 heading data
    tracker_update_compass_heading(sg_yaw_degree);
    return;
}
