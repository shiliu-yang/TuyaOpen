/**
 * @file app_gps.c
 * @brief GPS module driver for LC76G via UART
 * 
 * Features:
 * - UART interface (115200 baud)
 * - Parses GGA and RMC NMEA sentences
 * - Updates every 10 seconds
 * - Thread-safe data access
 * 
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "app_gps.h"
#include "app_dp.h"
#include "BNO08x.h"
#include "app_gps_calc.h"
#include "cloud_api.h"
#include "app_ui_main.h"
#include "tal_api.h"
#include <string.h>
#include <stdlib.h>

#include "tkl_gpio.h"
#include "tkl_uart.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define GPS_UART_PORT        TUYA_UART_NUM_2
#define GPS_UART_BAUDRATE    115200
#define GPS_RESET_PIN        9              /* GPIO9 for GPS reset */
#define GPS_BUFFER_SIZE      2048           /* UART read buffer */
#define GPS_UPDATE_INTERVAL  5000          /* 5 seconds */
#define GPS_TASK_STACK_SIZE  4096
#define GPS_TASK_PRIORITY    THREAD_PRIO_3

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/
static void gps_task(void *param);
static OPERATE_RET gps_uart_init(void);
static OPERATE_RET gps_hardware_reset(void);
static OPERATE_RET gps_configure_nmea_output(void);
static OPERATE_RET gps_send_pair062(uint8_t type, uint8_t output_rate);
static uint8_t gps_calculate_nmea_checksum(const char *sentence);
static OPERATE_RET gps_read_nmea(char *buffer, uint32_t size);
static void gps_parse_nmea(const char *buffer, uint32_t length);
static bool gps_validate_checksum(const char *sentence);
static void gps_parse_gga(const char *sentence);
static void gps_parse_rmc(const char *sentence);
static double gps_convert_latlon(const char *value, const char *hemisphere, bool is_latitude);
static void gps_extract_time(const char *time_str);
static void gps_upload_workq_cb(void *data);
static void gps_calc_workq_cb(void *data);

/***********************************************************
***********************variable define**********************
***********************************************************/
static THREAD_HANDLE sg_gps_task_handle = NULL;
static MUTEX_HANDLE sg_gps_mutex = NULL;
static app_gps_data_t sg_gps_data = {0};
static bool sg_gps_initialized = false;

/* GPS upload workqueue data */
typedef struct {
    double latitude;
    double longitude;
    int altitude_m;
} gps_upload_data_t;

typedef struct {
    double tracker_lat;
    double tracker_lon;
    double cattle_lat;
    double cattle_lon;
} gps_calc_data_t;

static gps_upload_data_t sg_gps_upload_data = {0};
static gps_calc_data_t sg_gps_calc_data = {0};

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Initialize GPS module
 */
OPERATE_RET app_gps_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    
    if (sg_gps_initialized) {
        PR_WARN("GPS already initialized");
        return OPRT_OK;
    }
    
    PR_INFO("[GPS] Initializing GPS module...");
    
    /* Create mutex for thread-safe access */
    rt = tal_mutex_create_init(&sg_gps_mutex);
    if (rt != OPRT_OK) {
        PR_ERR("[GPS] Failed to create mutex: %d", rt);
        return rt;
    }
    
    /* Hardware reset GPS module */
    rt = gps_hardware_reset();
    if (rt != OPRT_OK) {
        PR_ERR("[GPS] Hardware reset failed: %d", rt);
        goto error;
    }
    
    /* Initialize UART interface */
    TUYA_CALL_ERR_GOTO(gps_uart_init(), error);
    
    /* Configure NMEA output (disable unnecessary sentences, reduce GGA/RMC rate) */
    TUYA_CALL_ERR_GOTO(gps_configure_nmea_output(), error);
    
    sg_gps_initialized = true;
    PR_INFO("[GPS] GPS module initialized successfully");
    return OPRT_OK;
    
error:
    if (sg_gps_mutex) {
        tal_mutex_release(sg_gps_mutex);
        sg_gps_mutex = NULL;
    }
    return rt;
}

/**
 * @brief Start GPS reading task
 */
OPERATE_RET app_gps_start(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (!sg_gps_initialized) {
        PR_ERR("[GPS] GPS not initialized");
        return OPRT_INVALID_PARM;
    }
    
    if (sg_gps_task_handle != NULL) {
        PR_WARN("[GPS] GPS task already running");
        return OPRT_OK;
    }
    
    PR_INFO("[GPS] Starting GPS task...");
    
    THREAD_CFG_T task_cfg = {
        .priority = GPS_TASK_PRIORITY,
        .stackDepth = GPS_TASK_STACK_SIZE,
        .thrdname = "gps"
    };
    
    TUYA_CALL_ERR_RETURN(tal_thread_create_and_start(&sg_gps_task_handle, NULL, NULL, 
                                                      gps_task, NULL, &task_cfg));
    
    PR_INFO("[GPS] GPS task started");
    return OPRT_OK;
}

/**
 * @brief Get current GPS data (thread-safe)
 */
OPERATE_RET app_gps_get_data(app_gps_data_t *data)
{
    if (!data) {
        return OPRT_INVALID_PARM;
    }
    
    if (!sg_gps_mutex) {
        return OPRT_COM_ERROR;
    }
    
    tal_mutex_lock(sg_gps_mutex);
    memcpy(data, &sg_gps_data, sizeof(app_gps_data_t));
    tal_mutex_unlock(sg_gps_mutex);
    
    return OPRT_OK;
}

/**
 * @brief Check if GPS has valid fix
 */
bool app_gps_is_fixed(void)
{
    bool fixed = false;
    
    if (sg_gps_mutex) {
        tal_mutex_lock(sg_gps_mutex);
        fixed = sg_gps_data.valid && (sg_gps_data.fix_quality > 0);
        tal_mutex_unlock(sg_gps_mutex);
    }
    
    return fixed;
}

/**
 * @brief Get GPS status (valid and satellite count)
 */
OPERATE_RET app_gps_get_status(bool *valid, uint8_t *satellite_count)
{
    if (!valid || !satellite_count) {
        return OPRT_INVALID_PARM;
    }
    
    if (!sg_gps_mutex) {
        return OPRT_COM_ERROR;
    }
    
    tal_mutex_lock(sg_gps_mutex);
    *valid = sg_gps_data.valid;
    *satellite_count = sg_gps_data.satellites_in_use;
    tal_mutex_unlock(sg_gps_mutex);
    
    return OPRT_OK;
}

/***********************************************************
******************Internal Functions************************
***********************************************************/

/**
 * @brief GPS reading task - runs every 10 seconds
 */
