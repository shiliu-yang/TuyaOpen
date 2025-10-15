/**
 * @file lc76g.c
 * @brief LC76G GPS Module Driver - Supports both I2C and UART interfaces
 *
 * Debug/Logging Controls:
 * -----------------------
 * LC76G_ENABLE_NMEA_LOGS - Controlled by Kconfig
 *   - Enable via: CONFIG_LC76G_ENABLE_NMEA_LOGS=y in Kconfig
 *   - Shows detailed NMEA sentence parsing logs
 *   - Configure via menuconfig: Application config → Enable detailed NMEA sentence parsing logs
 */

#include "lc76g.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

uint8_t readData[4];

static lc76g_state_t g_state = {.utc_hour = 0,
                                .utc_minute = 0,
                                .utc_second = 0,
                                .utc_millisecond = 0,
                                .latitude_deg = 0.0,
                                .longitude_deg = 0.0,
                                .altitude_m = 0.0,
                                .date_ddmmyy = {0},
                                .fix_quality = 0,
                                .satellites_in_use = 0,
                                .connect_state = 0,
                                .signal_level_5 = 0,
                                .speed_kmh = 0.0,
                                .course_deg = 0.0,
                                .last_status = false};

// Status logging control (reduce frequency)
static uint32_t g_log_counter = 0;

// ----------------------
// NMEA parsing utilities
// ----------------------

static uint8_t nmea_checksum_calculate(const char *message)
{
    if (message == NULL) {
        return -1;
    }

    // 查找起始符'$'
    const char *start = strchr(message, '$');
    if (start == NULL) {
        return -1; // 没有找到起始符
    }

    // 查找结束符'*'，如果没有则计算到字符串末尾
    const char *end = strchr(start, '*');
    if (end == NULL) {
        end = start + strlen(start);
    }

    // 计算校验和 (跳过$符号，从下一个字符开始)
    uint8_t checksum = 0;
    for (const char *ptr = start + 1; ptr < end; ptr++) {
        checksum ^= (uint8_t)(*ptr);
    }

    return checksum;
}

static bool nmea_validate(const char *line)
{
    // Expect format: $...*HH[\r][\n]
    const char *start = line;
    if (!start || start[0] != '$')
        return false;
    const char *asterisk = strchr(start, '*');
    if (!asterisk || (asterisk - start) < 1)
        return false;

    // Read hex checksum after '*'
    if (!asterisk[1] || !asterisk[2])
        return false;
    char hex[3];
    hex[0] = asterisk[1];
    hex[1] = asterisk[2];
    hex[2] = '\0';
    uint8_t provided = (uint8_t)strtoul(hex, NULL, 16);

    // Compute checksum over characters between '$' and '*'
    uint8_t computed = 0;
    for (const char *p = start + 1; p < asterisk; ++p) {
        computed ^= (uint8_t)(*p);
    }
    return computed == provided;
}

static int split_fields(char *line, char **fields, int max_fields)
{
    // Tokenize in place up to '*' or end; commas separate fields
    int count = 0;
    char *p = line;
    // Skip leading '$'
    if (*p == '$')
        p++;
    fields[count++] = p;
    while (*p && *p != '*' && count < max_fields) {
        if (*p == ',') {
            *p = '\0';
            fields[count++] = p + 1;
        }
        p++;
    }
    return count;
}

#if LC76G_ENABLE_NMEA_LOGS
static const char *safe_str(const char *s)
{
    return (s && s[0]) ? s : "-";
}
#endif

static void parse_hms(const char *utc, int *hh, int *mm, int *ss, int *ms)
{
    // utc like HHMMSS or HHMMSS.sss
    *hh = *mm = *ss = *ms = 0;
    if (!utc || utc[0] == '\0')
        return;
    int len = (int)strlen(utc);
    if (len < 6)
        return;
    char buf[4] = {0};
    memcpy(buf, utc, 2);
    *hh = atoi(buf);
    memcpy(buf, utc + 2, 2);
    *mm = atoi(buf);
    memcpy(buf, utc + 4, 2);
    *ss = atoi(buf);
    const char *dot = strchr(utc, '.');
    if (dot && dot[1]) {
        char msbuf[4] = {0};
        // take up to 3 digits
        int i = 0;
        dot++;
        while (i < 3 && *dot && *dot != ',') {
            msbuf[i++] = *dot++;
        }
        *ms = atoi(msbuf);
    }
}

