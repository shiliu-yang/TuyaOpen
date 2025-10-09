/**
 * @file main.c
 * @brief BMM150 Magnetometer Demo Application
 *
 * This file provides a demo implementation of the BMM150 magnetometer sensor
 * using I2C communication through dev_config.c (following GPS codebase pattern).
 * It demonstrates reading magnetic field data and calculating heading.
 *
 * The BMM150 is a low-power, low-noise 3-axis digital magnetometer that
 * provides accurate magnetic field measurements for compass applications.
 *
 * @note This example uses dev_config.c for I2C initialization following the GPS codebase
 * pattern for consistency and reliability.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "bmm150.h"
#include <math.h>
#include <string.h>
#include "tal_cli.h"

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
static THREAD_HANDLE sg_bmm150_handle;
static bmm150_dev_t g_bmm150_dev; // Global device for CLI access

/***********************************************************
***********************function define**********************
***********************************************************/

// ============================================================================
// BMM150 CLI COMMANDS (Tuya CLI Framework)
// ============================================================================

/**
 * @brief CLI command: Start BMM150 calibration
 * @param argc Number of arguments
 * @param argv Argument array
 */
static void bmm150_cal_cmd(int argc, char *argv[]) {
    PR_INFO("Starting BMM150 calibration...");
    bmm150_cli_manual_calibration(&g_bmm150_dev);
}

/**
 * @brief CLI command: Show calibration status
 * @param argc Number of arguments
 * @param argv Argument array
 */
static void bmm150_status_cmd(int argc, char *argv[]) {
    bmm150_cli_cal_status(&g_bmm150_dev);
}

/**
 * @brief CLI command: Reset calibration
 * @param argc Number of arguments
 * @param argv Argument array
 */
static void bmm150_reset_cmd(int argc, char *argv[]) {
    bmm150_cli_reset_calibration(&g_bmm150_dev);
}

/**
 * @brief CLI command: Show current offsets
 * @param argc Number of arguments
 * @param argv Argument array
 */
static void bmm150_offsets_cmd(int argc, char *argv[]) {
    bmm150_cli_show_offsets(&g_bmm150_dev);
}

/**
 * @brief CLI command: Show help
 * @param argc Number of arguments
 * @param argv Argument array
 */
static void bmm150_help_cmd(int argc, char *argv[]) {
    PR_INFO("=== BMM150 CLI Commands ===");
    PR_INFO("bmm150_test    - Test CLI functionality");
    PR_INFO("bmm150_cal     - Start figure-8 calibration");
    PR_INFO("bmm150_status  - Show calibration status");
    PR_INFO("bmm150_reset   - Reset calibration data");
    PR_INFO("bmm150_offsets - Show current offsets");
    PR_INFO("bmm150_read    - Get current sensor reading");
    PR_INFO("bmm150_help    - Show this help");
    PR_INFO("========================");
}

/**
 * @brief CLI command: Test command
 * @param argc Number of arguments
 * @param argv Argument array
 */
static void bmm150_test_cmd(int argc, char *argv[]) {
    PR_INFO("BMM150 CLI test command working!");
    PR_INFO("Arguments received: %d", argc);
    for (int i = 0; i < argc; i++) {
        PR_INFO("  argv[%d] = %s", i, argv[i]);
    }
}

/**
 * @brief CLI command: Get current sensor reading
 * @param argc Number of arguments
 * @param argv Argument array
 */
