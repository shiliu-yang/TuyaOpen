/**
 * @file main.c
 * @brief Example implementation of an I2C driver for Tuya IoT projects.
 *
 * This file provides an example implementation of an I2C driver using the Tuya SDK.
 * It demonstrates the configuration and usage of I2C communication for reading and writing data to an I2C device.
 * The example covers initializing the I2C interface, sending commands to the device, and reading data from the device.
 *
 * The I2C driver example aims to help developers understand how to communicate with I2C devices in Tuya IoT projects.
 * It includes detailed examples of setting up I2C configurations, sending commands, and reading data from I2C devices.
 *
 * @note This example is designed to be adaptable to various Tuya IoT devices and platforms, showcasing fundamental I2C
 * operations that are critical for IoT device development.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "qmi8658.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
#define TASK_GPIO_PRIORITY THREAD_PRIO_2
#define TASK_GPIO_SIZE     4096

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static THREAD_HANDLE sg_i2c_handle;

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief i2c task
 *
 * @param[in] param:Task parameters
 * @return none
 */
static void __example_i2c_task(void *param)
{
    OPERATE_RET op_ret = OPRT_OK;

    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("TuyaOpen commit-id:  %s", OPEN_COMMIT);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("Platform commit-id:  %s", PLATFORM_COMMIT);

    qmi8658_dev_t dev;
    qmi8658_data_t data;

    op_ret = dev_i2c_init();
    if (op_ret != OPRT_OK) {
        PR_INFO("Failed to initialize I2C (error: %d)", op_ret);
        tal_thread_delete(NULL); // 删除自己
    }

    op_ret = qmi8658_init(&dev, QMI8658_ADDRESS_HIGH);
    if (op_ret != OPRT_OK) {
        PR_INFO("Failed to initialize QMI8658 (error: %d)", op_ret);
        tal_thread_delete(NULL); // 删除自己
    }

    qmi8658_set_accel_range(&dev, QMI8658_ACCEL_RANGE_8G);
    qmi8658_set_accel_odr(&dev, QMI8658_ACCEL_ODR_1000HZ);
    qmi8658_set_gyro_range(&dev, QMI8658_GYRO_RANGE_512DPS);
    qmi8658_set_gyro_odr(&dev, QMI8658_GYRO_ODR_1000HZ);
    
    qmi8658_set_accel_unit_mps2(&dev, true);
    qmi8658_set_gyro_unit_rads(&dev, true);
    
    qmi8658_set_display_precision(&dev, 4);

    while (1) {
        bool ready;
        op_ret = qmi8658_is_data_ready(&dev, &ready);
        if (op_ret == OPRT_OK && ready) {
            op_ret = qmi8658_read_sensor_data(&dev, &data);
            if (op_ret == OPRT_OK) {
                PR_INFO("Accel: X=%.4f m/s², Y=%.4f m/s², Z=%.4f m/s²",
                         data.accelX, data.accelY, data.accelZ);
                PR_INFO("Gyro:  X=%.4f rad/s, Y=%.4f rad/s, Z=%.4f rad/s",
                         data.gyroX, data.gyroY, data.gyroZ);
                PR_INFO("Temp:  %.2f °C, Timestamp: %lu",
                         data.temperature, data.timestamp);
                PR_INFO("----------------------------------------");
            } else {
                PR_ERR( "Failed to read sensor data (error: %d)", op_ret);
            }
        } else {
            PR_ERR( "Data not ready or error reading status (error: %d)", op_ret);
        }

        tal_system_sleep(100);
    }
}

/**
 * @brief user_main
 *
 * @return none
 */
void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;

    /* basic init */
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 4096, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    dev_sys_init();
    
    static THREAD_CFG_T thrd_param = {.priority = TASK_GPIO_PRIORITY, .stackDepth = TASK_GPIO_SIZE, .thrdname = "i2c"};
    TUYA_CALL_ERR_LOG(tal_thread_create_and_start(&sg_i2c_handle, NULL, NULL, __example_i2c_task, NULL, &thrd_param));

    return;
}

/**
 * @brief main
 *
 * @param argc
 * @param argv
 * @return void
 */
#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    user_main();

    while (1) {
        tal_system_sleep(500);
    }
}
#else

/* Tuya thread handle */
static THREAD_HANDLE ty_app_thread = NULL;

/**
 * @brief  task thread
 *
 * @param[in] arg:Parameters when creating a task
 * @return none
 */
static void tuya_app_thread(void *arg)
{
    user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {4096, 4, "tuya_app_main"};
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif