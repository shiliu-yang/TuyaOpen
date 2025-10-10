/**
 * @file sensor_integration.c
 * @brief Integration wrapper for BMM150 and GPS sensors
 *
 * This source provides unified implementation for the BMM150 magnetometer and LC76G GPS module.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "sensor_integration.h"
#include "bmm150.h"
#include "lc76g.h"
#include "dev_config.h"

#include "tal_log.h"
#include "tal_thread.h"
#include "tal_system.h"
#include <math.h>

/***********************************************************
************************macro define************************
***********************************************************/
#define TASK_BMM150_PRIORITY THREAD_PRIO_2
#define TASK_BMM150_SIZE     4096
#define TASK_GPS_PRIORITY    THREAD_PRIO_2
#define TASK_GPS_SIZE        4096

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static THREAD_HANDLE sg_bmm150_handle = NULL;
static THREAD_HANDLE sg_gps_handle = NULL;
static bmm150_dev_t g_bmm150_dev;
static lc76g_dev_t g_gps_dev;
static sensor_data_t g_sensor_data = {0};
static MUTEX_HANDLE g_sensor_mutex = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief BMM150 sensor reading task
 */
static void __bmm150_task(void *param)
{
    OPERATE_RET op_ret = OPRT_OK;

    PR_INFO("[BMM150] Task started - initializing sensor...");

    // Initialize device structure
    g_bmm150_dev.i2c_addr = BMM150_ADDRESS;
    
    // Initialize I2C using BMM150's dev_config
    op_ret = bmm150_i2c_init();
    if (op_ret != OPRT_OK) {
        PR_ERR("[BMM150] Failed to initialize I2C (error: %d)", op_ret);
        tal_thread_delete(NULL);
        return;
    }
    
    // Initialize BMM150 sensor
    op_ret = bmm150_init(&g_bmm150_dev, BMM150_ADDRESS);
    if (op_ret != OPRT_OK) {
        PR_ERR("[BMM150] Failed to initialize sensor (error: %d)", op_ret);
        tal_thread_delete(NULL);
        return;
    }

    PR_INFO("[BMM150] Initialized successfully!");
    
    // Set default calibration offsets (from BMM150 app)
    #define DEFAULT_X_OFFSET -80
    #define DEFAULT_Y_OFFSET -190
    #define DEFAULT_Z_OFFSET -7029
    
    g_bmm150_dev.calibration.x_offset = DEFAULT_X_OFFSET;
    g_bmm150_dev.calibration.y_offset = DEFAULT_Y_OFFSET;
    g_bmm150_dev.calibration.z_offset = DEFAULT_Z_OFFSET;
    g_bmm150_dev.calibration.calibrated = true;
    g_bmm150_dev.calibration.calibration_time = tal_system_get_millisecond();
    
    PR_INFO("[BMM150] Using default calibration: X=%d, Y=%d, Z=%d", 
            DEFAULT_X_OFFSET, DEFAULT_Y_OFFSET, DEFAULT_Z_OFFSET);

    // Update sensor status
    tal_mutex_lock(g_sensor_mutex);
    g_sensor_data.bmm150_ready = true;
    tal_mutex_unlock(g_sensor_mutex);

    // Main reading loop
    while (1) {
        op_ret = bmm150_read_mag_data(&g_bmm150_dev);
        if (op_ret == OPRT_OK) {
            // Apply calibration
            bmm150_mag_data_t value;
            value.x = g_bmm150_dev.raw_mag_data.raw_datax - g_bmm150_dev.calibration.x_offset;
            value.y = g_bmm150_dev.raw_mag_data.raw_datay - g_bmm150_dev.calibration.y_offset;
            value.z = g_bmm150_dev.raw_mag_data.raw_dataz - g_bmm150_dev.calibration.z_offset;
            
            // Calculate heading
            float xyHeading = atan2(value.x, value.y);
            float heading = xyHeading;
            
            // Normalize to 0-360 range
            if (heading < 0) heading += 2 * M_PI;
            if (heading > 2 * M_PI) heading -= 2 * M_PI;
            float heading_degrees = heading * 180.0f / M_PI;
            
            // Update global sensor data
            tal_mutex_lock(g_sensor_mutex);
            g_sensor_data.heading_degrees = heading_degrees;
            g_sensor_data.mag_x = value.x;
            g_sensor_data.mag_y = value.y;
            g_sensor_data.mag_z = value.z;
            tal_mutex_unlock(g_sensor_mutex);
            
            // Print to console
            printf("BMM150_DATA,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%.1f\n", 
                   g_bmm150_dev.raw_mag_data.raw_datax, 
                   g_bmm150_dev.raw_mag_data.raw_datay, 
                   g_bmm150_dev.raw_mag_data.raw_dataz, 
                   g_bmm150_dev.raw_mag_data.raw_data_r,
                   value.x, value.y, value.z,
                   g_bmm150_dev.calibration.x_offset,
                   g_bmm150_dev.calibration.y_offset,
                   g_bmm150_dev.calibration.z_offset,
                   heading_degrees);
        } else {
            PR_ERR("[BMM150] Failed to read data (error: %d)", op_ret);
        }
        
        tal_system_sleep(100); // Read at 10Hz
    }
}

/**
 * @brief GPS sensor reading task
 */
static void __gps_task(void *param)
{
    OPERATE_RET op_ret = OPRT_OK;

    PR_INFO("[GPS] Task started - initializing GPS...");
    
    // Initialize I2C using GPS's dev_config
    op_ret = dev_i2c_init();
    if (op_ret != OPRT_OK) {
        PR_ERR("[GPS] Failed to initialize I2C (error: %d)", op_ret);
        tal_thread_delete(NULL);
        return;
    }
    
    // Initialize GPS module
    op_ret = lc76g_init(&g_gps_dev, LC76G_ADDRESS, DEVICE_ADDRESS_R);
    if (op_ret != OPRT_OK) {
        PR_ERR("[GPS] Failed to initialize GPS (error: %d)", op_ret);
        tal_thread_delete(NULL);
        return;
    }

    PR_INFO("[GPS] Initialized successfully!");
    
    // Update sensor status
    tal_mutex_lock(g_sensor_mutex);
    g_sensor_data.gps_ready = true;
    tal_mutex_unlock(g_sensor_mutex);

    // Main reading loop
    while (1) {
        lc76g_get_data(&g_gps_dev);
        const lc76g_state_t *s = lc76g_get_state();
        
        // Update global sensor data
        tal_mutex_lock(g_sensor_mutex);
        g_sensor_data.latitude_deg = s->latitude_deg;
        g_sensor_data.longitude_deg = s->longitude_deg;
        g_sensor_data.altitude_m = s->altitude_m;
        g_sensor_data.speed_kmh = s->speed_kmh;
        g_sensor_data.course_deg = s->course_deg;
        g_sensor_data.satellites_in_use = s->satellites_in_use;
        g_sensor_data.fix_quality = s->fix_quality;
        tal_mutex_unlock(g_sensor_mutex);
        
        // Print to console
        char datebuf[7] = {0};
        lc76g_get_data_ddmmyy(datebuf);
        printf("GPS_DATA,%02d:%02d:%02d.%03dZ,%.6f,%.6f,%.1f,%s,%d,%d,%d,%d,%.1f,%.1f\n",
               s->utc_hour, s->utc_minute, s->utc_second, s->utc_millisecond,
               s->latitude_deg, s->longitude_deg, s->altitude_m,
               datebuf,
               s->satellites_in_use, s->fix_quality, s->connect_state, s->signal_level_5,
               s->speed_kmh, s->course_deg);
        
        tal_system_sleep(1000); // Read at 1Hz
    }
}

// Track if system has been initialized
static bool g_system_initialized = false;

/**
 * @brief Initialize BMM150 magnetometer sensor
 */
OPERATE_RET sensor_bmm150_init(void)
{
    PR_INFO("[SENSOR] Initializing BMM150 magnetometer...");
    
    // Create mutex if not exists
    if (g_sensor_mutex == NULL) {
        OPERATE_RET ret = tal_mutex_create_init(&g_sensor_mutex);
        if (ret != OPRT_OK) {
            PR_ERR("[SENSOR] Failed to create mutex (error: %d)", ret);
            return ret;
        }
    }
    
    // Initialize system only once (GPIO, buttons, etc.)
    if (!g_system_initialized) {
        OPERATE_RET ret = dev_sys_init();
        if (ret != OPRT_OK) {
            PR_ERR("[SENSOR] Failed to initialize system (error: %d)", ret);
            return ret;
        }
        g_system_initialized = true;
        PR_INFO("[SENSOR] System initialized");
    }
    
    return OPRT_OK;
}