static void bmm150_read_cmd(int argc, char *argv[]) {
    OPERATE_RET ret = bmm150_read_mag_data(&g_bmm150_dev);
    if (ret == OPRT_OK) {
        // Apply calibration offsets following Grove demo pattern
        bmm150_mag_data_t value;
        value.x = g_bmm150_dev.raw_mag_data.raw_datax - g_bmm150_dev.calibration.x_offset;
        value.y = g_bmm150_dev.raw_mag_data.raw_datay - g_bmm150_dev.calibration.y_offset;
        value.z = g_bmm150_dev.raw_mag_data.raw_dataz - g_bmm150_dev.calibration.z_offset;
        
        // Calculate heading following Grove BMM150 demo pattern exactly
        float xyHeading = atan2(value.x, value.y);
        float zxHeading = atan2(value.z, value.x);
        float heading = xyHeading;
        
        // Normalize to 0-2π range
        if (heading < 0) heading += 2 * 3.14159f;
        if (heading > 2 * 3.14159f) heading -= 2 * 3.14159f;
        float heading_degrees = heading * 180.0f / 3.14159f;
        
        PR_INFO("BMM150 Reading:");
        PR_INFO("  Calibrated X: %d μT (raw: %d)", value.x, g_bmm150_dev.raw_mag_data.raw_datax);
        PR_INFO("  Calibrated Y: %d μT (raw: %d)", value.y, g_bmm150_dev.raw_mag_data.raw_datay);
        PR_INFO("  Calibrated Z: %d μT (raw: %d)", value.z, g_bmm150_dev.raw_mag_data.raw_dataz);
        PR_INFO("  XY Heading: %.1f°", xyHeading * 180.0f / 3.14159f);
        PR_INFO("  ZX Heading: %.1f°", zxHeading * 180.0f / 3.14159f);
        PR_INFO("  Final Heading: %.1f°", heading_degrees);
        
        // CSV style raw data output using printf with calibrated offsets and fixed offset settings
        printf("CLI_RAW_DATA,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%.1f,%.1f,%.1f\n", 
               g_bmm150_dev.raw_mag_data.raw_datax, 
               g_bmm150_dev.raw_mag_data.raw_datay, 
               g_bmm150_dev.raw_mag_data.raw_dataz, 
               g_bmm150_dev.raw_mag_data.raw_data_r,
               value.x,  // calibrated X
               value.y,  // calibrated Y
               value.z,  // calibrated Z
               g_bmm150_dev.calibration.x_offset,  // fixed offset X
               g_bmm150_dev.calibration.y_offset,  // fixed offset Y
               g_bmm150_dev.calibration.z_offset,  // fixed offset Z
               xyHeading * 180.0f / 3.14159f, 
               zxHeading * 180.0f / 3.14159f, 
               heading_degrees);
    } else {
        PR_ERR("Failed to read sensor data: %d", ret);
    }
}

/**
 * @brief CLI command table for BMM150
 */
static cli_cmd_t bmm150_cli_cmd[] = {
    {.name = "bmm150_test",    .func = bmm150_test_cmd,    .help = "Test BMM150 CLI functionality"},
    {.name = "bmm150_cal",     .func = bmm150_cal_cmd,     .help = "Start BMM150 figure-8 calibration"},
    {.name = "bmm150_status",  .func = bmm150_status_cmd,  .help = "Show BMM150 calibration status"},
    {.name = "bmm150_reset",   .func = bmm150_reset_cmd,   .help = "Reset BMM150 calibration data"},
    {.name = "bmm150_offsets", .func = bmm150_offsets_cmd, .help = "Show BMM150 current offsets"},
    {.name = "bmm150_read",    .func = bmm150_read_cmd,    .help = "Get current BMM150 sensor reading"},
    {.name = "bmm150_help",    .func = bmm150_help_cmd,    .help = "Show BMM150 CLI help"},
};

/**
 * @brief Initialize BMM150 CLI commands
 */
void bmm150_cli_init(void) {
    OPERATE_RET ret = tal_cli_cmd_register(bmm150_cli_cmd, sizeof(bmm150_cli_cmd) / sizeof(bmm150_cli_cmd[0]));
    if (ret == OPRT_OK) {
        PR_INFO("BMM150 CLI commands registered successfully!");
        PR_INFO("Available commands: bmm150_cal, bmm150_status, bmm150_reset, bmm150_offsets, bmm150_read, bmm150_help");
    } else {
        PR_ERR("Failed to register BMM150 CLI commands: %d", ret);
    }
}

/**
 * @brief BMM150 magnetometer task
 *
 * @param[in] param:Task parameters
 * @return none
 */
