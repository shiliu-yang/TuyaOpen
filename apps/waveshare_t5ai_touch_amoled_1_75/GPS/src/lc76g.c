#include "lc76g.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

uint8_t readData[4];

#ifndef LC76G_ENABLE_NMEA_LOGS
#define LC76G_ENABLE_NMEA_LOGS 0
#endif

static lc76g_state_t g_state = {
    .utc_hour = 0,
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
    .last_status = '-'
};

// ----------------------
// NMEA parsing utilities
// ----------------------

static bool nmea_validate(const char *line)
{
    // Expect format: $...*HH[\r][\n]
    const char *start = line;
    if (!start || start[0] != '$') return false;
    const char *asterisk = strchr(start, '*');
    if (!asterisk || (asterisk - start) < 1) return false;

    // Read hex checksum after '*'
    if (!asterisk[1] || !asterisk[2]) return false;
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
    if (*p == '$') p++;
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
    if (!utc || utc[0] == '\0') return;
    int len = (int)strlen(utc);
    if (len < 6) return;
    char buf[4] = {0};
    memcpy(buf, utc, 2); *hh = atoi(buf);
    memcpy(buf, utc + 2, 2); *mm = atoi(buf);
    memcpy(buf, utc + 4, 2); *ss = atoi(buf);
    const char *dot = strchr(utc, '.');
    if (dot && dot[1]) {
        char msbuf[4] = {0};
        // take up to 3 digits
        int i = 0; dot++;
        while (i < 3 && *dot && *dot != ',') { msbuf[i++] = *dot++; }
        *ms = atoi(msbuf);
    }
}

static double parse_latlon(const char *val, const char *hemi, bool is_lat)
{
    // val like ddmm.mmmm (lat) or dddmm.mmmm (lon)
    if (!val || val[0] == '\0') return 0.0;
    int deg_len = is_lat ? 2 : 3;
    if ((int)strlen(val) < deg_len + 2) return 0.0;
    char degbuf[4] = {0};
    memcpy(degbuf, val, deg_len);
    int degrees = atoi(degbuf);
    double minutes = atof(val + deg_len);
    double decimal = (double)degrees + (minutes / 60.0);
    if (hemi && (hemi[0] == 'S' || hemi[0] == 'W')) decimal = -decimal;
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
    if (n < 11) return;
    int hh, mm, ss, ms; parse_hms(f[1], &hh, &mm, &ss, &ms);
    int fix = f[6] && f[6][0] ? atoi(f[6]) : 0;
    int sats = f[7] && f[7][0] ? atoi(f[7]) : 0;
    const char *hdop = (n > 8) ? f[8] : "";
    const char *alt = (n > 9) ? f[9] : "";
#if LC76G_ENABLE_NMEA_LOGS
    const char *alt_u = (n > 10) ? f[10] : "";
#endif
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
    PR_NOTICE("[%s]", type ? type : "GGA");
    PR_NOTICE("  Time (UTC): %02d:%02d:%02d.%03d", hh, mm, ss, ms);
    PR_NOTICE("  Fix quality: %d (0=invalid,1=GPS,2=DGPS)", fix);
    PR_NOTICE("  Satellites: %d", sats);
    PR_NOTICE("  HDOP: %s", safe_str(hdop));
    PR_NOTICE("  Latitude: %.6f (raw %s %s)", lat, safe_str(f[2]), safe_str(f[3]));
    PR_NOTICE("  Longitude: %.6f (raw %s %s)", lon, safe_str(f[4]), safe_str(f[5]));
    PR_NOTICE("  Altitude: %s %s", safe_str(alt), safe_str(alt_u));
#endif
}

static void handle_rmc(const char *type, char **f, int n)
{
    // RMC fields: 1:UTC 2:status(A/V) 3:lat 4:N/S 5:lon 6:E/W 7:speed(knots) 8:course 9:date(ddmmyy)
    if (n < 10) return;
    int hh, mm, ss, ms; parse_hms(f[1], &hh, &mm, &ss, &ms);
    char status = f[2] && f[2][0] ? f[2][0] : 'V';
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
    g_state.last_status = status;
    if (date && strlen(date) >= 6) {
        // Copy first 6 chars ddmmyy
        memcpy(g_state.date_ddmmyy, date, 6);
        g_state.date_ddmmyy[6] = '\0';
    }
    g_state.connect_state = (status == 'A') ? 1 : g_state.connect_state;
#if LC76G_ENABLE_NMEA_LOGS
    PR_NOTICE("[%s]", type ? type : "RMC");
    PR_NOTICE("  Time (UTC): %02d:%02d:%02d.%03d", hh, mm, ss, ms);
    PR_NOTICE("  Status: %c (A=valid, V=void)", status);
    PR_NOTICE("  Latitude: %.6f (raw %s %s)", lat, safe_str(f[3]), safe_str(f[4]));
    PR_NOTICE("  Longitude: %.6f (raw %s %s)", lon, safe_str(f[5]), safe_str(f[6]));
    PR_NOTICE("  Speed: %.2f km/h (%.2f kn, raw kn=%s)", spd_kmh, spd_kn, safe_str(f[7]));
    PR_NOTICE("  Course over ground: %s", safe_str(course));
    PR_NOTICE("  Date (ddmmyy): %s", safe_str(date));
#endif
}