static void gps_task(void *param)
{
    (void)param;

    OPERATE_RET rt = OPRT_OK;
    
    char *buffer = NULL;
    uint32_t error_count = 0;
    
    PR_INFO("[GPS] GPS task running (update interval: %d seconds)", GPS_UPDATE_INTERVAL / 1000);
    
    /* Allocate buffer */
    buffer = (char *)tal_psram_malloc(GPS_BUFFER_SIZE);
    if (!buffer) {
        PR_ERR("[GPS] Failed to allocate buffer");
        return;
    }
    
    while (1) {
        /* Read NMEA data from UART */
        rt = gps_read_nmea(buffer, GPS_BUFFER_SIZE);

#if defined(ENABLE_DEBUG_VIRTUAL_SIMULATION) && (ENABLE_DEBUG_VIRTUAL_SIMULATION == 1)
        strncpy((char *)buffer, "$GNGGA,051746.000,3018.024840,N,12004.092300,E,1,22,0.66,709.565,M,-14.256,M,,*5D\r\n$GNRMC,075546.000,A,3018.024840,N,12004.092300,E,0.94,292.56,151025,,,A,V*05\r\n",
        GPS_BUFFER_SIZE);
        rt = OPRT_OK;
#endif

        if (rt == OPRT_OK) {
            /* Parse NMEA sentences */
            gps_parse_nmea(buffer, strlen(buffer));
            
            /* Reset error count on success */
            if (error_count > 0) {
                PR_INFO("[GPS] GPS data read recovered after %d errors", error_count);
                error_count = 0;
            }
            
            /* Log GPS status */
            tal_mutex_lock(sg_gps_mutex);
            bool valid = sg_gps_data.valid;
            uint8_t sats = sg_gps_data.satellites_in_use;
            uint8_t fix = sg_gps_data.fix_quality;
            double lat = sg_gps_data.latitude_deg;
            double lon = sg_gps_data.longitude_deg;
            float alt = sg_gps_data.altitude_m;
            tal_mutex_unlock(sg_gps_mutex);
            
            if (valid) {
                PR_INFO("[GPS] Fix: %d | Sats: %d | Pos: %.6f, %.6f | Alt: %.1fm", 
                        fix, sats, lat, lon, alt);
                
                /* Upload GPS data to cloud only if satellites >= 10 */
                if (sats >= 10) {
                    /* Store data for async upload */
                    sg_gps_upload_data.latitude = lat;
                    sg_gps_upload_data.longitude = lon;
                    sg_gps_upload_data.altitude_m = (int)alt;
                    
                    /* Schedule async upload via workqueue */
                    tal_workq_schedule(WORKQ_SYSTEM, gps_upload_workq_cb, NULL);
                    
                    PR_DEBUG("[GPS] Scheduled async upload (sats: %d)", sats);
                    
                    /* Store tracker position for async GPS calculation */
                    sg_gps_calc_data.tracker_lat = lat;
                    sg_gps_calc_data.tracker_lon = lon;

                    cattle_location_t cattle_loc = {0};
                    rt = cloud_api_get_cattle_location(&cattle_loc);
                    if (rt == OPRT_OK) {
                        sg_gps_calc_data.cattle_lat = cattle_loc.lat;
                        sg_gps_calc_data.cattle_lon = cattle_loc.lon;
                    }
                    /* Schedule async GPS calculation via workqueue */
                    tal_workq_schedule(WORKQ_SYSTEM, gps_calc_workq_cb, NULL);
                    
                    PR_DEBUG("[GPS] Scheduled async GPS calculation");
                } else {
                    PR_DEBUG("[GPS] Skip upload - insufficient satellites (sats: %d < 10)", sats);
                }
            } else {
                PR_DEBUG("[GPS] Searching satellites... (Sats: %d)", sats);
            }
        } else {
            error_count++;
            PR_ERR("[GPS] Failed to read GPS data (error: %d, count: %d)", rt, error_count);
        }
        
        /* Sleep for 5 seconds */
        tal_system_sleep(GPS_UPDATE_INTERVAL);
    }
    
    /* Cleanup (never reached in normal operation) */
    if (buffer) {
        tal_psram_free(buffer);
    }
}

/**
 * @brief Hardware reset GPS module
 */
static OPERATE_RET gps_hardware_reset(void)
{
    OPERATE_RET rt = OPRT_OK;

    PR_INFO("[GPS] Hardware resetting GPS module (pin %d)...", GPS_RESET_PIN);
    
    /* Configure reset pin as output */
    TUYA_GPIO_BASE_CFG_T pin_cfg = {
        .mode = TUYA_GPIO_PUSH_PULL,
        .direct = TUYA_GPIO_OUTPUT,
        .level = TUYA_GPIO_LEVEL_HIGH
    };
    
    TUYA_CALL_ERR_RETURN(tkl_gpio_init(GPS_RESET_PIN, &pin_cfg));
    
    /* Assert reset (active LOW) */
    tkl_gpio_write(GPS_RESET_PIN, TUYA_GPIO_LEVEL_LOW);
    PR_DEBUG("[GPS] GPS in RESET state (pin LOW)");
    tal_system_sleep(200);
    
    /* Release reset */
    tkl_gpio_write(GPS_RESET_PIN, TUYA_GPIO_LEVEL_HIGH);
    PR_DEBUG("[GPS] GPS RESET released (pin HIGH)");
    
    /* Wait for GPS to boot */
    PR_INFO("[GPS] Waiting for GPS to boot...");
    tal_system_sleep(2000);
    
    PR_INFO("[GPS] GPS hardware reset complete");
    return OPRT_OK;
}

/**
 * @brief Initialize UART interface for GPS
 */