static double parse_latlon(const char *val, const char *hemi, bool is_lat)
{
    // val like ddmm.mmmm (lat) or dddmm.mmmm (lon)
    if (!val || val[0] == '\0')
        return 0.0;
    int deg_len = is_lat ? 2 : 3;
    if ((int)strlen(val) < deg_len + 2)
        return 0.0;
    char degbuf[4] = {0};
    memcpy(degbuf, val, deg_len);
    int degrees = atoi(degbuf);
    double minutes = atof(val + deg_len);
    double decimal = (double)degrees + (minutes / 60.0);
    if (hemi && (hemi[0] == 'S' || hemi[0] == 'W'))
        decimal = -decimal;
    return decimal;
}

static double knots_to_kmh(double knots)
{
    return knots * 1.852;
}

static void handle_gga(const char *type, char **f, int n)
{
    // GGA fields (index starting at 0 after talker+type):
    // 0: talker+type (e.g., GNGGA) 1:UTC,2:lat,3:N/S,4:lon,5:E/W,6:fix,7:sats,8:HDOP,9:alt,10:altUnit
    if (n < 11)
        return;
    int hh, mm, ss, ms;
    parse_hms(f[1], &hh, &mm, &ss, &ms);
    int fix = f[6] && f[6][0] ? atoi(f[6]) : 0;
    int sats = f[7] && f[7][0] ? atoi(f[7]) : 0;
    const char *hdop = (n > 8) ? f[8] : "";
    const char *alt = (n > 9) ? f[9] : "";
    double lat = parse_latlon(f[2], f[3], true);
    double lon = parse_latlon(f[4], f[5], false);
    // Update state
    g_state.utc_hour = hh;
    g_state.utc_minute = mm;
    g_state.utc_second = ss;
    g_state.utc_millisecond = ms;
    g_state.latitude_deg = lat;
    g_state.longitude_deg = lon;
    g_state.fix_quality = fix;
    g_state.satellites_in_use = sats;
    if (alt && alt[0]) {
        g_state.altitude_m = atof(alt);
    }
    // Simple connectivity heuristic
    g_state.connect_state = (fix > 0) ? 1 : 0;
    // Signal strength: derive from sats and HDOP (lower is better)
    double hdop_val = (hdop && hdop[0]) ? atof(hdop) : 99.0;
    int level = 0;
    if (fix == 0) {
        level = 0;
    } else if (sats >= 12 || (sats >= 8 && hdop_val <= 1.0)) {
        level = 5;
    } else if (sats >= 8 || (sats >= 6 && hdop_val <= 1.5)) {
        level = 4;
    } else if (sats >= 6 || (sats >= 4 && hdop_val <= 2.5)) {
        level = 3;
    } else if (sats >= 4 || hdop_val <= 4.0) {
        level = 2;
    } else if (sats >= 1) {
        level = 1;
    }
    g_state.signal_level_5 = level;
#if LC76G_ENABLE_NMEA_LOGS
    // Only show GGA details when we have a fix or when specifically debugging
    if (fix > 0 || g_state.satellites_in_use > 0) {
        PR_NOTICE("[%s] Fix:%d Sats:%d Pos:%.6f,%.6f Alt:%s", type ? type : "GGA", fix, sats, lat, lon, safe_str(alt));
    } else {
        PR_DEBUG("[%s] No fix (searching for satellites...)", type ? type : "GGA");
    }
#endif
}

