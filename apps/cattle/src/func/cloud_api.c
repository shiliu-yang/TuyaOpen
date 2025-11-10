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
#include "app_system_info.h"

#if defined(ENABLE_GUI_TRACKER) && (ENABLE_GUI_TRACKER == 1)
#include "cattle_ai_tracker_app.h"
#include "ui_display.h"
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define CLOUD_API_MALLOC tal_psram_malloc
#define CLOUD_API_FREE   tal_psram_free

// #define CATTLE_LOCATION_QUERY_API "m.outdoors.cattle.location.query"
#define CATTLE_LOCATION_QUERY_API "thing.cattle.location.query"
#define CATTLE_LOCATION_QUERY_VER "1.0"

#define DEFAULT_REQUEST_INTERVAL_MS (30 * 1000) // default minimum interval between requests in milliseconds

#define KEY_CATTLE_ID "cattleId"

#define CLOUD_API_TASK_PRIORITY THREAD_PRIO_4
#define CLOUD_API_TASK_STACK_SIZE 4096

#define ENABLE_DEBUG_VIRTUAL_SIMULATION 0

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    THREAD_HANDLE thread;
    MUTEX_HANDLE mutex;
    SEM_HANDLE sem;
    int cattle_id;
    OPERATE_RET api_rt;
    uint8_t is_getting;
    uint8_t error_count;
    SYS_TIME_T last_query_time;
    SYS_TIME_T request_interval_ms;
} cloud_api_ctx_t;

/***********************************************************
********************function declaration********************
***********************************************************/
static OPERATE_RET __get_cattle_location(cattle_location_t *location, uint8_t force_update);

/***********************************************************
***********************variable define**********************
***********************************************************/
static cloud_api_ctx_t sg_cloud_api_ctx = {
    .thread = NULL,
    .mutex = NULL,
    .sem = NULL,
    .cattle_id = 1,
    .api_rt = OPRT_OK,
    .is_getting = 0,
    .error_count = 0,
    .last_query_time = 0,
    .request_interval_ms = DEFAULT_REQUEST_INTERVAL_MS, // minimum interval between requests in milliseconds
};

#define MAX_ERROR_COUNT 8
static SYS_TIME_T sg_request_interval_ms[] = {
    DEFAULT_REQUEST_INTERVAL_MS, // 30 seconds  - error_count 0
    10 * 1000,                   // 10 seconds  - error_count 1
    20 * 1000,                   // 20 seconds  - error_count 2
    30 * 1000,                   // 30 seconds  - error_count 3
    40 * 1000,                   // 40 seconds  - error_count 4
    50 * 1000,                   // 50 seconds  - error_count 5
    60 * 1000,                   // 1 minute    - error_count 6
    3 * 60 * 1000,               // 3 minutes   - error_count 7
    5 * 60 * 1000,               // 5 minutes   - error_count 8+
};

static cattle_location_t sg_cattle_location = {0};

static uint8_t sg_need_force_update = 0;

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

    if (error_count > MAX_ERROR_COUNT) {
        PR_WARN("error_count %d exceeds max_index %d, clamping to max", error_count, MAX_ERROR_COUNT);
        error_count = MAX_ERROR_COUNT;
        sg_cloud_api_ctx.error_count = MAX_ERROR_COUNT;
    }

    SYS_TIME_T interval = sg_request_interval_ms[error_count];
    // PR_DEBUG("get_interval: error_count=%d, max_index=%d, interval=%u ms", error_count, max_index,
    // (uint32_t)interval);

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
    } else {
        sg_cloud_api_ctx.error_count = MAX_ERROR_COUNT;
    }

    sg_cloud_api_ctx.request_interval_ms = __get_current_request_interval();

    PR_WARN("Request failed, error_count: %d, next interval: %u ms", sg_cloud_api_ctx.error_count,
            (uint32_t)sg_cloud_api_ctx.request_interval_ms);
}

void cloud_api_cattle_id_set(int cattle_id)
{
    sg_cloud_api_ctx.cattle_id = cattle_id;
    tal_kv_set(KEY_CATTLE_ID, (uint8_t *)&cattle_id, sizeof(cattle_id));
    sg_need_force_update = 1;
}

int cloud_api_cattle_id_get(void)
{
    return sg_cloud_api_ctx.cattle_id;
}