static OPERATE_RET gps_uart_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_UART_BASE_CFG_T cfg = {
        .baudrate = GPS_UART_BAUDRATE,
        .databits = TUYA_UART_DATA_LEN_8BIT,
        .stopbits = TUYA_UART_STOP_LEN_1BIT,
        .parity = TUYA_UART_PARITY_TYPE_NONE,
        .flowctrl = TUYA_UART_FLOWCTRL_NONE
    };
    
    TUYA_CALL_ERR_RETURN(tkl_uart_init(GPS_UART_PORT, &cfg));
    
    PR_INFO("[GPS] UART initialized (Port: %d, Baudrate: %d)", GPS_UART_PORT, GPS_UART_BAUDRATE);
    return OPRT_OK;
}

/**
 * @brief Calculate NMEA sentence checksum
 * 
 * @param sentence NMEA sentence starting with '$', ending with '*' or '\0'
 * @return Calculated checksum
 */
static uint8_t gps_calculate_nmea_checksum(const char *sentence)
{
    if (!sentence || sentence[0] != '$') {
        return 0;
    }
    
    /* Find the end of the sentence (either '*' or end of string) */
    const char *start = sentence + 1;  /* Skip '$' */
    const char *end = strchr(start, '*');
    if (!end) {
        end = start + strlen(start);
    }
    
    /* Calculate XOR checksum */
    uint8_t checksum = 0;
    for (const char *p = start; p < end; p++) {
        checksum ^= (uint8_t)(*p);
    }
    
    return checksum;
}

/**
 * @brief Send PAIR062 command to configure NMEA sentence output
 * 
 * PAIR062 Command Format: $PAIR062,<type>,<output_rate>*hh<CR><LF>
 * 
 * @param type NMEA sentence type:
 *             0 = GGA (position, altitude, satellites)
 *             1 = GLL (position)
 *             2 = GSA (DOP and active satellites)
 *             3 = GSV (satellites in view)
 *             4 = RMC (position, time, speed, course)
 *             5 = VTG (course and speed)
 *             8 = GST (position error statistics)
 * @param output_rate Output rate:
 *                    0 = Disable
 *                    N = Output once every N position fixes (1-20)
 * @return OPERATE_RET OPRT_OK on success
 */
static OPERATE_RET gps_send_pair062(uint8_t type, uint8_t output_rate)
{
    char send_buf[40] = {0};
    char recv_buf[64] = {0};
    
    /* Build PAIR062 command */
    int len = snprintf(send_buf, sizeof(send_buf), "$PAIR062,%d,%d*", type, output_rate);
    
    /* Calculate and append checksum */
    uint8_t checksum = gps_calculate_nmea_checksum(send_buf);
    len += snprintf(send_buf + len, sizeof(send_buf) - len, "%02X\r\n", checksum);
    
    PR_DEBUG("[GPS] Sending PAIR062: %s", send_buf);
    
    /* Send command */
    int bytes_sent = tkl_uart_write(GPS_UART_PORT, (uint8_t *)send_buf, len);
    if (bytes_sent != len) {
        PR_ERR("[GPS] Failed to send PAIR062 command");
        return OPRT_COM_ERROR;
    }
    
    /* Wait for response with timeout */
    tal_system_sleep(100);  /* Give GPS time to process and respond */
    
    /* Read response: $PAIR001,062,<result>*hh<CR><LF> */
    uint32_t start_time = tal_system_get_millisecond();
    uint32_t timeout_ms = 1000;
    int total_read = 0;
    
    while (total_read < (int)sizeof(recv_buf) - 1) {
        uint32_t fifo_len = tkl_uart_get_rxfifo_len(GPS_UART_PORT);
        if (fifo_len > 0) {
            int to_read = (fifo_len < sizeof(recv_buf) - total_read - 1) ? 
                          fifo_len : (sizeof(recv_buf) - total_read - 1);
            int bytes = tkl_uart_read(GPS_UART_PORT, (uint8_t *)(recv_buf + total_read), to_read);
            if (bytes > 0) {
                total_read += bytes;
            }
            
            /* Check if we have complete response (ends with \n) */
            if (total_read > 0 && recv_buf[total_read - 1] == '\n') {
                break;
            }
        }
        
        /* Check timeout */
        if ((tal_system_get_millisecond() - start_time) > timeout_ms) {
            PR_WARN("[GPS] PAIR062 response timeout");
            break;
        }
        
        tal_system_sleep(10);
    }
    
    recv_buf[total_read] = '\0';
    
    if (total_read == 0) {
        PR_WARN("[GPS] No response to PAIR062 (type=%d, rate=%d) - continuing anyway", type, output_rate);
        return OPRT_OK;  /* Some GPS modules don't respond, but command still works */
    }
    
    PR_DEBUG("[GPS] PAIR062 response: %s", recv_buf);
    
    /* Parse response: $PAIR001,062,<result>*hh */
    if (strstr(recv_buf, "PAIR001") && strstr(recv_buf, "062")) {
        /* Extract result code (0 = success) */
        char *result_str = strstr(recv_buf, "062,");
        if (result_str) {
            result_str += 4;  /* Skip "062," */
            int result = atoi(result_str);
            if (result == 0) {
                PR_DEBUG("[GPS] PAIR062 command successful (type=%d, rate=%d)", type, output_rate);
                return OPRT_OK;
            } else {
                PR_WARN("[GPS] PAIR062 command failed with code %d", result);
                return OPRT_COM_ERROR;
            }
        }
    }
    
    /* If we can't parse the response, assume success */
    PR_DEBUG("[GPS] PAIR062 response received (type=%d, rate=%d)", type, output_rate);
    return OPRT_OK;
}

