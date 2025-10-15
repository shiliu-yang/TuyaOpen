/**
 * @file cloud_api.c
 * @brief Cloud API implementation for cattle location tracking
 *
 * This file implements cloud API functionality with conditional compilation support.
 * Set ENABLE_CLOUD_API=1 to enable full cloud functionality, or ENABLE_CLOUD_API=0
 * to use stub implementations for testing without cloud dependencies.
 *
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "cloud_api.h"

#include "tal_api.h"
#include "tuya_iot.h"

#include "app_dp.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define CLOUD_API_MALLOC tal_psram_malloc
#define CLOUD_API_FREE   tal_psram_free

// #define CATTLE_LOCATION_QUERY_API "m.outdoors.cattle.location.query"
#define CATTLE_LOCATION_QUERY_API "thing.cattle.location.query"
#define CATTLE_LOCATION_QUERY_VER "1.0"

#define DEFAULT_REQUEST_INTERVAL_MS (30 * 1000)     // default minimum interval between requests in milliseconds
#define MAX_ERROR_COUNT             5               // maximum consecutive error count before using max interval
#define MAX_REQUEST_INTERVAL_MS     (5 * 60 * 1000) // maximum interval of 5 minutes

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    MUTEX_HANDLE mutex;
    SEM_HANDLE sem;
    OPERATE_RET api_rt;
    uint8_t is_getting;
    uint8_t error_count;
    SYS_TIME_T last_query_time;
    SYS_TIME_T request_interval_ms;
} cloud_api_ctx_t;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static cloud_api_ctx_t sg_cloud_api_ctx = {
    .mutex = NULL,
    .sem = NULL,
    .api_rt = OPRT_OK,
    .is_getting = 0,
    .error_count = 0,
    .last_query_time = 0,
    .request_interval_ms = DEFAULT_REQUEST_INTERVAL_MS, // minimum interval between requests in milliseconds
};

static SYS_TIME_T sg_request_interval_ms[] = {
    DEFAULT_REQUEST_INTERVAL_MS, // 30 seconds  - error_count 0
    50 * 1000,                   // 50 seconds  - error_count 1
    60 * 1000,                   // 1 minute    - error_count 2
    3 * 60 * 1000,               // 3 minutes   - error_count 3
    5 * 60 * 1000,               // 5 minutes   - error_count 4+
};

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Get current request interval based on error count
 * @return Current request interval in milliseconds
 */
static SYS_TIME_T __get_current_request_interval(void)
{
    uint8_t error_count = sg_cloud_api_ctx.error_count;
    uint8_t max_index = sizeof(sg_request_interval_ms) / sizeof(sg_request_interval_ms[0]) - 1;

    if (error_count > max_index) {
        PR_WARN("error_count %d exceeds max_index %d, clamping to max", error_count, max_index);
        error_count = max_index;
        sg_cloud_api_ctx.error_count = max_index;
    }

    SYS_TIME_T interval = sg_request_interval_ms[error_count];
    PR_DEBUG("get_interval: error_count=%d, max_index=%d, interval=%u ms", 
             error_count, max_index, (uint32_t)interval);
    
    return interval;
}

/**
 * @brief Reset error count and request interval on success
 */
static void __reset_error_state(void)
{
    sg_cloud_api_ctx.error_count = 0;
    sg_cloud_api_ctx.request_interval_ms = DEFAULT_REQUEST_INTERVAL_MS;
    PR_DEBUG("Reset error state, interval back to %u ms", DEFAULT_REQUEST_INTERVAL_MS);
}

/**
 * @brief Increment error count and adjust request interval
 */
static void __increment_error_count(void)
{
    if (sg_cloud_api_ctx.error_count < MAX_ERROR_COUNT) {
        sg_cloud_api_ctx.error_count++;
    }

    sg_cloud_api_ctx.request_interval_ms = __get_current_request_interval();

    PR_WARN("Request failed, error_count: %d, next interval: %u ms", sg_cloud_api_ctx.error_count,
            (uint32_t)sg_cloud_api_ctx.request_interval_ms);
}

OPERATE_RET cloud_api_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (sg_cloud_api_ctx.mutex == NULL) {
        TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&sg_cloud_api_ctx.mutex), __EXIT);
    }

    if (sg_cloud_api_ctx.sem == NULL) {
        TUYA_CALL_ERR_GOTO(tal_semaphore_create_init(&sg_cloud_api_ctx.sem, 0, 1), __EXIT);
    }

    // Initialize error state
    sg_cloud_api_ctx.error_count = 0;
    sg_cloud_api_ctx.request_interval_ms = DEFAULT_REQUEST_INTERVAL_MS;
    sg_cloud_api_ctx.last_query_time = 0;
    sg_cloud_api_ctx.is_getting = 0;
    sg_cloud_api_ctx.api_rt = OPRT_OK;

    PR_INFO("Cloud API initialized with default interval: %u ms", DEFAULT_REQUEST_INTERVAL_MS);