static void handle_rmc(const char *type, char **f, int n)
{
    // RMC fields: 1:UTC 2:status(A/V) 3:lat 4:N/S 5:lon 6:E/W 7:speed(knots) 8:course 9:date(ddmmyy)
    if (n < 10)
        return;
    int hh, mm, ss, ms;
    parse_hms(f[1], &hh, &mm, &ss, &ms);
    char status_char = f[2] && f[2][0] ? f[2][0] : 'V';
    double lat = parse_latlon(f[3], f[4], true);
    double lon = parse_latlon(f[5], f[6], false);
    double spd_kn = f[7] && f[7][0] ? atof(f[7]) : 0.0;
    double spd_kmh = knots_to_kmh(spd_kn);
    const char *course = (n > 8) ? f[8] : "";
    const char *date = (n > 9) ? f[9] : "";
    // Update state
    g_state.utc_hour = hh;
    g_state.utc_minute = mm;
    g_state.utc_second = ss;
    g_state.utc_millisecond = ms;
    g_state.latitude_deg = lat;
    g_state.longitude_deg = lon;
    g_state.speed_kmh = spd_kmh;
    g_state.course_deg = (course && course[0]) ? atof(course) : 0.0;
    g_state.last_status = (status_char == 'A'); // true if valid fix, false otherwise
    if (date && strlen(date) >= 6) {
        // Copy first 6 chars ddmmyy
        memcpy(g_state.date_ddmmyy, date, 6);
        g_state.date_ddmmyy[6] = '\0';
    }
    g_state.connect_state = (status_char == 'A') ? 1 : g_state.connect_state;
#if LC76G_ENABLE_NMEA_LOGS
    // Only show RMC details when data is valid or when moving
    if (status_char == 'A' || spd_kmh > 0.5) {
        PR_NOTICE("[%s] Status:%c Time:%02d:%02d:%02d Speed:%.1f km/h Course:%.1f°", type ? type : "RMC", status_char,
                  hh, mm, ss, spd_kmh, g_state.course_deg);
    } else {
        PR_DEBUG("[%s] Status:%c (waiting for valid fix)", type ? type : "RMC", status_char);
    }
#endif
}

static void handle_vtg(const char *type, char **f, int n)
{
    // VTG fields: 1:courseTrue 2:'T' 3:courseMag 4:'M' 5:speedKnots 6:'N' 7:speedKmh 8:'K'
    const char *course_t = (n > 1) ? f[1] : "";
    double spd_kn = (n > 5 && f[5] && f[5][0]) ? atof(f[5]) : 0.0;
    double spd_kmh = (n > 7 && f[7] && f[7][0]) ? atof(f[7]) : knots_to_kmh(spd_kn);
    // Update state
    g_state.speed_kmh = spd_kmh;
    g_state.course_deg = (course_t && course_t[0]) ? atof(course_t) : g_state.course_deg;
#if LC76G_ENABLE_NMEA_LOGS
    // Only show VTG when moving
    if (spd_kmh > 0.5) {
        PR_DEBUG("[%s] Speed:%.1f km/h Course:%.1f°", type ? type : "VTG", spd_kmh, g_state.course_deg);
    }
#endif
}

static void handle_gsa(const char *type, char **f, int n)
{
    // GSA: mode1(A/M), mode2(1=no fix,2=2D,3=3D), sat IDs (12 fields), PDOP, HDOP, VDOP
    if (n < 3)
        return;
    int mode2 = (n > 2 && f[2] && f[2][0]) ? atoi(f[2]) : 0;
    const char *hdop = (n > 16) ? f[16] : "";
    // Update connectivity/quality hints
    if (mode2 >= 2) {
        g_state.connect_state = 1;
    }
    // Re-evaluate signal level if we have hdop
    if (hdop && hdop[0]) {
        double hdop_val = atof(hdop);
        int sats = g_state.satellites_in_use;
        int level = g_state.signal_level_5;
        if (g_state.fix_quality == 0) {
            level = 0;
        } else if (sats >= 12 || (sats >= 8 && hdop_val <= 1.0)) {
            level = 5;
        } else if (sats >= 8 || (sats >= 6 && hdop_val <= 1.5)) {
            level = 4;
        } else if (sats >= 6 || (sats >= 4 && hdop_val <= 2.5)) {
            level = 3;
        } else if (sats >= 4 || hdop_val <= 4.0) {
            level = 2;
        } else if (sats >= 1) {
            level = 1;
        }
        g_state.signal_level_5 = level;

#if LC76G_ENABLE_NMEA_LOGS
        // Only show GSA when we have dilution data
        if (mode2 >= 2) {
            PR_DEBUG("[%s] Mode:%d HDOP:%s Signal level:%d", type ? type : "GSA", mode2, safe_str(hdop), level);
        }
#endif
    }
}

static void handle_gsv(const char *type, char **f, int n)
{
    // GSV: totalMsgs,msgNum,svInView, then 4-sat blocks (satPRN, elev, azim, SNR)
    if (n < 4)
        return;
#if LC76G_ENABLE_NMEA_LOGS
    int total = atoi(f[1]);
    int num = atoi(f[2]);
    int in_view = atoi(f[3]);
#endif
#if LC76G_ENABLE_NMEA_LOGS
    // Only show GSV when satellites are visible
    if (in_view > 0) {
        PR_DEBUG("[%s] Message %d/%d, Satellites in view: %d", type ? type : "GSV", num, total, in_view);
    }
#endif
}

