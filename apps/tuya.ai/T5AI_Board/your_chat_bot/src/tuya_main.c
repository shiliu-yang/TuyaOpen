/**
 * @file tuya_main.c
 * @brief Implements main audio functionality for IoT device
 *
 * This source file provides the implementation of the main audio functionalities
 * required for an IoT device. It includes functionality for audio processing,
 * device initialization, event handling, and network communication. The
 * implementation supports audio volume control, data point processing, and
 * interaction with the Tuya IoT platform. This file is essential for developers
 * working on IoT applications that require audio capabilities and integration
 * with the Tuya IoT ecosystem.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include <assert.h>
#include "cJSON.h"
#include "tal_api.h"
#include "tuya_config.h"
#include "tuya_iot.h"
#include "tuya_iot_dp.h"
#include "netmgr.h"
#include "tkl_output.h"
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
#include "netconn_wifi.h"
#endif
#if defined(ENABLE_WIRED) && (ENABLE_WIRED == 1)
#include "netconn_wired.h"
#endif
#if defined(ENABLE_LIBLWIP) && (ENABLE_LIBLWIP == 1)
#include "lwip_init.h"
#endif

#include "ai_audio_proc.h"

/* Tuya device handle */
tuya_iot_client_t ai_client;

#ifndef PROJECT_VERSION
#define PROJECT_VERSION "1.0.0"
#endif

#define VOLUME_KEY_NAME "VOLUME"
#define DEFAULT_VOLUME  50

#define DPID_VOLUME 3

/**
 * @brief user defined log output api, in this demo, it will use uart0 as log-tx
 *
 * @param str log string
 * @return void
 */
void user_log_output_cb(const char *str)
{
    tal_uart_write(TUYA_UART_NUM_0, (const uint8_t *)str, strlen(str));
}

/**
 * @brief user defined upgrade notify callback, it will notify device a OTA request received
 *
 * @param client device info
 * @param upgrade the upgrade request info
 * @return void
 */
void user_upgrade_notify_on(tuya_iot_client_t *client, cJSON *upgrade)
{
    PR_INFO("----- Upgrade information -----");
    PR_INFO("OTA Channel: %d", cJSON_GetObjectItem(upgrade, "type")->valueint);
    PR_INFO("Version: %s", cJSON_GetObjectItem(upgrade, "version")->valuestring);
    PR_INFO("Size: %s", cJSON_GetObjectItem(upgrade, "size")->valuestring);
    PR_INFO("MD5: %s", cJSON_GetObjectItem(upgrade, "md5")->valuestring);
    PR_INFO("HMAC: %s", cJSON_GetObjectItem(upgrade, "hmac")->valuestring);
    PR_INFO("URL: %s", cJSON_GetObjectItem(upgrade, "url")->valuestring);
    PR_INFO("HTTPS URL: %s", cJSON_GetObjectItem(upgrade, "httpsUrl")->valuestring);
}

OPERATE_RET audio_dp_obj_proc(dp_obj_recv_t *dpobj)
{
    uint32_t index = 0;
    for (index = 0; index < dpobj->dpscnt; index++) {
        dp_obj_t *dp = dpobj->dps + index;
        PR_DEBUG("idx:%d dpid:%d type:%d ts:%u", index, dp->id, dp->type, dp->time_stamp);

        switch (dp->id) {
        case DPID_VOLUME: {
            uint8_t volume = dp->value.dp_value;
            PR_DEBUG("volume:%d", volume);
            audio_volume_set(volume);
            tuya_audio_player_set_volume(volume);
            break;
        }

        default: {
            break;
        }
        }
    }

    return OPRT_OK;
}

void audio_volume_set(uint8_t volue)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_LOG(tal_kv_set(VOLUME_KEY_NAME, &volue, sizeof(volue)));
}

uint8_t audio_volume_get(void)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t volue = 0;
    size_t read_len;
    uint8_t *read_volue = NULL;

    rt = tal_kv_get(VOLUME_KEY_NAME, &read_volue, &read_len);
    if (rt != OPRT_OK) {
        PR_ERR("read volume failed");
        return DEFAULT_VOLUME;
    }

    volue = *read_volue;
    TUYA_CALL_ERR_LOG(tal_kv_free(read_volue));

    return volue;
}
OPERATE_RET ai_audio_status_proc(void)
{
    OPERATE_RET rt = OPRT_OK;
    tuya_iot_client_t *client = tuya_iot_client_get();
    dp_obj_t dp_obj = {0};

    uint8_t volume = audio_volume_get();

    dp_obj.id = DPID_VOLUME;
    dp_obj.type = PROP_VALUE;
    dp_obj.value.dp_value = volume;

    return tuya_iot_dp_obj_report(client, client->activate.devid, &dp_obj, 1, 0);
}
/**
 * @brief user defined event handler
 *
 * @param client device info
 * @param event the event info
 * @return void
 */
