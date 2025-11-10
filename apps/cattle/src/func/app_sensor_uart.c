/**
 * @file app_sensor_uart.c
 * @brief Sensor UART communication module implementation
 * 
 * Features:
 * - Fixed configuration: 460800,8N1
 * - Configurable UART port
 * - Blocking and non-blocking read
 * - FIFO status checking
 * 
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "app_sensor_uart.h"
#include "cloud_api.h"

#include "app_gps_calc.h"
#include "app_ui_main.h"
#include "app_dp.h"

#include "tal_api.h"
#include <string.h>
#include <stdlib.h>

#include "tkl_uart.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define SENSOR_UART_CHECK_INTERVAL_MS  10   /* Check interval for blocking read */
#define SENSOR_UART_TASK_STACK_SIZE    (8*1024) /* Task stack size */
#define SENSOR_UART_TASK_PRIORITY      THREAD_PRIO_3 /* Task priority */
#define SENSOR_UART_SEND_INTERVAL_MS   5000 /* Send "hello" every 5 seconds */

/***********************************************************
********************function declaration********************
***********************************************************/
static void sensor_uart_task(void *args);
static void sensor_uart_send_start_cmd_work(void *data);
static void sensor_uart_send_stop_cmd_work(void *data);

/***********************************************************
***********************variable define**********************
***********************************************************/
static uint32_t sg_uart_port = 0;
static bool sg_uart_initialized = false;
static THREAD_HANDLE sg_uart_task_handle = NULL;

static uint32_t sg_ui_total_distance = 1000;
static float sg_ui_heading_degrees = 0.0f;
static float sg_ui_bearing_degrees = 0.0f;

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Initialize sensor UART with fixed configuration (460800,8N1)
 */
OPERATE_RET app_sensor_uart_init(uint32_t port)
{
    OPERATE_RET rt = OPRT_OK;
    
    if (sg_uart_initialized) {
        PR_WARN("[SENSOR_UART] Already initialized");
        return OPRT_OK;
    }
    
    PR_INFO("[SENSOR_UART] Initializing UART%d with fixed config: 460800,8N1", port);
    
    /* Configure UART with fixed settings: 460800,8N1 */
    TUYA_UART_BASE_CFG_T cfg = {
        .baudrate = SENSOR_UART_BAUDRATE,
        .databits = TUYA_UART_DATA_LEN_8BIT,
        .stopbits = TUYA_UART_STOP_LEN_1BIT,
        .parity = TUYA_UART_PARITY_TYPE_NONE,
        .flowctrl = TUYA_UART_FLOWCTRL_NONE
    };

    /* Initialize UART */
    TUYA_CALL_ERR_RETURN(tkl_uart_init(port, &cfg));
    
    sg_uart_port = port;
    sg_uart_initialized = true;
    
    PR_INFO("[SENSOR_UART] Initialized successfully on UART%d", port);
    return OPRT_OK;
}

/**
 * @brief Send data via sensor UART
 */
int app_sensor_uart_send(const uint8_t *data, uint32_t len)
{
    if (!sg_uart_initialized) {
        PR_ERR("[SENSOR_UART] Not initialized");
        return -OPRT_COM_ERROR;
    }
    
    if (!data || len == 0) {
        PR_ERR("[SENSOR_UART] Invalid parameters");
        return -OPRT_INVALID_PARM;
    }
    
    /* Send data */
    int bytes_sent = tkl_uart_write(sg_uart_port, (uint8_t *)data, len);
    
    if (bytes_sent < 0) {
        PR_ERR("[SENSOR_UART] Write error: %d", bytes_sent);
        return bytes_sent;
    }
    
    if ((uint32_t)bytes_sent != len) {
        PR_WARN("[SENSOR_UART] Partial write: %d/%d bytes", bytes_sent, len);
    }
    
    PR_DEBUG("[SENSOR_UART] Sent %d bytes", bytes_sent);
    return bytes_sent;
}

/**
 * @brief Send sensor control command
 * 
 * Frame format: AA 55 [type] [time_h] [time_l] [cmd]
 * Example: AA 55 00 07 D0 01 -> Start BNO sensor with 2000ms interval
 */