/**
 * @brief Initialize LC76G GPS module
 */
OPERATE_RET sensor_gps_init(void)
{
    PR_INFO("[SENSOR] Initializing LC76G GPS...");
    
    // Create mutex if not exists
    if (g_sensor_mutex == NULL) {
        OPERATE_RET ret = tal_mutex_create_init(&g_sensor_mutex);
        if (ret != OPRT_OK) {
            PR_ERR("[SENSOR] Failed to create mutex (error: %d)", ret);
            return ret;
        }
    }
    
    // Initialize system only once (GPIO, buttons, etc.)
    if (!g_system_initialized) {
        OPERATE_RET ret = dev_sys_init();
        if (ret != OPRT_OK) {
            PR_ERR("[SENSOR] Failed to initialize system (error: %d)", ret);
            return ret;
        }
        g_system_initialized = true;
        PR_INFO("[SENSOR] System initialized");
    }
    
    return OPRT_OK;
}

/**
 * @brief Start sensor reading tasks
 */
OPERATE_RET sensor_tasks_start(void)
{
    OPERATE_RET ret = OPRT_OK;
    
    PR_INFO("[SENSOR] Starting sensor tasks...");
    
    // Start BMM150 task
    #ifdef ENABLE_BMM150_SENSOR
    if (sg_bmm150_handle == NULL) {
        static THREAD_CFG_T bmm150_param = {
            .priority = TASK_BMM150_PRIORITY,
            .stackDepth = TASK_BMM150_SIZE,
            .thrdname = "bmm150"
        };
        ret = tal_thread_create_and_start(&sg_bmm150_handle, NULL, NULL, __bmm150_task, NULL, &bmm150_param);
        if (ret != OPRT_OK) {
            PR_ERR("[SENSOR] Failed to create BMM150 task (error: %d)", ret);
            return ret;
        }
        PR_INFO("[SENSOR] BMM150 task started");
    }
    #endif
    
    // Start GPS task
    #ifdef ENABLE_GPS_LC76G
    if (sg_gps_handle == NULL) {
        static THREAD_CFG_T gps_param = {
            .priority = TASK_GPS_PRIORITY,
            .stackDepth = TASK_GPS_SIZE,
            .thrdname = "gps"
        };
        ret = tal_thread_create_and_start(&sg_gps_handle, NULL, NULL, __gps_task, NULL, &gps_param);
        if (ret != OPRT_OK) {
            PR_ERR("[SENSOR] Failed to create GPS task (error: %d)", ret);
            return ret;
        }
        PR_INFO("[SENSOR] GPS task started");
    }
    #endif
    
    return OPRT_OK;
}

/**
 * @brief Get current sensor data
 */
OPERATE_RET sensor_get_data(sensor_data_t *data)
{
    if (data == NULL) {
        return OPRT_INVALID_PARM;
    }
    
    if (g_sensor_mutex == NULL) {
        return OPRT_COM_ERROR;
    }
    
    tal_mutex_lock(g_sensor_mutex);
    memcpy(data, &g_sensor_data, sizeof(sensor_data_t));
    tal_mutex_unlock(g_sensor_mutex);
    
    return OPRT_OK;
}

/**
 * @brief Print sensor readings to console
 */
void sensor_print_readings(void)
{
    sensor_data_t data;
    if (sensor_get_data(&data) == OPRT_OK) {
        PR_INFO("=== Sensor Readings ===");
        PR_INFO("BMM150: heading=%.1f° mag_x=%d mag_y=%d mag_z=%d ready=%d", 
                data.heading_degrees, data.mag_x, data.mag_y, data.mag_z, data.bmm150_ready);
        PR_INFO("GPS: lat=%.6f lon=%.6f alt=%.1fm sats=%d fix=%d ready=%d",
                data.latitude_deg, data.longitude_deg, data.altitude_m,
                data.satellites_in_use, data.fix_quality, data.gps_ready);
    }
}