void user_event_handler_on(tuya_iot_client_t *client, tuya_event_msg_t *event)
{
    PR_DEBUG("Tuya Event ID:%d(%s)", event->id, EVENT_ID2STR(event->id));
    PR_INFO("Device Free heap %d", tal_system_get_free_heap_size());

    switch (event->id) {
    case TUYA_EVENT_BIND_START:
        PR_INFO("Device Bind Start!");
        // 软重启，未配网，播报配网提示
        tuya_audio_player_play_alert(AUDIO_ALART_TYPE_NETWORK_CFG, TRUE);
        break;

    /* MQTT with tuya cloud is connected, device online */
    case TUYA_EVENT_MQTT_CONNECTED:
        PR_INFO("Device MQTT Connected!");
        tal_event_publish(EVENT_MQTT_CONNECTED, NULL);
        break;

    /* MQTT with tuya cloud is disconnected, device offline */
    case TUYA_EVENT_MQTT_DISCONNECT:
        PR_INFO("Device MQTT DisConnected!");
        tal_event_publish(EVENT_MQTT_DISCONNECTED, NULL);
        break;

    /* RECV upgrade request */
    case TUYA_EVENT_UPGRADE_NOTIFY:
        user_upgrade_notify_on(client, event->value.asJSON);
        break;

    /* Sync time with tuya Cloud */
    case TUYA_EVENT_TIMESTAMP_SYNC:
        PR_INFO("Sync timestamp:%d", event->value.asInteger);
        tal_time_set_posix(event->value.asInteger, 1);
        break;

    case TUYA_EVENT_RESET:
        PR_INFO("Device Reset:%d", event->value.asInteger);
        tal_event_publish(EVENT_RESET, NULL);
        break;

    /* RECV OBJ DP */
    case TUYA_EVENT_DP_RECEIVE_OBJ: {
        dp_obj_recv_t *dpobj = event->value.dpobj;
        PR_DEBUG("SOC Rev DP Cmd t1:%d t2:%d CNT:%u", dpobj->cmd_tp, dpobj->dtt_tp, dpobj->dpscnt);
        if (dpobj->devid != NULL) {
            PR_DEBUG("devid.%s", dpobj->devid);
        }

        audio_dp_obj_proc(dpobj);

        tuya_iot_dp_obj_report(client, dpobj->devid, dpobj->dps, dpobj->dpscnt, 0);

    } break;

    /* RECV RAW DP */
    case TUYA_EVENT_DP_RECEIVE_RAW: {
        dp_raw_recv_t *dpraw = event->value.dpraw;
        PR_DEBUG("SOC Rev DP Cmd t1:%d t2:%d", dpraw->cmd_tp, dpraw->dtt_tp);
        if (dpraw->devid != NULL) {
            PR_DEBUG("devid.%s", dpraw->devid);
        }

        uint32_t index = 0;
        dp_raw_t *dp = &dpraw->dp;
        PR_DEBUG("dpid:%d type:RAW len:%d data:", dp->id, dp->len);
        for (index = 0; index < dp->len; index++) {
            PR_DEBUG_RAW("%02x", dp->data[index]);
        }

        tuya_iot_dp_raw_report(client, dpraw->devid, &dpraw->dp, 3);

    } break;

    default:
        break;
    }
}

/**
 * @brief user defined network check callback, it will check the network every 1sec,
 *        in this demo it alwasy return ture due to it's a wired demo
 *
 * @return true
 * @return false
 */
bool user_network_check(void)
{
    netmgr_status_e status = NETMGR_LINK_DOWN;
    netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_STATUS, &status);
    return status == NETMGR_LINK_DOWN ? false : true;
}

void user_main(void)
{
    int ret = OPRT_OK;

    //! open iot development kit runtim init
    cJSON_InitHooks(&(cJSON_Hooks){.malloc_fn = tal_malloc, .free_fn = tal_free});
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);
    tal_kv_init(&(tal_kv_cfg_t){
        .seed = "vmlkasdh93dlvlcy",
        .key = "dflfuap134ddlduq",
    });
    tal_sw_timer_init();
    tal_workq_init();

    reset_netconfig_init();

    tuya_iot_license_t license;

    if (OPRT_OK != tuya_iot_license_read(&license)) {
        license.uuid = TUYA_OPENSDK_UUID;
        license.authkey = TUYA_OPENSDK_AUTHKEY;
        PR_WARN("Replace the TUYA_OPENSDK_UUID and TUYA_OPENSDK_AUTHKEY contents, otherwise the demo cannot work.\n \
                Visit https://platform.tuya.com/purchase/index?type=6 to get the open-sdk uuid and authkey.");
    }

    /* Initialize Tuya device configuration */
    ret = tuya_iot_init(&ai_client, &(const tuya_iot_config_t){
                                        .software_ver = PROJECT_VERSION,
                                        .productkey = TUYA_PRODUCT_KEY,
                                        .uuid = license.uuid,
                                        .authkey = license.authkey,
                                        // .firmware_key      = TUYA_DEVICE_FIRMWAREKEY,
                                        .event_handler = user_event_handler_on,
                                        .network_check = user_network_check,
                                    });
    assert(ret == OPRT_OK);

    // 初始化LWIP
#if defined(ENABLE_LIBLWIP) && (ENABLE_LIBLWIP == 1)
    TUYA_LwIP_Init();
#endif

    // network init
    netmgr_type_e type = 0;
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    type |= NETCONN_WIFI;
#endif
#if defined(ENABLE_WIRED) && (ENABLE_WIRED == 1)
    type |= NETCONN_WIRED;
#endif
    netmgr_init(type);
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    netmgr_conn_set(NETCONN_WIFI, NETCONN_CMD_NETCFG, &(netcfg_args_t){.type = NETCFG_TUYA_BLE});
#endif

    PR_DEBUG("tuya_iot_init success");

    ret = tuya_ai_audio_init();
    if (ret != OPRT_OK) {
        PR_ERR("tuya_audio_recorde_init failed");
        return;
    }

    /* Start tuya iot task */
    tuya_iot_start(&ai_client);

    tuya_display_init();

    reset_netconfig_check();

    for (;;) {
        /* Loop to receive packets, and handles client keepalive */
        tuya_iot_yield(&ai_client);
    }
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
    THREAD_CFG_T thrd_param = {4096+1024, 4, "tuya_app_main"};
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif
