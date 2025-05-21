/**
 * @file tuya_lvgl_for_esp32.c
 * @brief tuya_lvgl_for_esp32 module is used to
 * @version 0.1
 * @date 2025-05-21
 */

#include "tuya_cloud_types.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/
extern OPERATE_RET board_esp32_lvgl_init(void);
extern OPERATE_RET board_esp32_mutex_lock(void);
extern OPERATE_RET board_esp32_mutex_unlock(void);

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Initialize LVGL library and related devices and threads
 *
 * @param None
 * @return OPERATE_RET Initialization result, OPRT_OK indicates success
 */
OPERATE_RET tuya_lvgl_init(void)
{
    return board_esp32_lvgl_init();
}

/**
 * @brief Lock the LVGL mutex
 *
 * @param None
 * @return OPERATE_RET Lock result, OPRT_OK indicates success
 */
OPERATE_RET tuya_lvgl_mutex_lock(void)
{
    return board_esp32_mutex_lock();
}

/**
 * @brief Unlock the LVGL mutex
 *
 * @param None
 * @return OPERATE_RET Lock result, OPRT_OK indicates success
 */
OPERATE_RET tuya_lvgl_mutex_unlock(void)
{
    return board_esp32_mutex_unlock();
}