/**
 * @brief Configure NMEA sentence output rates
 * 
 * Strategy:
 * - Enable GGA every 5 fixes (position, altitude, satellites)
 * - Enable RMC every 5 fixes (position, time, speed, course)
 * - Disable all other sentences (GLL, GSA, GSV, VTG, GST)
 * 
 * This reduces UART traffic and CPU load while maintaining essential GPS data.
 * With 10-second update interval, this means GPS data every ~50 seconds per sentence type.
 */
static OPERATE_RET gps_configure_nmea_output(void)
{
    PR_INFO("[GPS] Configuring NMEA sentence output...");
    
    /* Configure each sentence type */
    /* GGA: Essential - position, altitude, satellites - every 5 fixes */
    gps_send_pair062(0, 5);
    tal_system_sleep(100);
    
    /* GLL: Disable - redundant with GGA/RMC */
    gps_send_pair062(1, 0);
    tal_system_sleep(100);
    
    /* GSA: Disable - DOP info not needed */
    gps_send_pair062(2, 0);
    tal_system_sleep(100);
    
    /* GSV: Disable - satellite details not needed */
    gps_send_pair062(3, 0);
    tal_system_sleep(100);
    
    /* RMC: Essential - position, time, speed, course - every 5 fixes */
    gps_send_pair062(4, 5);
    tal_system_sleep(100);
    
    /* VTG: Disable - redundant with RMC */
    gps_send_pair062(5, 0);
    tal_system_sleep(100);
    
    /* GST: Disable - error statistics not needed */
    gps_send_pair062(8, 0);
    tal_system_sleep(100);
    
    PR_INFO("[GPS] NMEA configuration complete (GGA/RMC enabled at 1/5 rate, others disabled)");
    return OPRT_OK;
}

/**
 * @brief Read NMEA data from UART with timeout
 */