static void parse_and_print_nmea(char *buffer, uint32_t len)
{
    int sentences_parsed = 0;
    int checksum_failures = 0;
    bool has_fix_data = false;

    // Iterate lines split by \n or \r\n
    char *p = buffer;
    char *end = buffer + len;
    while (p < end) {
        // Find start '$'
        char *dollar = memchr(p, '$', (size_t)(end - p));
        if (!dollar)
            break;
        // Find line end
        char *line_end = memchr(dollar, '\n', (size_t)(end - dollar));
        if (!line_end)
            line_end = end; // possibly last line without EOL

        // Create a working copy of the line
        size_t line_len = (size_t)(line_end - dollar);
        if (line_len < 6) {
            p = line_end + (line_end < end ? 1 : 0);
            continue;
        }
        char *line = (char *)tal_malloc(line_len + 1);
        if (!line) {
            PR_ERR("Memory allocation failed for NMEA line");
            return;
        }
        memcpy(line, dollar, line_len);
        line[line_len] = '\0';

        // Trim trailing \r
        size_t L = line_len;
        if (L > 0 && line[L - 1] == '\r')
            line[L - 1] = '\0';

        if (nmea_validate(line)) {
            sentences_parsed++;
            // Split fields
            char *fields[32] = {0};
            int n = split_fields(line, fields, 32);
            if (n > 0) {
                const char *type = fields[0];
                if (strstr(type, "GGA")) {
                    handle_gga(type, fields, n);
                    if (g_state.fix_quality > 0)
                        has_fix_data = true;
                } else if (strstr(type, "RMC")) {
                    handle_rmc(type, fields, n);
                    if (g_state.last_status)
                        has_fix_data = true;
                } else if (strstr(type, "VTG")) {
                    handle_vtg(type, fields, n);
                } else if (strstr(type, "GSA")) {
                    handle_gsa(type, fields, n);
                } else if (strstr(type, "GSV")) {
                    handle_gsv(type, fields, n);
                } else {
#if LC76G_ENABLE_NMEA_LOGS
                    // For other sentences, print a compact line once validated
                    PR_DEBUG("NMEA %s", type);
#endif
                }
            }
        } else {
            checksum_failures++;
#if LC76G_ENABLE_NMEA_LOGS
            if (checksum_failures <= 3) { // Only show first few failures to avoid spam
                PR_DEBUG("NMEA checksum fail: %s", line);
            }
#endif
        }

        tal_free(line);
        p = line_end + (line_end < end ? 1 : 0);
    }

    // Print a concise summary, but reduce frequency to avoid log spam
    g_log_counter++;

    // Print summary every 5th read (every 25 seconds with 5s intervals) or when we have a fix
    if ((g_log_counter % 5 == 0) || has_fix_data || g_state.satellites_in_use > 0) {
        PR_INFO("NMEA: %d sentences, %d fails, fix=%s, sats=%d, status=%s, signal=%d/5", sentences_parsed,
                checksum_failures, has_fix_data ? "YES" : "NO", g_state.satellites_in_use,
                g_state.last_status ? "VALID" : "INVALID", g_state.signal_level_5);
    } else {
        PR_DEBUG("NMEA: %d sentences processed (searching satellites...)", sentences_parsed);
    }
}