static void cloud_api_thread_cb(void *args)
{
    uint32_t time_tick = 0;

    while (1) {
        tal_system_sleep(1000);
        time_tick++;

        if (time_tick >= 10 || sg_need_force_update) {
            cattle_location_t location = {0};
            OPERATE_RET rt = __get_cattle_location(&location, sg_need_force_update);
            if (rt != OPRT_OK) {
                // PR_ERR("get cattle location failed, rt: %d", rt);
                continue;
            }
            memcpy(&sg_cattle_location, &location, sizeof(cattle_location_t));
            PR_DEBUG("cattle location: %lf, %lf", location.lat, location.lon);
            time_tick = 0;
            sg_need_force_update = 0;
        }


    }
}

OPERATE_RET cloud_api_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    uint8_t *p_cattle_id = 0;
    size_t length = 0;
    rt = tal_kv_get(KEY_CATTLE_ID, &p_cattle_id, &length);
    if (rt == OPRT_OK && length == sizeof(sg_cloud_api_ctx.cattle_id)) {
        sg_cloud_api_ctx.cattle_id = *((int *)p_cattle_id);
        tal_kv_free(p_cattle_id);
        p_cattle_id = NULL;
        PR_INFO("Loaded cattle_id %d from KV store", sg_cloud_api_ctx.cattle_id);
    }

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

    if (sg_cloud_api_ctx.thread == NULL) {
        THREAD_CFG_T task_cfg = {
            .priority = CLOUD_API_TASK_PRIORITY,
            .stackDepth = CLOUD_API_TASK_STACK_SIZE,
            .thrdname = "cattle_api"
        };
        TUYA_CALL_ERR_RETURN(tal_thread_create_and_start(&sg_cloud_api_ctx.thread, NULL, NULL, cloud_api_thread_cb, NULL, &task_cfg));
    }

    PR_INFO("Cloud API initialized with default interval: %u ms", DEFAULT_REQUEST_INTERVAL_MS);

__EXIT:

    return rt;
}

#if defined(ENABLE_DEBUG_VIRTUAL_SIMULATION) && (ENABLE_DEBUG_VIRTUAL_SIMULATION == 1)
static const char *cattle_virtual_data[] = {
    "{\"accuracy\":0,\"cattleId\":\"6c1694304986b00e8eabfs\",\"direction\":0,\"lat\":\"30.304574\","
    "\"locationTime\":1760180386499,\"lon\":\"120.059163\",\"speed\":0}",
    "{\"accuracy\":0,\"cattleId\":\"6c1694304986b00e8eabfs\",\"direction\":0,\"lat\":\"30.304852\","
    "\"locationTime\":1760180386499,\"lon\":\"120.067508\",\"speed\":0}",
    "{\"accuracy\":0,\"cattleId\":\"6c1694304986b00eabfs\",\"direction\":0,\"lat\":\"30.304557\","
    "\"locationTime\":1760180386499,\"lon\":\"120.072992\",\"speed\":0}",
    "{\"accuracy\":0,\"cattleId\":\"6c1694304986b00e8eabfs\",\"direction\":0,\"lat\":\"30.301117\","
    "\"locationTime\":1760180386499,\"lon\":\"120.074899\",\"speed\":0}",
    "{\"accuracy\":0,\"cattleId\":\"6c1694304986b00e8eabfs\",\"direction\":0,\"lat\":\"30.298440\","
    "\"locationTime\":1760180386499,\"lon\":\"120.073929\",\"speed\":0}",
    "{\"accuracy\":0,\"cattleId\":\"6c1694304986b00e8eabfs\",\"direction\":0,\"lat\":\"30.296779\","
    "\"locationTime\":1760180386499,\"lon\":\"120.067661\",\"speed\":0}",
    "{\"accuracy\":0,\"cattleId\":\"6c1694304986b00e8eabfs\",\"direction\":0,\"lat\":\"30.295926\","
    "\"locationTime\":1760180386499,\"lon\":\"120.059691\",\"speed\":0}",
    "{\"accuracy\":0,\"cattleId\":\"6c1694304986b00e8eabfs\",\"direction\":0,\"lat\":\"30.300073\","
    "\"locationTime\":1760180386499,\"lon\":\"120.059776\",\"speed\":0}",
    "{\"accuracy\":0,\"cattleId\":\"6c1694304986b00e8eabfs\",\"direction\":0,\"lat\":\"30.301587\","
    "\"locationTime\":1760180386499,\"lon\":\"120.057681\",\"speed\":0}",
};
#endif