static OPERATE_RET gps_read_nmea(char *buffer, uint32_t size)
{
    if (!buffer || size == 0) {
        return OPRT_INVALID_PARM;
    }
    
    memset(buffer, 0, size);
    
    /* Read UART data with stabilization wait */
    uint32_t start_time = tal_system_get_millisecond();
    uint32_t last_len = 0;
    uint32_t stable_count = 0;
    const uint32_t timeout_ms = 1000;
    const uint32_t stable_delay_ms = 50;
    
    /* Wait for FIFO to stabilize (no new data for 50ms) */
    while (1) {
        uint32_t current_len = tkl_uart_get_rxfifo_len(GPS_UART_PORT);
        
        if (current_len == last_len) {
            stable_count++;
            if (stable_count >= (stable_delay_ms / 10)) {
                /* FIFO stable, ready to read */
                break;
            }
        } else {
            /* New data arrived, reset counter */
            last_len = current_len;
            stable_count = 0;
            start_time = tal_system_get_millisecond();
        }
        
        /* Check timeout */
        if ((tal_system_get_millisecond() - start_time) > timeout_ms) {
            if (last_len == 0) {
                PR_WARN("[GPS] No data received within timeout");
                return OPRT_COM_ERROR;
            }
            break;
        }
        
        tal_system_sleep(10);
    }
    
    /* Read available data */
    uint32_t read_len = (last_len < size - 1) ? last_len : (size - 1);
    if (read_len == 0) {
        return OPRT_COM_ERROR;
    }
    
    int bytes = tkl_uart_read(GPS_UART_PORT, (uint8_t *)buffer, read_len);
    if (bytes <= 0) {
        PR_ERR("[GPS] UART read error: %d", bytes);
        return OPRT_COM_ERROR;
    }
    
    buffer[bytes] = '\0';
    PR_DEBUG("[GPS] Read %d bytes from UART", bytes);
    
    return OPRT_OK;
}

/**
 * @brief Parse NMEA sentences from buffer
 */
static void gps_parse_nmea(const char *buffer, uint32_t length)
{
    if (!buffer || length == 0) {
        return;
    }
    
    /* Find and parse each NMEA sentence */
    const char *p = buffer;
    const char *end = buffer + length;
    
    while (p < end) {
        /* Find sentence start '$' */
        const char *start = memchr(p, '$', end - p);
        if (!start) {
            break;
        }
        
        /* Find sentence end '\n' */
        const char *line_end = memchr(start, '\n', end - start);
        if (!line_end) {
            line_end = end;
        }
        
        /* Extract sentence */
        size_t sentence_len = line_end - start;
        if (sentence_len >= 10 && sentence_len < 200) {
            char sentence[200];
            memcpy(sentence, start, sentence_len);
            sentence[sentence_len] = '\0';
            
            /* Remove trailing \r if present */
            if (sentence_len > 0 && sentence[sentence_len - 1] == '\r') {
                sentence[sentence_len - 1] = '\0';
            }
            
            /* Validate and parse */
            if (gps_validate_checksum(sentence)) {
                if (strstr(sentence, "GGA")) {
                    gps_parse_gga(sentence);
                } else if (strstr(sentence, "RMC")) {
                    gps_parse_rmc(sentence);
                }
            }
        }
        
        p = line_end + 1;
    }
}

/**
 * @brief Validate NMEA sentence checksum
 */
static bool gps_validate_checksum(const char *sentence)
{
    if (!sentence || sentence[0] != '$') {
        return false;
    }
    
    /* Find asterisk */
    const char *asterisk = strchr(sentence, '*');
    if (!asterisk || asterisk[1] == '\0' || asterisk[2] == '\0') {
        return false;
    }
    
    /* Calculate checksum (XOR of characters between $ and *) */
    uint8_t calculated = 0;
    for (const char *p = sentence + 1; p < asterisk; p++) {
        calculated ^= (uint8_t)(*p);
    }
    
    /* Parse provided checksum */
    char hex[3] = {asterisk[1], asterisk[2], '\0'};
    uint8_t provided = (uint8_t)strtoul(hex, NULL, 16);
    
    return (calculated == provided);
}

/**
 * @brief Parse GGA sentence (position and fix quality)
 * Format: $GPGGA,hhmmss.sss,ddmm.mmmm,N,dddmm.mmmm,E,fix,sats,hdop,alt,M,...*hh
 */