static void __example_bmm150_task(void *param)
{
    OPERATE_RET op_ret = OPRT_OK;

    // Use global device for CLI access
    g_bmm150_dev.i2c_addr = BMM150_ADDRESS;
    

    // Initialize I2C using dev_config.c (following GPS codebase pattern)
    op_ret = bmm150_i2c_init();
    if (op_ret != OPRT_OK) {
        PR_ERR("Failed to initialize I2C (error: %d)", op_ret);
        tal_thread_delete(NULL);
    }
    
    // Test I2C port functionality
    op_ret = bmm150_test_i2c_port();
    if (op_ret != OPRT_OK) {
        PR_ERR("I2C port test failed (error: %d)", op_ret);
        tal_thread_delete(NULL);
    }
    
    // Test chip ID reading
    op_ret = bmm150_test_chip_id(0x10);
    if (op_ret != OPRT_OK) {
        op_ret = bmm150_test_chip_id(0x11);
        if (op_ret != OPRT_OK) {
            PR_ERR("BMM150 not found on I2C bus");
            tal_thread_delete(NULL);
        }
    }
    
    // // First scan the I2C bus to check if device is present
    // op_ret = bmm150_scan_i2c_bus(&dev);
    // if (op_ret != OPRT_OK) {
    //     PR_ERR("BMM150 not detected on I2C bus. Check connections:");
    //     PR_ERR("- VCC to 3.3V");
    //     PR_ERR("- GND to GND");
    //     PR_ERR("- SCL to P20 (with 4.7kΩ pull-up to 3.3V)");
    //     PR_ERR("- SDA to P21 (with 4.7kΩ pull-up to 3.3V)");
    //     PR_ERR("- SD0 to GND (for I2C address 0x10)");
    //     PR_ERR("- CSB to 3.3V (enable I2C mode)");
    //     PR_ERR("- PS to 3.3V (power enable)");
    //     tal_thread_delete(NULL);
    // }
    
    op_ret = bmm150_init(&g_bmm150_dev, BMM150_ADDRESS);
    if (op_ret != OPRT_OK) {
        PR_ERR("Failed to initialize BMM150 (error: %d)", op_ret);
        tal_thread_delete(NULL);
    }

    PR_INFO("BMM150 initialized successfully - reading magnetometer data...");
    PR_INFO("CLI Commands available: 'help' for command list");
    
    // Configuration: Enable/disable calibration process
    #define ENABLE_CALIBRATION_PROCESS 0  // Set to 1 to enable, 0 to disable
    
    // Default calibration offsets (X, Y, Z)
    // #define DEFAULT_X_OFFSET -20
    // #define DEFAULT_Y_OFFSET 108
    // #define DEFAULT_Z_OFFSET -5823
    #define DEFAULT_X_OFFSET -80
    #define DEFAULT_Y_OFFSET -190
    #define DEFAULT_Z_OFFSET -7029
    
    #if ENABLE_CALIBRATION_PROCESS
    PR_INFO("Starting figure-8 calibration following Grove BMM150 demo...");
    PR_INFO("Move the sensor in figure-8 patterns for 10 seconds!");

    // Figure-8 calibration following Grove BMM150 demo pattern
    bmm150_mag_data_t value_offset;
    int16_t value_x_min = 0, value_x_max = 0;
    int16_t value_y_min = 0, value_y_max = 0;
    int16_t value_z_min = 0, value_z_max = 0;
    
    // Get initial readings
    op_ret = bmm150_read_mag_data(&g_bmm150_dev);
    if (op_ret == OPRT_OK) {
        value_x_min = g_bmm150_dev.raw_mag_data.raw_datax;
        value_x_max = g_bmm150_dev.raw_mag_data.raw_datax;
        value_y_min = g_bmm150_dev.raw_mag_data.raw_datay;
        value_y_max = g_bmm150_dev.raw_mag_data.raw_datay;
        value_z_min = g_bmm150_dev.raw_mag_data.raw_dataz;
        value_z_max = g_bmm150_dev.raw_mag_data.raw_dataz;
    }
    tal_system_sleep(100);
    
    uint32_t timeStart = tal_system_get_millisecond();
    uint32_t timeout = 10000; // 10 seconds
    
    PR_INFO("Calibration started - move sensor in figure-8 patterns!");
    
    while ((tal_system_get_millisecond() - timeStart) < timeout) {
        op_ret = bmm150_read_mag_data(&g_bmm150_dev);
        if (op_ret == OPRT_OK) {
            // Update X-axis min/max (following Grove demo exactly)
            if (value_x_min > g_bmm150_dev.raw_mag_data.raw_datax) {
                value_x_min = g_bmm150_dev.raw_mag_data.raw_datax;
            } else if (value_x_max < g_bmm150_dev.raw_mag_data.raw_datax) {
                value_x_max = g_bmm150_dev.raw_mag_data.raw_datax;
            }
            
            // Update Y-axis min/max
            if (value_y_min > g_bmm150_dev.raw_mag_data.raw_datay) {
                value_y_min = g_bmm150_dev.raw_mag_data.raw_datay;
            } else if (value_y_max < g_bmm150_dev.raw_mag_data.raw_datay) {
                value_y_max = g_bmm150_dev.raw_mag_data.raw_datay;
            }
            
            // Update Z-axis min/max
            if (value_z_min > g_bmm150_dev.raw_mag_data.raw_dataz) {
                value_z_min = g_bmm150_dev.raw_mag_data.raw_dataz;
            } else if (value_z_max < g_bmm150_dev.raw_mag_data.raw_dataz) {
                value_z_max = g_bmm150_dev.raw_mag_data.raw_dataz;
            }
        }
        
        PR_INFO("."); // Progress indicator
        tal_system_sleep(100);
    }
    
    // Calculate offsets following Grove demo exactly
    value_offset.x = value_x_min + (value_x_max - value_x_min) / 2;
    value_offset.y = value_y_min + (value_y_max - value_y_min) / 2;
    value_offset.z = value_z_min + (value_z_max - value_z_min) / 2;
    
    PR_INFO("Calibration completed!");
    PR_INFO("X: min=%d, max=%d, offset=%d", value_x_min, value_x_max, value_offset.x);
    PR_INFO("Y: min=%d, max=%d, offset=%d", value_y_min, value_y_max, value_offset.y);
    PR_INFO("Z: min=%d, max=%d, offset=%d", value_z_min, value_z_max, value_offset.z);
    
    // Store calibration data
    g_bmm150_dev.calibration.x_min = value_x_min;
    g_bmm150_dev.calibration.x_max = value_x_max;
    g_bmm150_dev.calibration.y_min = value_y_min;
    g_bmm150_dev.calibration.y_max = value_y_max;
    g_bmm150_dev.calibration.z_min = value_z_min;
    g_bmm150_dev.calibration.z_max = value_z_max;
    g_bmm150_dev.calibration.x_offset = value_offset.x;
    g_bmm150_dev.calibration.y_offset = value_offset.y;
    g_bmm150_dev.calibration.z_offset = value_offset.z;
    g_bmm150_dev.calibration.calibrated = true;
    g_bmm150_dev.calibration.calibration_time = tal_system_get_millisecond();
    
    #else
    PR_INFO("Using default calibration offsets - calibration process disabled");
    
    // Set default calibration data
    g_bmm150_dev.calibration.x_offset = DEFAULT_X_OFFSET;
    g_bmm150_dev.calibration.y_offset = DEFAULT_Y_OFFSET;
    g_bmm150_dev.calibration.z_offset = DEFAULT_Z_OFFSET;
    g_bmm150_dev.calibration.calibrated = true;
    g_bmm150_dev.calibration.calibration_time = tal_system_get_millisecond();
    
    PR_INFO("Default calibration offsets: X=%d, Y=%d, Z=%d", 
            DEFAULT_X_OFFSET, DEFAULT_Y_OFFSET, DEFAULT_Z_OFFSET);
    #endif
    
    // Initialize live calibration system if enabled
    #if ENABLE_LIVE_CALIBRATION
    bmm150_live_cal_init(&g_bmm150_dev);
    PR_INFO("Live calibration system enabled");
    #endif
    
    // Print CSV header for one-liner CSV style output
    printf("CSV_HEADER,raw_x,raw_y,raw_z,raw_r,cal_x,cal_y,cal_z,offset_x,offset_y,offset_z,live_status,xy_heading,zx_heading,final_heading,direction,turbulence_status\n");

    while (1) {
        // Read magnetometer data using Grove driver algorithm
        op_ret = bmm150_read_mag_data(&g_bmm150_dev);
        if (op_ret != OPRT_OK) {
            PR_ERR("Failed to read magnetometer data (error: %d)", op_ret);
            tal_system_sleep(1000);
            continue;
        }
        
        // Apply calibration offsets (default or live)
        bmm150_mag_data_t value;
        
        #if ENABLE_LIVE_CALIBRATION
        // Update live calibration system
        bmm150_live_cal_update(&g_bmm150_dev);
        bmm150_live_cal_detect_turbulence(&g_bmm150_dev);
        
        // Apply live calibration offsets
        bmm150_live_cal_apply_offsets(&g_bmm150_dev, &value);
        #else
        // Apply default calibration offsets
        value.x = g_bmm150_dev.raw_mag_data.raw_datax - g_bmm150_dev.calibration.x_offset;
        value.y = g_bmm150_dev.raw_mag_data.raw_datay - g_bmm150_dev.calibration.y_offset;
        value.z = g_bmm150_dev.raw_mag_data.raw_dataz - g_bmm150_dev.calibration.z_offset;
        #endif
        
        // Calculate heading following Grove BMM150 demo pattern exactly
        float xyHeading = atan2(value.x, value.y);
        float zxHeading = atan2(value.z, value.x);
        float heading = xyHeading;
        
        // Normalize to 0-2π range
        if (heading < 0) {
            heading += 2 * 3.14159f;
        }
        if (heading > 2 * 3.14159f) {
            heading -= 2 * 3.14159f;
        }
        float heading_degrees = heading * 180.0f / 3.14159f;
        float xyHeading_degrees = xyHeading * 180.0f / 3.14159f;
        float zxHeading_degrees = zxHeading * 180.0f / 3.14159f;
        
        // Display compass direction
        const char* direction;
        if (heading_degrees >= 337.5 || heading_degrees < 22.5) {
            direction = "North";
        } else if (heading_degrees >= 22.5 && heading_degrees < 67.5) {
            direction = "Northeast";
        } else if (heading_degrees >= 67.5 && heading_degrees < 112.5) {
            direction = "East";
        } else if (heading_degrees >= 112.5 && heading_degrees < 157.5) {
            direction = "Southeast";
        } else if (heading_degrees >= 157.5 && heading_degrees < 202.5) {
            direction = "South";
        } else if (heading_degrees >= 202.5 && heading_degrees < 247.5) {
            direction = "Southwest";
        } else if (heading_degrees >= 247.5 && heading_degrees < 292.5) {
            direction = "West";
        } else {
            direction = "Northwest";
        }
        direction = direction;
        
        // Single formatted line with calibrated sensor data
        // Get turbulence status for one-liner output
        // const char* turbulence_status = "N/A";
        
        // #if ENABLE_LIVE_CALIBRATION
        // if (g_bmm150_dev.live_cal.turbulence_detected) {
        //     turbulence_status = "TURBULENCE";
        // } else {
        //     turbulence_status = "STABLE";
        // }
        // #endif
        
        // Get live calibration status
        uint8_t live_status = 0;
        #if ENABLE_LIVE_CALIBRATION
        live_status = bmm150_live_cal_get_status(&g_bmm150_dev);
        #endif
        
        // One-liner CSV style output with all data
        printf("BMM150_DATA,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%.1f,%.1f,%.1f,%x\n", 
               g_bmm150_dev.raw_mag_data.raw_datax, 
               g_bmm150_dev.raw_mag_data.raw_datay, 
               g_bmm150_dev.raw_mag_data.raw_dataz, 
               g_bmm150_dev.raw_mag_data.raw_data_r,
               value.x,  // calibrated X
               value.y,  // calibrated Y
               value.z,  // calibrated Z
               g_bmm150_dev.calibration.x_offset,  // fixed offset X
               g_bmm150_dev.calibration.y_offset,  // fixed offset Y
               g_bmm150_dev.calibration.z_offset,  // fixed offset Z
               live_status,  // live calibration status (1=turbulence/calibration needed, 0=working)
               xyHeading_degrees, 
               zxHeading_degrees, 
               heading_degrees,
               g_bmm150_dev.live_cal.turbulence_detected);  // turbulence status

        tal_system_sleep(100); // Read data every 100ms (10Hz)
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
    
    // Initialize CLI system
    PR_INFO("Initializing CLI system...");
    tal_cli_init();
    PR_INFO("CLI system initialized, registering BMM150 commands...");
    bmm150_cli_init();
    
    // Small delay to ensure CLI is fully initialized
    tal_system_sleep(100);
    
    PR_INFO("CLI initialization completed. Try: bmm150_test");
    
    static THREAD_CFG_T thrd_param = {.priority = TASK_GPIO_PRIORITY, .stackDepth = TASK_GPIO_SIZE, .thrdname = "bmm150"};
    TUYA_CALL_ERR_LOG(tal_thread_create_and_start(&sg_bmm150_handle, NULL, NULL, __example_bmm150_task, NULL, &thrd_param));

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