static void handle_vtg(const char *type, char **f, int n)
{
    // VTG fields: 1:courseTrue 2:'T' 3:courseMag 4:'M' 5:speedKnots 6:'N' 7:speedKmh 8:'K'
    const char *course_t = (n > 1) ? f[1] : "";
    double spd_kn = (n > 5 && f[5] && f[5][0]) ? atof(f[5]) : 0.0;
    double spd_kmh = (n > 7 && f[7] && f[7][0]) ? atof(f[7]) : knots_to_kmh(spd_kn);
#if LC76G_ENABLE_NMEA_LOGS
    const char *course_m = (n > 3) ? f[3] : "";
#endif
    // Update state
    g_state.speed_kmh = spd_kmh;
    g_state.course_deg = (course_t && course_t[0]) ? atof(course_t) : g_state.course_deg;
#if LC76G_ENABLE_NMEA_LOGS
    PR_NOTICE("[%s]", type ? type : "VTG");
    PR_NOTICE("  Course (true): %s deg", safe_str(course_t));
    PR_NOTICE("  Course (magnetic): %s deg", safe_str(course_m));
    PR_NOTICE("  Speed: %.2f km/h (%.2f kn, raw kn=%s, raw kmh=%s)", spd_kmh, spd_kn, (n>5?safe_str(f[5]):"-"), (n>7?safe_str(f[7]):"-"));
#endif
}

static void handle_gsa(const char *type, char **f, int n)
{
    // GSA: mode1(A/M), mode2(1=no fix,2=2D,3=3D), sat IDs (12 fields), PDOP, HDOP, VDOP
    if (n < 3) return;
    int mode2 = (n > 2 && f[2] && f[2][0]) ? atoi(f[2]) : 0;
#if LC76G_ENABLE_NMEA_LOGS
    const char mode1 = (n > 1 && f[1] && f[1][0]) ? f[1][0] : '-';
    const char *pdop = (n > 15) ? f[15] : "";
    const char *vdop = (n > 17) ? f[17] : "";
#endif
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
    }
#if LC76G_ENABLE_NMEA_LOGS
    PR_NOTICE("[%s]", type ? type : "GSA");
    PR_NOTICE("  Mode: %c, Dimensional fix: %d (1=no,2=2D,3=3D)", mode1, mode2);
    PR_NOTICE("  PDOP: %s  HDOP: %s  VDOP: %s", safe_str(pdop), safe_str(hdop), safe_str(vdop));
#endif
}

static void handle_gsv(const char *type, char **f, int n)
{
    // GSV: totalMsgs,msgNum,svInView, then 4-sat blocks (satPRN, elev, azim, SNR)
    if (n < 4) return;
#if LC76G_ENABLE_NMEA_LOGS
    int total = atoi(f[1]);
    int num = atoi(f[2]);
    int in_view = atoi(f[3]);
#endif
#if LC76G_ENABLE_NMEA_LOGS
    PR_NOTICE("[%s]", type ? type : "GSV");
    PR_NOTICE("  Message %d/%d, Satellites in view: %d", num, total, in_view);
    // Print up to the first 4 satellites in this sentence
    for (int i = 4, idx = 0; i + 3 < n && idx < 4; i += 4, ++idx) {
        PR_NOTICE("  Sat%u: PRN=%s elev=%s azim=%s SNR=%s", (unsigned)(idx + 1), safe_str(f[i]), safe_str(f[i+1]), safe_str(f[i+2]), safe_str(f[i+3]));
    }
#endif
}

static void parse_and_print_nmea(char *buffer, uint32_t len)
{
    // Iterate lines split by \n or \r\n
    char *p = buffer;
    char *end = buffer + len;
    while (p < end) {
        // Find start '$'
        char *dollar = memchr(p, '$', (size_t)(end - p));
        if (!dollar) break;
        // Find line end
        char *line_end = memchr(dollar, '\n', (size_t)(end - dollar));
        if (!line_end) line_end = end; // possibly last line without EOL

        // Create a working copy of the line
        size_t line_len = (size_t)(line_end - dollar);
        if (line_len < 6) { p = line_end + (line_end < end ? 1 : 0); continue; }
        char *line = (char *)malloc(line_len + 1);
        if (!line) return;
        memcpy(line, dollar, line_len);
        line[line_len] = '\0';

        // Trim trailing \r
        size_t L = line_len;
        if (L > 0 && line[L - 1] == '\r') line[L - 1] = '\0';

        if (nmea_validate(line)) {
            // Split fields
            char *fields[32] = {0};
            int n = split_fields(line, fields, 32);
            if (n > 0) {
                const char *type = fields[0];
                if (strstr(type, "GGA")) {
                    handle_gga(type, fields, n);
                } else if (strstr(type, "RMC")) {
                    handle_rmc(type, fields, n);
                } else if (strstr(type, "VTG")) {
                    handle_vtg(type, fields, n);
                } else if (strstr(type, "GSA")) {
                    handle_gsa(type, fields, n);
                } else if (strstr(type, "GSV")) {
                    handle_gsv(type, fields, n);
                } else {
#if LC76G_ENABLE_NMEA_LOGS
                    // For other sentences, print a compact line once validated
                    PR_INFO("NMEA %s", type);
#endif
                }
            }
        } else {
#if LC76G_ENABLE_NMEA_LOGS
            PR_DEBUG("NMEA checksum fail: %s", line);
#endif
        }

        free(line);
        p = line_end + (line_end < end ? 1 : 0);
    }
}