int app_sensor_uart_send_cmd(sensor_type_e type, uint16_t interval_ms, sensor_cmd_e cmd)
{
    uint8_t frame[6];
    
    /* Frame header */
    frame[0] = 0xAA;
    frame[1] = 0x55;
    
    /* Sensor type */
    frame[2] = (uint8_t)type;
    
    /* Time interval (big-endian: high byte first, low byte second) */
    frame[3] = (uint8_t)((interval_ms >> 8) & 0xFF);  /* High byte */
    frame[4] = (uint8_t)(interval_ms & 0xFF);         /* Low byte */
    
    /* Command */
    frame[5] = (uint8_t)cmd;
    
    /* Send command */
    int sent = app_sensor_uart_send(frame, sizeof(frame));
    
    if (sent > 0) {
        const char *type_str = (type == SENSOR_TYPE_BNO) ? "BNO" : 
                               (type == SENSOR_TYPE_GPS) ? "GPS" : "UNKNOWN";
        const char *cmd_str = (cmd == SENSOR_CMD_START) ? "START" : "STOP";
        
        PR_INFO("[SENSOR_UART] Sent command: %s %s, interval=%dms", 
                type_str, cmd_str, interval_ms);
        PR_DEBUG("[SENSOR_UART] Frame: %02X %02X %02X %02X %02X %02X",
                 frame[0], frame[1], frame[2], frame[3], frame[4], frame[5]);
    }
    
    return sent;
}

/**
 * @brief Parse BNO sensor data
 * 
 * Data format: "bno:296.96"
 * Example: "bno:296.96" -> heading = 296.96°
 */
OPERATE_RET app_sensor_uart_parse_bno(const char *data_str, bno_sensor_data_t *bno_data)
{
    if (!data_str || !bno_data) {
        PR_ERR("[SENSOR_UART] Invalid parameters for BNO parsing");
        return OPRT_INVALID_PARM;
    }
    
    /* Initialize output */
    bno_data->heading = 0.0f;
    bno_data->valid = false;
    
    /* Check prefix "bno:" */
    if (strncmp(data_str, "bno:", 4) != 0) {
        PR_WARN("[SENSOR_UART] Invalid BNO format, expected 'bno:' prefix");
        return OPRT_INVALID_PARM;
    }
    
    /* Parse heading value */
    const char *value_str = data_str + 4;  /* Skip "bno:" */
    char *endptr = NULL;
    float heading = strtof(value_str, &endptr);
    
    /* Check if parsing was successful */
    if (endptr == value_str) {
        PR_WARN("[SENSOR_UART] Failed to parse BNO heading value");
        return OPRT_INVALID_PARM;
    }
    
    /* Normalize heading to 0-360 range */
    while (heading < 0.0f) {
        heading += 360.0f;
    }
    while (heading >= 360.0f) {
        heading -= 360.0f;
    }
    
    /* Store parsed data */
    bno_data->heading = heading;
    bno_data->valid = true;
    
    // PR_INFO("[SENSOR_UART] BNO heading: %.2f°", heading);
    return OPRT_OK;
}

/**
 * @brief Parse GNSS sensor data
 * 
 * Data format: "gnss:lat,lon,alt,sats"
 * Example: "gnss:39.908722,116.397496,50.5,12"
 *          "gnss:0.000000,0.000000,nan,0" (no fix)
 */
OPERATE_RET app_sensor_uart_parse_gnss(const char *data_str, gnss_sensor_data_t *gnss_data)
{
    if (!data_str || !gnss_data) {
        PR_ERR("[SENSOR_UART] Invalid parameters for GNSS parsing");
        return OPRT_INVALID_PARM;
    }
    
    /* Initialize output */
    gnss_data->latitude = 0.0;
    gnss_data->longitude = 0.0;
    gnss_data->altitude = 0.0f;
    gnss_data->satellites = 0;
    gnss_data->valid = false;
    
    /* Check prefix "gnss:" */
    if (strncmp(data_str, "gnss:", 5) != 0) {
        PR_WARN("[SENSOR_UART] Invalid GNSS format, expected 'gnss:' prefix");
        return OPRT_INVALID_PARM;
    }
    
    /* Parse GNSS data: lat,lon,alt,sats */
    const char *value_str = data_str + 5;  /* Skip "gnss:" */
    
    double latitude = 0.0;
    double longitude = 0.0;
    char alt_str[32] = {0};
    int satellites = 0;
    
    /* Parse using sscanf */
    int parsed = sscanf(value_str, "%lf,%lf,%31[^,],%d", 
                       &latitude, &longitude, alt_str, &satellites);
    
    if (parsed < 4) {
        PR_WARN("[SENSOR_UART] Failed to parse GNSS data, parsed=%d fields", parsed);
        return OPRT_INVALID_PARM;
    }
    
    /* Parse altitude (handle "nan" case) */
    float altitude = 0.0f;
    if (strcmp(alt_str, "nan") == 0 || strcmp(alt_str, "NaN") == 0 || 
        strcmp(alt_str, "NAN") == 0) {
        altitude = 0.0f;  /* Use 0 for invalid altitude */
    } else {
        altitude = atof(alt_str);
    }
    
    /* Store parsed data */
    gnss_data->latitude = latitude;
    gnss_data->longitude = longitude;
    gnss_data->altitude = altitude;
    gnss_data->satellites = (uint8_t)(satellites > 0 ? satellites : 0);
    
    /* Data is valid if we have satellites */
    gnss_data->valid = (satellites > 0);
    
    if (gnss_data->valid) {
        PR_INFO("[SENSOR_UART] GNSS: lat=%.6f, lon=%.6f, alt=%.1fm, sats=%d",
               latitude, longitude, altitude, satellites);
    } else {
        PR_DEBUG("[SENSOR_UART] GNSS: No fix (sats=%d)", satellites);
    }
    
    return OPRT_OK;
}