OPERATE_RET lc76g_pair_062(lc76g_dev_t *dev, uint8_t type, uint8_t output_rate)
{
    // Type of standard NMEA sentence. -1 = Reset the output rates of all types of sentences to default values.
    // 0 = NMEA_SEN_GGA
    // 1 = NMEA_SEN_GLL
    // 2 = NMEA_SEN_GSA
    // 3 = NMEA_SEN_GSV
    // 4 = NMEA_SEN_RMC
    // 5 = NMEA_SEN_VTG
    // 8 = NMEA_SEN_GST

    // Output rate setting.
    // 0 = Disabled or not supported
    // N = Output once every N position fix(es)
    // Range of N: 0–20. Default value: 1.

    // $PAIR062,<type>,<output_rate>*hh<CR><LF>
    OPERATE_RET rt = OPRT_OK;

    uint8_t send_data[40] = {0};
    int len = snprintf((char *)send_data, sizeof(send_data), "$PAIR062,%d,%d*", type, output_rate);

    // Calculate checksum
    uint8_t checksum = nmea_checksum_calculate((const char *)send_data);
    len += snprintf((char *)send_data + len, sizeof(send_data) - (size_t)len, "%02X\r\n", checksum);

    PR_DEBUG("PAIR062 command len: %d, data: %.*s", len, (int)len, send_data);

    dev_uart_write(dev->config.uart.port, send_data, len);

    // Read response (expecting $PAIR062,OK*hh or $PAIR062,ERR*hh)
    uint8_t recv_buffer[32] = {0};
    int read_len = dev_uart_read(dev->config.uart.port, recv_buffer, sizeof(recv_buffer), 3000);
    if (read_len == 0) {
        PR_ERR("No response to PAIR062 command");
        return OPRT_COM_ERROR;
    }
    PR_DEBUG("PAIR062 response len: %d, data: %.*s", read_len, read_len, recv_buffer);

    // Simple validation
    if (nmea_validate((const char *)recv_buffer) == false) {
        PR_ERR("Invalid NMEA response to PAIR062");
        return OPRT_COM_ERROR;
    }

    // Parse response
    // $PAIR001,<CommandID>,<Result>*<Checksum><CR><LF>
    // $PAIR001,062,0*3F

    int command_id = -1;
    int result = -1;

    char *fields[8] = {0};
    int n = split_fields((char *)recv_buffer, fields, 8);
    if (n >= 3) {
        command_id = atoi(fields[1]);
        result = atoi(fields[2]);
    }

    if (command_id != 62) {
        PR_ERR("Unexpected command ID in PAIR062 response: %d", command_id);
        return OPRT_COM_ERROR;
    }

    if (result != 0) {
        PR_ERR("PAIR062 command failed with result code: %d", result);
        return OPRT_COM_ERROR;
    }

    return rt;
}

OPERATE_RET lc76g_init_i2c(lc76g_dev_t *dev, uint8_t i2c_addr_wr, uint8_t i2c_addr_r)
{
    if (!dev)
        return OPRT_COM_ERROR;

    dev->interface = LC76G_INTERFACE_I2C;
    dev->config.i2c.addr_wr = i2c_addr_wr;
    dev->config.i2c.addr_r = i2c_addr_r;

    // 拉高复位引脚
    dev_gpio_init(EXAMPLE_GPS_RESET_PIN, TUYA_GPIO_OUTPUT);
    dev_digital_write(EXAMPLE_GPS_RESET_PIN, 0);
    tal_system_sleep(50);
    dev_digital_write(EXAMPLE_GPS_RESET_PIN, 1);
    tal_system_sleep(500);
    PR_INFO("LC76G initialized successfully with I2C interface");
    return OPRT_OK;
}

// UART interface configuration (following Waveshare demo)
#define UART_BUFFER_SIZE     4096 // Match demo code buffer size
#define UART_READ_TIMEOUT_MS 1000 // Total timeout for reading

OPERATE_RET lc76g_init_uart(lc76g_dev_t *dev, TUYA_UART_NUM_E port, uint32_t baudrate)
{
    if (!dev)
        return OPRT_COM_ERROR;

    dev->interface = LC76G_INTERFACE_UART;
    dev->config.uart.port = port;
    dev->config.uart.baudrate = baudrate;

    // LC76G Hardware Reset Sequence (per Waveshare documentation)
    // GPS_RST is active LOW: 0=RESET, 1=NORMAL
    PR_NOTICE("========================================");
    PR_NOTICE("LC76G GPS Module Initialization");
    PR_NOTICE("========================================");
    PR_NOTICE("Resetting GPS module on pin %d...", EXAMPLE_GPS_RESET_PIN);

    dev_gpio_init(EXAMPLE_GPS_RESET_PIN, TUYA_GPIO_OUTPUT);

    // Assert reset (active low)
    dev_digital_write(EXAMPLE_GPS_RESET_PIN, 0);
    PR_NOTICE("GPS in RESET state (pin LOW)");
    tal_system_sleep(200); // Hold reset for 200ms

    // Release reset - GPS module starts
    dev_digital_write(EXAMPLE_GPS_RESET_PIN, 1);
    PR_NOTICE("GPS RESET released (pin HIGH)");
    PR_NOTICE("GPS module starting...");
    PR_NOTICE("Waiting for GPS to boot and start transmitting NMEA...");
    tal_system_sleep(2000); // Wait 2s for GPS to fully boot and start transmitting

    // LC76G default baudrate is 115200 (per PAIR864 documentation)
    // It automatically outputs NMEA - no commands needed!
    PR_NOTICE("Using GPS UART interface");
    dev_uart_init(port, baudrate);

    PR_NOTICE("LC76G Configuration:");
    PR_NOTICE("  Interface: UART (continuous read mode)");
    PR_NOTICE("  Port: UART%d", port);
    PR_NOTICE("  Baudrate: %d", baudrate);
    PR_NOTICE("  TX Pin: P41 (MCU transmits to GPS RX)");
    PR_NOTICE("  RX Pin: P40 (MCU receives from GPS TX)");
    PR_NOTICE("  Buffer: %d bytes", UART_BUFFER_SIZE);
    PR_NOTICE("========================================");
    PR_NOTICE("NOTE: LC76G streams NMEA continuously");
    PR_NOTICE("      Cold start takes ~26 seconds for first fix");
    PR_NOTICE("========================================");

    // only enable RMC
    // RMC update once every 3s
    lc76g_pair_062(dev, 0, 0);
    lc76g_pair_062(dev, 1, 0);
    lc76g_pair_062(dev, 2, 0);
    lc76g_pair_062(dev, 3, 0);
    lc76g_pair_062(dev, 4, 5);
    lc76g_pair_062(dev, 5, 0);

    return OPRT_OK;
}

// Backward compatibility wrapper
OPERATE_RET lc76g_init(lc76g_dev_t *dev, uint8_t i2c_addr_wr, uint8_t i2c_addr_r)
{
    return lc76g_init_i2c(dev, i2c_addr_wr, i2c_addr_r);
}

uint8_t data[] = {0x08, 0x00, 0x51, 0xAA, 0x04, 0x00, 0x00, 0x00};

static OPERATE_RET lc76g_get_data_i2c(lc76g_dev_t *dev)
{
    OPERATE_RET ret;

    ret = dev_i2c_write_nbytes(dev->config.i2c.addr_wr, data, sizeof(data));
    if (ret != OPRT_OK) {
        PR_ERR("Failed to write data from device");
        return ret;
    }
    tal_system_sleep(100);

    ret = dev_i2c_read_only_nbytes(dev->config.i2c.addr_r, readData, sizeof(readData));
    if (ret != OPRT_OK) {
        PR_ERR("Failed to read data from device");
        return ret;
    }

    uint32_t dataLength = (readData[0]) | (readData[1] << 8) | (readData[2] << 16) | (readData[3] << 24);

    if (dataLength == 0) {
        PR_ERR("Invalid data length");
        return OPRT_COM_ERROR;
    }

    uint8_t data2[] = {0x00, 0x20, 0x51, 0xAA};
    uint8_t dataToSend[sizeof(data2) + sizeof(readData)];
    memcpy(dataToSend, data2, sizeof(data2));
    memcpy(dataToSend + sizeof(data2), readData, sizeof(readData));
    tal_system_sleep(100);

    ret = dev_i2c_write_nbytes(dev->config.i2c.addr_wr, dataToSend, sizeof(dataToSend));
    if (ret != OPRT_OK) {
        PR_ERR("Failed to write concatenated data");
        return ret;
    }

    uint8_t *dynamicReadData = (uint8_t *)tal_malloc(dataLength + 1);
    if (!dynamicReadData) {
        PR_ERR("Memory allocation failed");
        return OPRT_COM_ERROR;
    }
    tal_system_sleep(10 + dataLength / 100);

    ret = dev_i2c_read_only_nbytes(dev->config.i2c.addr_r, dynamicReadData, dataLength);
    if (ret != OPRT_OK) {
        PR_ERR("Failed to read dynamic data");
        tal_free(dynamicReadData);
        return ret;
    }
    dynamicReadData[dataLength] = '\0';
    // Parse NMEA sentences and print human readable summaries
    parse_and_print_nmea((char *)dynamicReadData, dataLength);

    tal_free(dynamicReadData);
    return ret;
}

