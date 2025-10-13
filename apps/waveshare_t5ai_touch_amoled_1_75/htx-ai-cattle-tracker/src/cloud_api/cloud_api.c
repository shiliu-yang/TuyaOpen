/**
 * @file cloud_api.c
 * @brief cloud_api module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "cloud_api.h"

#include "tal_api.h"
#include "tuya_iot.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define CLOUD_API_MALLOC tal_malloc
#define CLOUD_API_FREE   tal_free

// #define CATTLE_LOCATION_QUERY_API "m.outdoors.cattle.location.query"
#define CATTLE_LOCATION_QUERY_API "thing.cattle.location.query"
#define CATTLE_LOCATION_QUERY_VER "1.0"

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    MUTEX_HANDLE mutex;
    SEM_HANDLE sem;
    OPERATE_RET api_rt;
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
};
/***********************************************************
***********************function define**********************
***********************************************************/

OPERATE_RET cloud_api_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (sg_cloud_api_ctx.mutex == NULL) {
        TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&sg_cloud_api_ctx.mutex), __EXIT);
    }

    if (sg_cloud_api_ctx.sem == NULL) {
        TUYA_CALL_ERR_GOTO(tal_semaphore_create_init(&sg_cloud_api_ctx.sem, 0, 1), __EXIT);
    }

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
    snprintf(post_data, post_data_len, "{\"compassDeviceId\":\"%s\",\"cattleId\":\"%d\",\"t\":%d}",
             tuya_iot_devid_get(tuya_iot_client_get()), 1, timestamp);

    PR_DEBUG("cattle location post data: %s", post_data);

    rt = atop_service_comm_post_simple(CATTLE_LOCATION_QUERY_API, CATTLE_LOCATION_QUERY_VER, post_data, NULL,
                                       &api_result);
    sg_cloud_api_ctx.api_rt = rt;

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
        PR_ERR("time not sync");
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(sg_cloud_api_ctx.mutex);

    TUYA_CALL_ERR_GOTO(tal_workq_schedule(WORKQ_SYSTEM, __get_cattle_location_work_queue_cb, location), __EXIT);
    tal_semaphore_wait(sg_cloud_api_ctx.sem, SEM_WAIT_FOREVER);
    rt = sg_cloud_api_ctx.api_rt;

__EXIT:
    tal_mutex_unlock(sg_cloud_api_ctx.mutex);

    return rt;
}