/**
 * @brief Read data from sensor UART
 * 
 * This function supports both blocking and non-blocking reads:
 * - timeout_ms = 0: Non-blocking, returns immediately
 * - timeout_ms > 0: Blocking, waits up to timeout_ms for data
 */
int app_sensor_uart_read(uint8_t *buffer, uint32_t size, uint32_t timeout_ms)
{
    if (!sg_uart_initialized) {
        PR_ERR("[SENSOR_UART] Not initialized");
        return -OPRT_COM_ERROR;
    }
    
    if (!buffer || size == 0) {
        PR_ERR("[SENSOR_UART] Invalid parameters");
        return -OPRT_INVALID_PARM;
    }
    
    /* Non-blocking read */
    if (timeout_ms == 0) {
        uint32_t available = tkl_uart_get_rxfifo_len(sg_uart_port);
        if (available == 0) {
            return 0;  /* No data available */
        }
        
        uint32_t to_read = (available < size) ? available : size;
        int bytes = tkl_uart_read(sg_uart_port, buffer, to_read);
        
        if (bytes < 0) {
            PR_ERR("[SENSOR_UART] Read error: %d", bytes);
            return bytes;
        }
        
        PR_DEBUG("[SENSOR_UART] Read %d bytes (non-blocking)", bytes);
        return bytes;
    }
    
    /* Blocking read with timeout */
    uint32_t start_time = tal_system_get_millisecond();
    uint32_t total_read = 0;
    
    while (total_read < size) {
        /* Check available data */
        uint32_t available = tkl_uart_get_rxfifo_len(sg_uart_port);
        
        if (available > 0) {
            /* Read available data */
            uint32_t to_read = (available < (size - total_read)) ? 
                              available : (size - total_read);
            
            int bytes = tkl_uart_read(sg_uart_port, buffer + total_read, to_read);
            
            if (bytes < 0) {
                PR_ERR("[SENSOR_UART] Read error: %d", bytes);
                return (total_read > 0) ? (int)total_read : bytes;
            }
            
            total_read += bytes;
            
            /* If we got some data, reset timeout */
            start_time = tal_system_get_millisecond();
        }
        
        /* Check timeout */
        if ((tal_system_get_millisecond() - start_time) >= timeout_ms) {
            if (total_read == 0) {
                PR_DEBUG("[SENSOR_UART] Read timeout (no data)");
            } else {
                PR_DEBUG("[SENSOR_UART] Read timeout (partial: %d bytes)", total_read);
            }
            break;
        }
        
        /* Small delay to avoid busy-waiting */
        if (available == 0) {
            tal_system_sleep(SENSOR_UART_CHECK_INTERVAL_MS);
        }
    }
    
    PR_DEBUG("[SENSOR_UART] Read %d bytes (blocking, timeout: %dms)", total_read, timeout_ms);
    return (int)total_read;
}

/**
 * @brief Get number of bytes available in UART RX FIFO
 */
uint32_t app_sensor_uart_available(void)
{
    if (!sg_uart_initialized) {
        PR_ERR("[SENSOR_UART] Not initialized");
        return 0;
    }
    
    uint32_t available = tkl_uart_get_rxfifo_len(sg_uart_port);
    return available;
}