static void gps_parse_gga(const char *sentence)
{
    char buffer[200];
    strncpy(buffer, sentence, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    /* Tokenize */
    char *fields[16] = {NULL};
    int field_count = 0;
    char *token = strtok(buffer, ",*");
    
    while (token && field_count < 16) {
        fields[field_count++] = token;
        token = strtok(NULL, ",*");
    }
    
    if (field_count < 10) {
        return;
    }
    
    /* Extract fields */
    const char *time_str = fields[1];
    const char *lat_str = fields[2];
    const char *lat_hem = fields[3];
    const char *lon_str = fields[4];
    const char *lon_hem = fields[5];
    const char *fix_str = fields[6];
    const char *sats_str = fields[7];
    const char *alt_str = fields[9];
    
    /* Parse values */
    tal_mutex_lock(sg_gps_mutex);
    
    gps_extract_time(time_str);
    sg_gps_data.latitude_deg = gps_convert_latlon(lat_str, lat_hem, true);
    sg_gps_data.longitude_deg = gps_convert_latlon(lon_str, lon_hem, false);
    sg_gps_data.fix_quality = (fix_str && fix_str[0]) ? atoi(fix_str) : 0;
    sg_gps_data.satellites_in_use = (sats_str && sats_str[0]) ? atoi(sats_str) : 0;
    sg_gps_data.altitude_m = (alt_str && alt_str[0]) ? atof(alt_str) : 0.0f;
    sg_gps_data.valid = (sg_gps_data.fix_quality > 0);
    
    tal_mutex_unlock(sg_gps_mutex);
    
    PR_DEBUG("[GGA] Fix:%d Sats:%d Alt:%.1fm", 
             sg_gps_data.fix_quality, sg_gps_data.satellites_in_use, sg_gps_data.altitude_m);
}

/**
 * @brief Parse RMC sentence (position, time, speed, course)
 * Format: $GPRMC,hhmmss.sss,A,ddmm.mmmm,N,dddmm.mmmm,E,speed,course,ddmmyy,...*hh
 */
static void gps_parse_rmc(const char *sentence)
{
    char buffer[200];
    strncpy(buffer, sentence, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    /* Tokenize */
    char *fields[16] = {NULL};
    int field_count = 0;
    char *token = strtok(buffer, ",*");
    
    while (token && field_count < 16) {
        fields[field_count++] = token;
        token = strtok(NULL, ",*");
    }
    
    if (field_count < 10) {
        return;
    }
    
    /* Extract fields */
    const char *time_str = fields[1];
    const char *status = fields[2];
    const char *lat_str = fields[3];
    const char *lat_hem = fields[4];
    const char *lon_str = fields[5];
    const char *lon_hem = fields[6];
    const char *speed_knots_str = fields[7];
    const char *course_str = fields[8];
    
    /* Parse values */
    tal_mutex_lock(sg_gps_mutex);
    
    gps_extract_time(time_str);
    sg_gps_data.latitude_deg = gps_convert_latlon(lat_str, lat_hem, true);
    sg_gps_data.longitude_deg = gps_convert_latlon(lon_str, lon_hem, false);
    
    /* Convert speed from knots to km/h */
    double speed_knots = (speed_knots_str && speed_knots_str[0]) ? atof(speed_knots_str) : 0.0;
    sg_gps_data.speed_kmh = speed_knots * 1.852f;
    
    sg_gps_data.course_deg = (course_str && course_str[0]) ? atof(course_str) : 0.0f;
    
    /* Status: A=valid, V=invalid */
    bool rmc_valid = (status && status[0] == 'A');
    if (rmc_valid) {
        sg_gps_data.valid = true;
    }
    
    tal_mutex_unlock(sg_gps_mutex);
    
    PR_DEBUG("[RMC] Speed:%.1f km/h Course:%.1f° Status:%c", 
             sg_gps_data.speed_kmh, sg_gps_data.course_deg, status ? status[0] : '?');
}

/**
 * @brief Convert NMEA lat/lon format to decimal degrees
 * Latitude:  ddmm.mmmm -> dd.dddddd
 * Longitude: dddmm.mmmm -> ddd.dddddd
 */
static double gps_convert_latlon(const char *value, const char *hemisphere, bool is_latitude)
{
    if (!value || !value[0]) {
        return 0.0;
    }
    
    /* Determine degree field width */
    int deg_width = is_latitude ? 2 : 3;
    
    if ((int)strlen(value) < deg_width + 2) {
        return 0.0;
    }
    
    /* Extract degrees */
    char deg_str[4] = {0};
    memcpy(deg_str, value, deg_width);
    int degrees = atoi(deg_str);
    
    /* Extract minutes */
    double minutes = atof(value + deg_width);
    
    /* Convert to decimal degrees */
    double decimal = (double)degrees + (minutes / 60.0);
    
    /* Apply hemisphere (S and W are negative) */
    if (hemisphere && (hemisphere[0] == 'S' || hemisphere[0] == 'W')) {
        decimal = -decimal;
    }
    
    return decimal;
}

/**
 * @brief Extract time from NMEA time string (hhmmss.sss)
 */
static void gps_extract_time(const char *time_str)
{
    if (!time_str || strlen(time_str) < 6) {
        return;
    }
    
    char buf[3] = {0};
    
    /* Extract hours */
    memcpy(buf, time_str, 2);
    sg_gps_data.utc_hour = atoi(buf);
    
    /* Extract minutes */
    memcpy(buf, time_str + 2, 2);
    sg_gps_data.utc_minute = atoi(buf);
    
    /* Extract seconds */
    memcpy(buf, time_str + 4, 2);
    sg_gps_data.utc_second = atoi(buf);
    
    /* Extract milliseconds if present */
    const char *dot = strchr(time_str, '.');
    if (dot && dot[1]) {
        char ms_buf[4] = {0};
        int i = 0;
        dot++;
        while (i < 3 && *dot && *dot != ',' && *dot != '*') {
            ms_buf[i++] = *dot++;
        }
        sg_gps_data.utc_millisecond = atoi(ms_buf);
    }
}

/**
 * @brief Workqueue callback for async GPS data upload
 * 
 * This callback runs in the system workqueue thread context,
 * ensuring non-blocking GPS data upload to the cloud.
 * 
 * @param data Unused (data is stored in sg_gps_upload_data)
 */
static void gps_upload_workq_cb(void *data)
{
    (void)data;
    
    /* Get GPS data from global variable */
    double lat = sg_gps_upload_data.latitude;
    double lon = sg_gps_upload_data.longitude;
    int alt = sg_gps_upload_data.altitude_m;
    
    PR_DEBUG("[GPS Upload] Starting async upload - Pos: %.6f, %.6f, Alt: %dm", lat, lon, alt);
    
    /* Upload position and altitude to cloud */
    app_dp_gps_position_upload(lat, lon);
    app_dp_gps_height_upload(alt);
    
    PR_DEBUG("[GPS Upload] Async upload completed");
}

/**
 * @brief Workqueue callback for async GPS distance and bearing calculation
 * 
 * This callback runs in the system workqueue thread context,
 * calculates distance and bearing from tracker to cattle using GPS calculation module,
 * and updates the tracker UI.
 * 
 * @param data Unused (data is stored in sg_gps_calc_data)
 */
static void gps_calc_workq_cb(void *data)
{
    (void)data;

    OPERATE_RET rt = OPRT_OK;

    /* Get tracker GPS position from global variable */
    double tracker_lat = sg_gps_calc_data.tracker_lat;
    double tracker_lon = sg_gps_calc_data.tracker_lon;

    double cattle_lat = sg_gps_calc_data.cattle_lat;
    double cattle_lon = sg_gps_calc_data.cattle_lon;
    PR_DEBUG("[GPS Calc] Starting async calculation - Tracker: %.6f, %.6f, Cattle: %.6f, %.6f", tracker_lat, tracker_lon, cattle_lat, cattle_lon);

    /* Calculate distance and bearing using GPS calculation module */
    app_gps_calc_result_t calc_result;
    rt = app_gps_calc_distance_and_bearing(
        tracker_lat,
        tracker_lon,
        cattle_lat,
        cattle_lon,
        &calc_result
    );
    
    if (rt != OPRT_OK || !calc_result.valid) {
        PR_ERR("[GPS Calc] GPS calculation failed: rt=%d, valid=%d", rt, calc_result.valid);
        return;
    }
    
    /* Log calculation results */
    PR_INFO("[GPS Calc] Distance: %.2f m, Bearing: %.1f°", 
            calc_result.distance_meters, calc_result.bearing_degrees);

    // get heading from compass
    float heading_degrees = bno08x_get_yaw_degree();
    // update ui tracker
    app_ui_tracker_target_update((uint32_t)calc_result.distance_meters, heading_degrees, calc_result.bearing_degrees);

    
    PR_DEBUG("[GPS Calc] Async calculation completed");
}
