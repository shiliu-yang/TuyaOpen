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

#include "lc76g.h"

#define CONFIG_USE_GPS_UART 1
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
static THREAD_HANDLE sg_gps_handle;

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief GPS task (supports both I2C and UART)
 *
 * @param[in] param:Task parameters
 * @return none
 */
static void __example_gps_task(void *param)
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

    lc76g_dev_t dev;

#ifdef CONFIG_USE_GPS_I2C
    PR_NOTICE("Using GPS I2C interface");
    op_ret = dev_i2c_init();
    if (op_ret != OPRT_OK) {
        PR_ERR("Failed to initialize I2C (error: %d)", op_ret);
        tal_thread_delete(NULL);
    }
    
    op_ret = lc76g_init_i2c(&dev, LC76G_ADDRESS, DEVICE_ADDRESS_R);
    if (op_ret != OPRT_OK) {
        PR_ERR("Failed to initialize LC76G with I2C (error: %d)", op_ret);
        tal_thread_delete(NULL);
    }
#elif defined(CONFIG_USE_GPS_UART)
    PR_NOTICE("Using GPS UART interface");
    op_ret = dev_uart_init(EXAMPLE_UART_PORT, EXAMPLE_UART_BAUDRATE);
    if (op_ret != OPRT_OK) {
        PR_ERR("Failed to initialize UART (error: %d)", op_ret);
        tal_thread_delete(NULL);
    }
    
    op_ret = lc76g_init_uart(&dev, EXAMPLE_UART_PORT, EXAMPLE_UART_BAUDRATE);
    if (op_ret != OPRT_OK) {
        PR_ERR("Failed to initialize LC76G with UART (error: %d)", op_ret);
        tal_thread_delete(NULL);
    }
#else
    #error "Please select GPS interface: CONFIG_USE_GPS_I2C or CONFIG_USE_GPS_UART"
#endif
    
    while (1) {
        lc76g_get_data(&dev);
        const lc76g_state_t *s = lc76g_get_state();
        char datebuf[7] = {0};
        lc76g_get_data_ddmmyy(datebuf);
        
        PR_NOTICE("----------------------------------------");
        PR_NOTICE("Parsed GPS Data:");
        PR_INFO("  Time (UTC):    %02d:%02d:%02d.%03d", s->utc_hour, s->utc_minute, s->utc_second, s->utc_millisecond);
        PR_INFO("  Date:          %s", datebuf);
        PR_INFO("  Latitude:      %.6f°", s->latitude_deg);
        PR_INFO("  Longitude:     %.6f°", s->longitude_deg);
        PR_INFO("  Altitude:      %.1f m", s->altitude_m);
        PR_INFO("  Satellites:    %d", s->satellites_in_use);
        PR_INFO("  Fix Quality:   %d (0=invalid, 1=GPS, 2=DGPS)", s->fix_quality);
        PR_INFO("  Connection:    %d (0=no fix, 1=fixed)", s->connect_state);
        PR_INFO("  Signal Level:  %d/5", s->signal_level_5);
        PR_INFO("  Speed:         %.1f km/h", s->speed_kmh);
        PR_INFO("  Course:        %.1f°", s->course_deg);
        PR_INFO("  Status:        %c (A=active, V=void)", s->last_status);
        PR_NOTICE("----------------------------------------");
        
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

    dev_sys_init();
    
    static THREAD_CFG_T thrd_param = {.priority = TASK_GPIO_PRIORITY, .stackDepth = TASK_GPIO_SIZE, .thrdname = "gps"};
    TUYA_CALL_ERR_LOG(tal_thread_create_and_start(&sg_gps_handle, NULL, NULL, __example_gps_task, NULL, &thrd_param));

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