/**
 * @brief Flush UART TX buffer
 * 
 * Note: The underlying tkl_uart layer may not support explicit flush.
 * This function provides a delay to allow pending data to be transmitted.
 */
OPERATE_RET app_sensor_uart_flush(void)
{
    if (!sg_uart_initialized) {
        PR_ERR("[SENSOR_UART] Not initialized");
        return OPRT_COM_ERROR;
    }
    
    /* Wait for TX buffer to empty (approximate) */
    tal_system_sleep(10);
    
    PR_DEBUG("[SENSOR_UART] Flush completed");
    return OPRT_OK;
}

/**
 * @brief Deinitialize sensor UART
 */
OPERATE_RET app_sensor_uart_deinit(void)
{
    if (!sg_uart_initialized) {
        PR_WARN("[SENSOR_UART] Not initialized");
        return OPRT_OK;
    }
    
    PR_INFO("[SENSOR_UART] Deinitializing...");
    
    /* Delete task if running */
    if (sg_uart_task_handle != NULL) {
        tal_thread_delete(sg_uart_task_handle);
        sg_uart_task_handle = NULL;
        PR_INFO("[SENSOR_UART] Task stopped");
    }
    
    /* Deinitialize UART */
    OPERATE_RET rt = tkl_uart_deinit(sg_uart_port);
    if (rt != OPRT_OK) {
        PR_ERR("[SENSOR_UART] Deinit failed: %d", rt);
        return rt;
    }
    
    sg_uart_initialized = false;
    
    PR_INFO("[SENSOR_UART] Deinitialized successfully");
    return OPRT_OK;
}

/**
 * @brief Sensor UART communication task
 * 
 * This task:
 * 1. Continuously reads data from UART and prints it
 * 2. Sends "hello" message every 5 seconds
 */