static OPERATE_RET lc76g_get_data_uart(lc76g_dev_t *dev)
{
    // Allocate buffer (like demo code: 1600 bytes)
    uint8_t *buffer = (uint8_t *)tal_malloc(UART_BUFFER_SIZE);
    if (!buffer) {
        PR_ERR("Memory allocation failed for UART buffer");
        return OPRT_COM_ERROR;
    }
    memset(buffer, 0, UART_BUFFER_SIZE);

#if LC76G_ENABLE_NMEA_LOGS
    // Simplified read: try to read a larger chunk at once
    PR_NOTICE("Attempting to read GPS data from UART...");
#endif

    // Try a single bulk read first
    int total_bytes = dev_uart_read(dev->config.uart.port, buffer, UART_BUFFER_SIZE - 1, UART_READ_TIMEOUT_MS);

#if LC76G_ENABLE_NMEA_LOGS
    PR_NOTICE("Initial read returned: %d bytes", total_bytes);
#endif

    if (total_bytes < 0) {
        PR_ERR("UART read error: %d", total_bytes);
        tal_free(buffer);
        return OPRT_COM_ERROR;
    }

    if (total_bytes == 0) {
        PR_NOTICE("No data on first read, waiting 500ms and retrying...");
        tal_system_sleep(500);

        total_bytes = dev_uart_read(dev->config.uart.port, buffer, UART_BUFFER_SIZE - 1, UART_READ_TIMEOUT_MS);
        PR_NOTICE("Second read returned: %d bytes", total_bytes);

        if (total_bytes <= 0) {
            PR_WARN("Still no GPS data after retry");
            tal_free(buffer);
            return OPRT_OK;
        }
    }

    buffer[total_bytes] = '\0';

// Only print full raw data if explicitly enabled for debugging
#if LC76G_ENABLE_NMEA_LOGS // Set to 1 to enable full NMEA raw dump
    PR_NOTICE("GPS: Received %d bytes from UART", total_bytes);
    // Print raw NMEA data summary (only show size to reduce log spam)
    PR_DEBUG("LC76G: Received %d bytes of NMEA data", total_bytes);

    PR_DEBUG("========================================");
    PR_DEBUG("LC76G NMEA RAW DATA:");
    PR_DEBUG("%s", buffer);
    PR_DEBUG("========================================");
#endif

    // Parse NMEA sentences
    parse_and_print_nmea((char *)buffer, total_bytes);

    tal_free(buffer);
    return OPRT_OK;
}

OPERATE_RET lc76g_get_data(lc76g_dev_t *dev)
{
    if (!dev)
        return OPRT_COM_ERROR;

    if (dev->interface == LC76G_INTERFACE_I2C) {
        return lc76g_get_data_i2c(dev);
    } else if (dev->interface == LC76G_INTERFACE_UART) {
        return lc76g_get_data_uart(dev);
    }

    PR_ERR("Unknown interface type");
    return OPRT_COM_ERROR;
}

/* Getter implementations */
const lc76g_state_t *lc76g_get_state(void)
{
    return &g_state;
}

void lc76g_get_utc(int *hh, int *mm, int *ss, int *ms)
{
    if (hh)
        *hh = g_state.utc_hour;
    if (mm)
        *mm = g_state.utc_minute;
    if (ss)
        *ss = g_state.utc_second;
    if (ms)
        *ms = g_state.utc_millisecond;
}

void lc76g_get_position(double *lat_deg, double *lon_deg, double *alt_m)
{
    if (lat_deg)
        *lat_deg = g_state.latitude_deg;
    if (lon_deg)
        *lon_deg = g_state.longitude_deg;
    if (alt_m)
        *alt_m = g_state.altitude_m;
}

void lc76g_get_data_ddmmyy(char out[7])
{
    if (out) {
        memcpy(out, g_state.date_ddmmyy, 6);
        out[6] = '\0';
    }
}

int lc76g_get_sat_count(void)
{
    return g_state.satellites_in_use;
}

int lc76g_get_fix_quality(void)
{
    return g_state.fix_quality;
}

int lc76g_get_connect_state(void)
{
    return g_state.connect_state;
}

int lc76g_get_signal_level5(void)
{
    return g_state.signal_level_5;
}

double lc76g_get_speed_kmh(void)
{
    return g_state.speed_kmh;
}

double lc76g_get_course_deg(void)
{
    return g_state.course_deg;
}

bool lc76g_get_fix_status(void)
{
    return g_state.last_status;
}
