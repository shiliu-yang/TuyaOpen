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

#include "pcf85063a.h"

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
static bool RTC_INT = FALSE;

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief interrupt callback function
 *
 * @param[in] args:parameters
 * @return none
 */
static void __gpio_irq_callback(void *args)
{
    /* Both TAL_PR_ and PR_ have locks in these two types of printing and should not be used in interrupts. */
    tkl_log_output("\r\n------------ GPIO IRQ Callbcak ------------\r\n");
    RTC_INT = TRUE;
}

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

    // Initial RTC time to be set
    static pcf85063a_datetime_t Set_Time = {
        .year = 2025,
        .month = 07,
        .day = 30,
        .dotw = 3,   // Day of the week: 0 = Sunday
        .hour = 9,
        .min = 0,
        .sec = 0
    };

    // Alarm time to be set
    static pcf85063a_datetime_t Set_Alarm_Time = {
        .year = 2025,
        .month = 07,
        .day = 30,
        .dotw = 3,
        .hour = 9,
        .min = 0,
        .sec = 2
    };

    char datetime_str[256];  // Buffer to store formatted date-time string
    pcf85063a_dev_t dev;
    pcf85063a_datetime_t Now_time;

    op_ret = dev_i2c_init();
    if (op_ret != OPRT_OK) {
        PR_ERR("Failed to initialize I2C (error: %d)", op_ret);
        tal_thread_delete(NULL); // 删除自己
    }

    PR_INFO("Initializing PCF85063A...");
    OPERATE_RET ret = pcf85063a_init(&dev, PCF85063A_ADDRESS);
    if (ret != OPRT_OK) {
        PR_ERR("Failed to initialize PCF85063A (error: %d)", ret);
        tal_thread_delete(NULL);
    }

    PR_INFO("Set current time.");
    pcf85063a_set_time_date(&dev, Set_Time);

    PR_INFO("Set alarm time.");
    pcf85063a_set_alarm(&dev, Set_Alarm_Time);

    PR_INFO("Enable alarm interrupt.");
    pcf85063a_enable_alarm(&dev);

    while (1) {
        
        // Read current time from RTC
        pcf85063a_get_time_date(&dev, &Now_time);

        // Format current time as a string
        pcf85063a_datetime_to_str(datetime_str, Now_time);
        PR_INFO("Now_time is %s", datetime_str);

        // Poll external IO pin for alarm (low level = alarm triggered)
        if (RTC_INT)
        {
            // Re-enable alarm if repeated alarms are required
            pcf85063a_enable_alarm(&dev);
            PR_INFO("The alarm clock goes off.");
            RTC_INT = FALSE;
        }
        tal_system_sleep(1000);
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

    dev_gpio_init(PCF85063A_INT_PIN, TUYA_GPIO_INPUT);
    dev_gpio_int_init(PCF85063A_INT_PIN, TUYA_GPIO_IRQ_LOW, __gpio_irq_callback);
    
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