static void sensor_uart_task(void *args)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t *rx_buffer = NULL;

    rx_buffer = tal_psram_malloc(SENSOR_UART_BUFFER_SIZE);
    if (NULL == rx_buffer) {
        PR_ERR("[SENSOR_UART] Failed to allocate memory");
    }

    // const char *hello_msg = "hello";
    // uint32_t last_send_time = 0;
    
    PR_INFO("[SENSOR_UART] Task started");

    // /* Start BNO sensor with 2000ms interval */
    // app_sensor_uart_send_cmd(SENSOR_TYPE_BNO, 200, SENSOR_CMD_START);
    // tal_system_sleep(100);

    // /* Start GPS sensor with 10000ms interval */
    // app_sensor_uart_send_cmd(SENSOR_TYPE_GPS, 10000, SENSOR_CMD_START);
    // tal_system_sleep(100);

    while (1) {
        if (NULL == rx_buffer) {
            tal_system_sleep(10 * 1000);
            continue;
        }

        uint32_t available = app_sensor_uart_available();
        if (available > 0) {
            tal_system_sleep(50);
            if (available != app_sensor_uart_available()) {
                continue;
            }
        } else {
            tal_system_sleep(100);
            continue;
        }

        memset(rx_buffer, 0, available);
        #if 1
        int bytes = tkl_uart_read(sg_uart_port, rx_buffer, available > SENSOR_UART_BUFFER_SIZE ? SENSOR_UART_BUFFER_SIZE : available);
        #else
        // gnss:30.300515,120.068617,50.00,10
        tal_system_sleep(10*1000);
        strncpy((char *)rx_buffer, "gnss:30.300515,120.068617,50.00,10", available > SENSOR_UART_BUFFER_SIZE ? SENSOR_UART_BUFFER_SIZE : available);
        int bytes = strlen((char *)rx_buffer);
        #endif
        if (bytes <= 0) {
            PR_ERR("[SENSOR_UART] Read error: %d", bytes);
            tal_system_sleep(100);
            continue;
        }

        /* Null-terminate the buffer */
        if (bytes < (int)SENSOR_UART_BUFFER_SIZE) {
            rx_buffer[bytes] = '\0';
        } else {
            rx_buffer[SENSOR_UART_BUFFER_SIZE - 1] = '\0';
        }

        // get cattle location
        cattle_location_t cattle_location;
        cloud_api_get_cattle_location(&cattle_location);
        float cattle_lat = cattle_location.lat;
        float cattle_lon = cattle_location.lon;
        float tracker_lat = 0.0f;
        float tracker_lon = 0.0f;
        
        /* Process data line by line (handle multiple lines in buffer) */
        char *line = (char *)rx_buffer;
        char *next_line = NULL;
        
        while (line != NULL && *line != '\0') {
            /* Find the next line */
            next_line = strchr(line, '\n');
            if (next_line != NULL) {
                *next_line = '\0';  /* Terminate current line */
                next_line++;        /* Move to start of next line */
            }
            
            /* Remove trailing '\r' if present */
            size_t len = strlen(line);
            if (len > 0 && line[len - 1] == '\r') {
                line[len - 1] = '\0';
            }
            
            /* Skip empty lines */
            if (strlen(line) == 0) {
                line = next_line;
                continue;
            }
            
            /* Try to parse BNO data */
            if (strncmp(line, "bno:", 4) == 0) {
                bno_sensor_data_t bno_data;
                if (app_sensor_uart_parse_bno(line, &bno_data) == OPRT_OK) {
                    if (bno_data.valid) {
                        /* BNO data parsed successfully */
                        // PR_INFO("[SENSOR_UART] BNO data: %s", line);
                        // PR_INFO("[SENSOR_UART] BNO heading: %.2f°", bno_data.heading);

                        if (abs(sg_ui_heading_degrees - bno_data.heading) > 3) {
                            sg_ui_heading_degrees = bno_data.heading;
                            app_ui_tracker_target_update(sg_ui_total_distance, sg_ui_heading_degrees, sg_ui_bearing_degrees);
                        }
                    }
                }
            }
            /* Try to parse GNSS data */
            else if (strncmp(line, "gnss:", 5) == 0) {
                gnss_sensor_data_t gnss_data;
                if (app_sensor_uart_parse_gnss(line, &gnss_data) == OPRT_OK) {
                    if (gnss_data.valid) {
                        /* GNSS data parsed successfully with valid fix */
                        // PR_INFO("[SENSOR_UART] GNSS data: %s", line);
                        // PR_INFO("[SENSOR_UART] GNSS latitude: %.6f", gnss_data.latitude);
                        // PR_INFO("[SENSOR_UART] GNSS longitude: %.6f", gnss_data.longitude);
                        // PR_INFO("[SENSOR_UART] GNSS altitude: %.1fm", gnss_data.altitude);
                        // PR_INFO("[SENSOR_UART] GNSS satellites: %d", gnss_data.satellites);

                        app_gps_calc_result_t calc_result;
                        tracker_lat = gnss_data.latitude;
                        tracker_lon = gnss_data.longitude;
                        rt = app_gps_calc_distance_and_bearing(tracker_lat, tracker_lon, cattle_lat, cattle_lon, &calc_result);
                        if (rt == OPRT_OK && calc_result.valid) {
                            sg_ui_total_distance = calc_result.distance_meters;
                            sg_ui_bearing_degrees = calc_result.bearing_degrees;
                            app_ui_tracker_target_update(sg_ui_total_distance, sg_ui_heading_degrees, sg_ui_bearing_degrees);

                            // update DP
                            app_dp_gps_position_upload(gnss_data.latitude, gnss_data.longitude);
                            app_dp_gps_height_upload((int)gnss_data.altitude);
                        }

                    } else {
                        /* GNSS data parsed but no fix (satellites = 0) */
                        PR_DEBUG("[SENSOR_UART] GNSS waiting for fix...");
                    }
                }
            }
            else {
                /* Unknown data format or empty line */
                if (strlen(line) > 0) {
                    PR_WARN("[SENSOR_UART] Unknown data format: %s", line);
                }
            }
            
            /* Move to next line */
            line = next_line;
        }

        /* Sleep for a short interval */
        tal_system_sleep(100);
    }
}

/**
 * @brief Work queue callback to send sensor start commands
 */
static void sensor_uart_send_start_cmd_work(void *data)
{
    (void)data; /* Unused parameter */
    
    if (!sg_uart_initialized) {
        PR_ERR("[SENSOR_UART] Not initialized, cannot send start commands");
        return;
    }
    
    PR_INFO("[SENSOR_UART] Sending sensor start commands via work queue");
    
    /* Start BNO sensor with 200ms interval */
    app_sensor_uart_send_cmd(SENSOR_TYPE_BNO, 200, SENSOR_CMD_START);
    tal_system_sleep(100);
    
    /* Start GPS sensor with 10000ms interval */
    app_sensor_uart_send_cmd(SENSOR_TYPE_GPS, 10000, SENSOR_CMD_START);
    tal_system_sleep(100);
    
    PR_INFO("[SENSOR_UART] Sensor start commands sent successfully");
}