__EXIT:

    return rt;
}

static void __get_cattle_location_work_queue_cb(void *data)
{
    cattle_location_t *loc = (cattle_location_t *)data;
    memset(loc, 0, sizeof(cattle_location_t));

    OPERATE_RET rt = OPRT_OK;
    cJSON *api_result = NULL;

    // {\"compassDeviceId\":\"xxxx\",\"cattleId\":\"xxxx\"}
    int post_data_len = strlen("{\"compassDeviceId\":\"\",\"cattleId\":\"\",\"t\":}") + MAX_LENGTH_DEVICE_ID +
                        CATTLE_ID_LEN + 20 + 1; // 20 for timestamp, 1 for '\0'

    PR_DEBUG("post_data_len %d", post_data_len);
    char *post_data = CLOUD_API_MALLOC(post_data_len);
    if (post_data == NULL) {
        PR_ERR("malloc failed");
        sg_cloud_api_ctx.api_rt = OPRT_MALLOC_FAILED;
        goto __EXIT;
    }
    memset(post_data, 0, post_data_len);

    TIME_T timestamp = 0;
    timestamp = tal_time_get_posix();
    (void)timestamp;
    uint8_t cattle_id = app_get_current_tracking_id();
    snprintf(post_data, post_data_len, "{\"compassDeviceId\":\"%s\",\"cattleId\":\"%d\",\"t\":%d}",
             tuya_iot_devid_get(tuya_iot_client_get()), cattle_id, timestamp);

    PR_DEBUG("cattle location post data: %s", post_data);
#if defined(ENABLE_DEBUG_VIRTUAL_SIMULATION) && (ENABLE_DEBUG_VIRTUAL_SIMULATION == 1)
    char *cattle_virtual_data[] = {
        "{\"accuracy\":0,\"cattleId\":\"6c1694304986b00e8eabfs\",\"direction\":0,\"lat\":\"31.300437\","
        "\"locationTime\":1760180386499,\"lon\":\"121.068184\",\"speed\":0}"};

    api_result = cJSON_Parse(cattle_virtual_data[0]);

    sg_cloud_api_ctx.api_rt = rt;
#else
    rt = atop_service_comm_post_simple(CATTLE_LOCATION_QUERY_API, CATTLE_LOCATION_QUERY_VER, post_data, NULL,
                                       &api_result);
    sg_cloud_api_ctx.api_rt = rt;
#endif

    if (rt != OPRT_OK) {
        PR_ERR("get cattle location api failed, rt: %d", rt);
        goto __EXIT;
    }

    if (api_result == NULL) {
        PR_ERR("api result is NULL");
        sg_cloud_api_ctx.api_rt = OPRT_COM_ERROR;
        goto __EXIT;
    }

    //{"accuracy":0,"cattleId":"6c1694304986b00e8eabfs","direction":0,"lat":"31.300437","locationTime":1760180386499,"lon":"121.068184","speed":0}
    // Check and parse accuracy
    cJSON *accuracy_item = cJSON_GetObjectItem(api_result, "accuracy");
    if (accuracy_item && cJSON_IsNumber(accuracy_item)) {
        loc->accuracy = accuracy_item->valueint;
    } else {
        PR_WARN("accuracy field not found or invalid, using default 0");
        loc->accuracy = 0;
    }

    // Check and parse cattleId
    cJSON *cattle_id_item = cJSON_GetObjectItem(api_result, "cattleId");
    if (cattle_id_item && cJSON_IsString(cattle_id_item) && cattle_id_item->valuestring) {
        strncpy(loc->cattleId, cattle_id_item->valuestring, CATTLE_ID_LEN - 1);
        loc->cattleId[CATTLE_ID_LEN - 1] = '\0';
    } else {
        PR_WARN("cattleId field not found or invalid, using empty string");
        loc->cattleId[0] = '\0';
    }
    // Check and parse direction
    cJSON *direction_item = cJSON_GetObjectItem(api_result, "direction");
    if (direction_item && cJSON_IsNumber(direction_item)) {
        loc->direction = (uint32_t)direction_item->valueint;
    } else {
        PR_WARN("direction field not found or invalid, using default 0");
        loc->direction = 0;
    }

    // Check and parse lat (use strtod for double precision)
    cJSON *lat_item = cJSON_GetObjectItem(api_result, "lat");
    if (lat_item && cJSON_IsString(lat_item) && lat_item->valuestring) {
        char *endptr = NULL;
        loc->lat = strtod(lat_item->valuestring, &endptr);
        if (endptr == lat_item->valuestring) {
            // Conversion failed
            PR_WARN("lat field conversion failed, using default 0.0");
            loc->lat = 0.0;
        }
    } else {
        PR_WARN("lat field not found or invalid, using default 0.0");
        loc->lat = 0.0;
    }

    // Check and parse lon (use strtod for double precision)
    cJSON *lon_item = cJSON_GetObjectItem(api_result, "lon");
    if (lon_item && cJSON_IsString(lon_item) && lon_item->valuestring) {
        char *endptr = NULL;
        loc->lon = strtod(lon_item->valuestring, &endptr);
        if (endptr == lon_item->valuestring) {
            // Conversion failed
            PR_WARN("lon field conversion failed, using default 0.0");
            loc->lon = 0.0;
        }
    } else {
        PR_WARN("lon field not found or invalid, using default 0.0");
        loc->lon = 0.0;
    }

    // Check and parse locationTime
    cJSON *location_time_item = cJSON_GetObjectItem(api_result, "locationTime");
    if (location_time_item && cJSON_IsNumber(location_time_item)) {
        loc->locationTime = (uint64_t)location_time_item->valuedouble;
    } else {
        PR_WARN("locationTime field not found or invalid, using default 0");
        loc->locationTime = 0;
    }

    // Check and parse speed
    cJSON *speed_item = cJSON_GetObjectItem(api_result, "speed");
    if (speed_item && cJSON_IsNumber(speed_item)) {
        loc->speed = (uint32_t)speed_item->valueint;
    } else {
        PR_WARN("speed field not found or invalid, using default 0");
        loc->speed = 0;
    }

    PR_DEBUG("cattle location: accuracy %d, cattleId %s, direction %d, lat %lf, lon %lf, locationTime "
             "%llu, speed %d",
             loc->accuracy, loc->cattleId, loc->direction, loc->lat, loc->lon, loc->locationTime, loc->speed);

__EXIT:
    // Update error state based on request result
    if (sg_cloud_api_ctx.api_rt == OPRT_OK) {
        __reset_error_state();
    } else {
        __increment_error_count();
    }

    if (post_data) {
        CLOUD_API_FREE(post_data);
        post_data = NULL;
    }

    if (api_result) {
        cJSON_free(api_result);
        api_result = NULL;
    }

    tal_semaphore_post(sg_cloud_api_ctx.sem);

    return;
}

OPERATE_RET cloud_api_get_cattle_location(cattle_location_t *location)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(location, OPRT_INVALID_PARM);

    if (sg_cloud_api_ctx.mutex == NULL || sg_cloud_api_ctx.sem == NULL) {
        PR_ERR("cloud api not init");
        return OPRT_COM_ERROR;
    }

    // check time is sync
    if (OPRT_OK != tal_time_check_time_sync()) {
        // PR_ERR("time not sync");
        return OPRT_COM_ERROR;
    }

    // check network is ready
    extern bool app_check_network_ready(void);
    if (!app_check_network_ready()) {
        PR_WARN("network not ready");
        return OPRT_COM_ERROR;
    }

    SYS_TIME_T now = tal_time_get_posix_ms();
    SYS_TIME_T current_interval = __get_current_request_interval();

    if (now - sg_cloud_api_ctx.last_query_time < current_interval) {
        PR_WARN("query too frequently, current interval: %u ms, time since last: %u ms", (uint32_t)current_interval,
                (uint32_t)(now - sg_cloud_api_ctx.last_query_time));
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(sg_cloud_api_ctx.mutex);

    TUYA_CALL_ERR_GOTO(tal_workq_schedule(WORKQ_SYSTEM, __get_cattle_location_work_queue_cb, location), __EXIT);
    tal_semaphore_wait(sg_cloud_api_ctx.sem, 10 * 1024);
    rt = sg_cloud_api_ctx.api_rt;

    // Always update last_query_time to ensure backoff works correctly
    // whether the request succeeds or fails
    sg_cloud_api_ctx.last_query_time = now;

__EXIT:
    tal_mutex_unlock(sg_cloud_api_ctx.mutex);

    return rt;
}

uint8_t cloud_api_get_error_count(void)
{
    return sg_cloud_api_ctx.error_count;
}

uint32_t cloud_api_get_request_interval(void)
{
    return (uint32_t)sg_cloud_api_ctx.request_interval_ms;
}

void cloud_api_reset_error_state(void)
{
    if (sg_cloud_api_ctx.mutex != NULL) {
        tal_mutex_lock(sg_cloud_api_ctx.mutex);
        __reset_error_state();
        tal_mutex_unlock(sg_cloud_api_ctx.mutex);
    }
}