OPERATE_RET lc76g_init(lc76g_dev_t *dev, uint8_t i2c_addr_wr, uint8_t i2c_addr_r) {
    if (!dev) return OPRT_COM_ERROR;
    
    dev->i2c_addr_wr = i2c_addr_wr;
    dev->i2c_addr_r = i2c_addr_r;

    //拉高复位引脚
    dev_gpio_init(EXAMPLE_GPS_RESET_PIN, TUYA_GPIO_OUTPUT);
    dev_digital_write(EXAMPLE_GPS_RESET_PIN, 0);
    tal_system_sleep(50);
    dev_digital_write(EXAMPLE_GPS_RESET_PIN, 1);
    tal_system_sleep(500);
    PR_INFO("LC76G initialized successfully");
    return OPRT_OK;
}

uint8_t data[] = { 0x08, 0x00, 0x51, 0xAA, 0x04, 0x00, 0x00, 0x00 };

OPERATE_RET lc76g_get_date(lc76g_dev_t *dev)
{
    if (!dev ) return OPRT_COM_ERROR;
    OPERATE_RET ret;

    ret = dev_i2c_write_nbytes(dev->i2c_addr_wr, data, sizeof(data));
    if (ret != OPRT_OK)
    {
        PR_ERR("Failed to write data from device");
        return ret;
    }
    tal_system_sleep(100);
    
    ret = dev_i2c_read_only_nbytes(dev->i2c_addr_r, readData, sizeof(readData));
    if (ret != OPRT_OK)
    {
        PR_ERR("Failed to read data from device");
        return ret;
    }

    uint32_t dataLength = (readData[0]) | (readData[1] << 8) | (readData[2] << 16) | (readData[3] << 24);

    if (dataLength == 0) {
        PR_ERR("Invalid data length");
        return OPRT_COM_ERROR;
    }

    uint8_t data2[] = { 0x00, 0x20, 0x51, 0xAA };
    uint8_t dataToSend[sizeof(data2) + sizeof(readData)];
    memcpy(dataToSend, data2, sizeof(data2));
    memcpy(dataToSend + sizeof(data2), readData, sizeof(readData));
    tal_system_sleep(100);

    ret = dev_i2c_write_nbytes(dev->i2c_addr_wr, dataToSend, sizeof(dataToSend));
    if (ret != OPRT_OK)
    {
        PR_ERR("Failed to write concatenated data");
        return ret;
    }

    // PR_INFO("I2C read data (%d bytes): ", dataLength);

    uint8_t *dynamicReadData = (uint8_t *)malloc(dataLength + 1);
    if (!dynamicReadData) {
        PR_ERR("Memory allocation failed");
        return OPRT_COM_ERROR;
    }
    // memset(dynamicReadData, 0, dataLength);
    tal_system_sleep(10 + dataLength / 100);

    ret = dev_i2c_read_only_nbytes(dev->i2c_addr_r, dynamicReadData, dataLength);
    if (ret != OPRT_OK)
    {
        PR_ERR("Failed to read dynamic data");
        free(dynamicReadData);
        return ret;
    }
    dynamicReadData[dataLength] = '\0';
    // Parse NMEA sentences and print human readable summaries
    parse_and_print_nmea((char *)dynamicReadData, dataLength);

    free(dynamicReadData);
	return ret;
}

/* Getter implementations */
const lc76g_state_t *lc76g_get_state(void)
{
    return &g_state;
}

void lc76g_get_utc(int *hh, int *mm, int *ss, int *ms)
{
    if (hh) *hh = g_state.utc_hour;
    if (mm) *mm = g_state.utc_minute;
    if (ss) *ss = g_state.utc_second;
    if (ms) *ms = g_state.utc_millisecond;
}

void lc76g_get_position(double *lat_deg, double *lon_deg, double *alt_m)
{
    if (lat_deg) *lat_deg = g_state.latitude_deg;
    if (lon_deg) *lon_deg = g_state.longitude_deg;
    if (alt_m) *alt_m = g_state.altitude_m;
}

void lc76g_get_date_ddmmyy(char out[7])
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

char lc76g_get_status_char(void)
{
    return g_state.last_status;
}
