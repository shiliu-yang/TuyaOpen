/**
 * @file app_sensor_uart.h
 * @brief Sensor UART communication module
 * 
 * This module provides UART communication for external sensors.
 * 
 * Fixed configuration: 460800,8N1
 * 
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __APP_SENSOR_UART_H__
#define __APP_SENSOR_UART_H__

#include "tuya_cloud_types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
/* Fixed UART configuration: 460800,8N1 */
#define SENSOR_UART_PORT     TUYA_UART_NUM_2
#define SENSOR_UART_BAUDRATE 460800
#define SENSOR_UART_BUFFER_SIZE 1024

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Initialize sensor UART (460800,8N1)
 * 
 * @param port UART port number
 * @return OPERATE_RET OPRT_OK on success
 */
OPERATE_RET app_sensor_uart_init(uint32_t port);

/**
 * @brief Send data via sensor UART
 * 
 * @param data Pointer to data buffer to send
 * @param len Length of data to send
 * @return int Number of bytes sent, or negative error code
 */
int app_sensor_uart_send(const uint8_t *data, uint32_t len);

/**
 * @brief Read data from sensor UART
 * 
 * @param buffer Pointer to buffer to store received data
 * @param size Size of buffer
 * @param timeout_ms Timeout in milliseconds (0 = non-blocking)
 * @return int Number of bytes read, or negative error code
 */
int app_sensor_uart_read(uint8_t *buffer, uint32_t size, uint32_t timeout_ms);

/**
 * @brief Get number of bytes available in UART RX FIFO
 * 
 * @return uint32_t Number of bytes available
 */
uint32_t app_sensor_uart_available(void);

/**
 * @brief Flush UART TX buffer (wait for all data to be sent)
 * 
 * @return OPERATE_RET OPRT_OK on success
 */
OPERATE_RET app_sensor_uart_flush(void);

/**
 * @brief Deinitialize sensor UART
 * 
 * @return OPERATE_RET OPRT_OK on success
 */
OPERATE_RET app_sensor_uart_deinit(void);

/**
 * @brief Start sensor UART communication task
 * 
 * This function creates a thread that:
 * - Continuously reads data from UART and prints it
 * - Sends "hello" message every 5 seconds
 * 
 * @return OPERATE_RET OPRT_OK on success
 */
OPERATE_RET app_sensor_uart_start(void);

/**
 * @brief Sensor type enumeration
 */
typedef enum {
    SENSOR_TYPE_BNO = 0x00,  /* BNO sensor */
    SENSOR_TYPE_GPS = 0x01   /* GPS sensor */
} sensor_type_e;

/**
 * @brief Sensor control command enumeration
 */
typedef enum {
    SENSOR_CMD_STOP  = 0x00,  /* Stop sensor */
    SENSOR_CMD_START = 0x01   /* Start sensor */
} sensor_cmd_e;

/**
 * @brief Send sensor control command
 * 
 * Frame format: AA 55 [type] [time_h] [time_l] [cmd]
 * - Header: AA 55 (fixed)
 * - type: Sensor type (0x00=BNO, 0x01=GPS)
 * - time_h, time_l: Time interval in milliseconds (big-endian)
 * - cmd: Command (0x00=Stop, 0x01=Start)
 * 
 * @param type Sensor type
 * @param interval_ms Time interval in milliseconds
 * @param cmd Command (start/stop)
 * @return int Number of bytes sent, or negative error code
 */
int app_sensor_uart_send_cmd(sensor_type_e type, uint16_t interval_ms, sensor_cmd_e cmd);

/**
 * @brief BNO sensor data structure
 */
typedef struct {
    float heading;     /* Heading/Yaw angle in degrees (0-360) */
    bool valid;        /* Data validity */
} bno_sensor_data_t;

/**
 * @brief GNSS sensor data structure
 */
typedef struct {
    double latitude;   /* Latitude in degrees */
    double longitude;  /* Longitude in degrees */
    float altitude;    /* Altitude in meters (NaN if invalid) */
    uint8_t satellites;/* Number of satellites */
    bool valid;        /* Data validity (true if satellites > 0) */
} gnss_sensor_data_t;

/**
 * @brief Parse BNO sensor data
 * 
 * Data format: "bno:296.96"
 * - "bno:" prefix
 * - heading: Yaw/Heading angle in degrees
 * 
 * @param data_str Input string containing BNO data
 * @param bno_data Output structure for parsed BNO data
 * @return OPERATE_RET OPRT_OK on success, error code otherwise
 */
OPERATE_RET app_sensor_uart_parse_bno(const char *data_str, bno_sensor_data_t *bno_data);

/**
 * @brief Parse GNSS sensor data
 * 
 * Data format: "gnss:lat,lon,alt,sats"
 * Example: "gnss:39.908722,116.397496,50.5,12"
 *          "gnss:0.000000,0.000000,nan,0" (no fix)
 * 
 * @param data_str Input string containing GNSS data
 * @param gnss_data Output structure for parsed GNSS data
 * @return OPERATE_RET OPRT_OK on success, error code otherwise
 */
OPERATE_RET app_sensor_uart_parse_gnss(const char *data_str, gnss_sensor_data_t *gnss_data);

#ifdef __cplusplus
}
#endif

#endif /* __APP_SENSOR_UART_H__ */