static void __attribute__((unused)) __get_cattle_location_work_queue_cb(void *data)
{
    OPERATE_RET rt = OPRT_OK;
    cJSON *api_result = NULL;
    char *post_data = NULL;

    cattle_location_t *loc = (cattle_location_t *)data;
    memset(loc, 0, sizeof(cattle_location_t));

    // {\"compassDeviceId\":\"xxxx\",\"cattleId\":\"xxxx\"}
    int post_data_len = strlen("{\"compassDeviceId\":\"\",\"cattleId\":\"\",\"t\":}") + MAX_LENGTH_DEVICE_ID +
                        CATTLE_ID_LEN + 20 + 1; // 20 for timestamp, 1 for '\0'

    PR_DEBUG("post_data_len %d", post_data_len);
    post_data = CLOUD_API_MALLOC(post_data_len);
    if (post_data == NULL) {
        PR_ERR("malloc failed");
        sg_cloud_api_ctx.api_rt = OPRT_MALLOC_FAILED;
        goto __EXIT;
    }
    memset(post_data, 0, post_data_len);

    TIME_T timestamp = 0;
    timestamp = tal_time_get_posix();
    (void)timestamp;
    uint8_t cattle_id = cloud_api_cattle_id_get();
    snprintf(post_data, post_data_len, "{\"compassDeviceId\":\"%s\",\"cattleId\":\"%d\",\"t\":%d}",
             tuya_iot_devid_get(tuya_iot_client_get()), cattle_id, timestamp);

    PR_DEBUG("cattle location post data: %s", post_data);
#if defined(ENABLE_DEBUG_VIRTUAL_SIMULATION) && (ENABLE_DEBUG_VIRTUAL_SIMULATION == 1)
    static int idx = 0;
    api_result = cJSON_Parse(cattle_virtual_data[idx]);
    PR_DEBUG("Using virtual simulation data: index %d", idx);
    idx = (idx + 1) % (sizeof(cattle_virtual_data) / sizeof(cattle_virtual_data[0]));

    sg_cloud_api_ctx.api_rt = OPRT_OK;
#else
    rt = atop_service_comm_post_simple(CATTLE_LOCATION_QUERY_API, CATTLE_LOCATION_QUERY_VER, post_data, NULL,
                                       &api_result);
    sg_cloud_api_ctx.api_rt = rt;
#endif
    CLOUD_API_FREE(post_data);
    post_data = NULL;

    if (rt != OPRT_OK) {
        PR_ERR("get cattle location api failed, rt: %d", rt);
        goto __EXIT;
    }

    if (api_result == NULL) {
        PR_ERR("api result is NULL");
        sg_cloud_api_ctx.api_rt = OPRT_COM_ERROR;
        goto __EXIT;
    }

    //{"accuracy":0,"cattleId":"6c1694304986b00exxxx","direction":0,"lat":"31.300437","locationTime":1760180386499,"lon":"121.068184","speed":0}
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
        cJSON_Delete(api_result);
        api_result = NULL;
    }

    tal_semaphore_post(sg_cloud_api_ctx.sem);

    return;
}

static OPERATE_RET __get_cattle_location(cattle_location_t *location, uint8_t force_update)
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
    if (!app_network_is_ready()) {
        PR_WARN("network not ready");
        return OPRT_COM_ERROR;
    }

    SYS_TIME_T now = tal_time_get_posix_ms();
    SYS_TIME_T current_interval = __get_current_request_interval();

    if (now - sg_cloud_api_ctx.last_query_time < current_interval && !force_update) {
        // PR_WARN("query too frequently, current interval: %u ms, time since last: %u ms", (uint32_t)current_interval,
        //         (uint32_t)(now - sg_cloud_api_ctx.last_query_time));
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(sg_cloud_api_ctx.mutex);

    TUYA_CALL_ERR_GOTO(tal_workq_schedule(WORKQ_SYSTEM, __get_cattle_location_work_queue_cb, location), __EXIT);
    tal_semaphore_wait(sg_cloud_api_ctx.sem, 10 * 1000);
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

OPERATE_RET cloud_api_get_cattle_location(cattle_location_t *location)
{
    if (location == NULL) {
        return OPRT_INVALID_PARM;
    }
    memcpy(location, &sg_cattle_location, sizeof(cattle_location_t));
    return OPRT_OK;
}