/**
 * @brief Start receiving sensor data by sending start commands asynchronously
 * 
 * This function schedules a work queue task to send sensor start commands
 * without blocking the caller.
 */
OPERATE_RET app_sensor_uart_recv_start(void)
{
    if (!sg_uart_initialized) {
        PR_ERR("[SENSOR_UART] Not initialized, call app_sensor_uart_init first");
        return OPRT_COM_ERROR;
    }
    
    PR_INFO("[SENSOR_UART] Scheduling sensor start commands via work queue");
    
    /* Schedule work to send start commands asynchronously */
    OPERATE_RET rt = tal_workq_schedule(WORKQ_SYSTEM, sensor_uart_send_start_cmd_work, NULL);
    if (rt != OPRT_OK) {
        PR_ERR("[SENSOR_UART] Failed to schedule work: %d", rt);
        return rt;
    }
    
    PR_DEBUG("[SENSOR_UART] Work scheduled successfully");
    return OPRT_OK;
}

/**
 * @brief Work queue callback to send sensor stop commands
 */
static void sensor_uart_send_stop_cmd_work(void *data)
{
    (void)data; /* Unused parameter */
    
    if (!sg_uart_initialized) {
        PR_ERR("[SENSOR_UART] Not initialized, cannot send stop commands");
        return;
    }
    
    PR_INFO("[SENSOR_UART] Sending sensor stop commands via work queue");
    
    /* Stop BNO sensor */
    app_sensor_uart_send_cmd(SENSOR_TYPE_BNO, 0, SENSOR_CMD_STOP);
    tal_system_sleep(100);
    
    /* Stop GPS sensor */
    app_sensor_uart_send_cmd(SENSOR_TYPE_GPS, 0, SENSOR_CMD_STOP);
    tal_system_sleep(100);
    
    PR_INFO("[SENSOR_UART] Sensor stop commands sent successfully");
}

/**
 * @brief Stop receiving sensor data by sending stop commands asynchronously
 * 
 * This function schedules a work queue task to send sensor stop commands
 * without blocking the caller.
 */
OPERATE_RET app_sensor_uart_recv_stop(void)
{
    if (!sg_uart_initialized) {
        PR_ERR("[SENSOR_UART] Not initialized, call app_sensor_uart_init first");
        return OPRT_COM_ERROR;
    }
    
    PR_INFO("[SENSOR_UART] Scheduling sensor stop commands via work queue");
    
    /* Schedule work to send stop commands asynchronously */
    OPERATE_RET rt = tal_workq_schedule(WORKQ_SYSTEM, sensor_uart_send_stop_cmd_work, NULL);
    if (rt != OPRT_OK) {
        PR_ERR("[SENSOR_UART] Failed to schedule work: %d", rt);
        return rt;
    }
    
    PR_DEBUG("[SENSOR_UART] Work scheduled successfully");
    return OPRT_OK;
}

/**
 * @brief Start sensor UART communication task
 */
OPERATE_RET app_sensor_uart_start(void)
{
    OPERATE_RET rt = OPRT_OK;
    
    if (!sg_uart_initialized) {
        PR_ERR("[SENSOR_UART] Not initialized, call app_sensor_uart_init first");
        return OPRT_COM_ERROR;
    }
    
    if (sg_uart_task_handle != NULL) {
        PR_WARN("[SENSOR_UART] Task already running");
        return OPRT_OK;
    }
    
    PR_INFO("[SENSOR_UART] Starting communication task...");
    
    /* Create and start task */
    THREAD_CFG_T task_cfg = {
        .priority = SENSOR_UART_TASK_PRIORITY,
        .stackDepth = SENSOR_UART_TASK_STACK_SIZE,
        .thrdname = "sensor_uart"
    };
    
    rt = tal_thread_create_and_start(&sg_uart_task_handle, NULL, NULL, 
                                      sensor_uart_task, NULL, &task_cfg);
    if (rt != OPRT_OK) {
        PR_ERR("[SENSOR_UART] Failed to create task: %d", rt);
        return rt;
    }
    
    PR_INFO("[SENSOR_UART] Task started successfully");
    return OPRT_OK;
}
