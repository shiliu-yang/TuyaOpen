/**
 * @file bread_compact_wifi.c
 * @brief bread_compact_wifi module is used to
 * @version 0.1
 * @date 2025-04-23
 */

#include "tuya_cloud_types.h"

#include "app_board_api.h"

#include "board_config.h"
#include "oled_display.h"

#include "tdd_audio_no_codec.h"

#include "tkl_memory.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/

OPERATE_RET app_audio_driver_init(const char *name)
{
    TDD_AUDIO_NO_CODEC_T cfg = {0};
    cfg.i2s_id = 0;
    cfg.mic_sample_rate = 16000;
    cfg.spk_sample_rate = 16000;

    return tdd_audio_no_codec_register(name, cfg);
}

OPERATE_RET app_display_init(void)
{
    oled_ssd1306_init(OLED_I2C_SCL, OLED_I2C_SDA, OLED_WIDTH, OLED_HEIGHT);

#if defined(OLED_128x32)
    oled_setup_ui_128x32();
#elif defined(OLED_128x64)
    oled_setup_ui_128x64();
#endif

    return OPRT_OK;
}

OPERATE_RET app_display_send_msg(TY_DISPLAY_TYPE_E tp, uint8_t *data, int len)
{
    uint8_t *p_data = NULL;

    if (len > 0) {
        p_data = tkl_system_malloc(len + 1);
        if (p_data == NULL) {
            return OPRT_MALLOC_FAILED;
        }
        memset(p_data, 0, len + 1);
        memcpy(p_data, data, len);
    }

    switch (tp) {
    case TY_DISPLAY_TP_USER_MSG: {
        oled_set_chat_message(CHAT_ROLE_USER, p_data);
    } break;
    case TY_DISPLAY_TP_ASSISTANT_MSG: {
        oled_set_chat_message(CHAT_ROLE_ASSISTANT, p_data);
    } break;
    case TY_DISPLAY_TP_SYSTEM_MSG: {
        oled_set_chat_message(CHAT_ROLE_SYSTEM, p_data);
    } break;
    case TY_DISPLAY_TP_EMOTION: {
        oled_set_emotion(p_data);
    } break;
    case TY_DISPLAY_TP_STATUS: {
        oled_set_status(p_data);
    } break;
    case TY_DISPLAY_TP_NOTIFICATION: {
        oled_show_notification(p_data);
    } break;
    case TY_DISPLAY_TP_NETWORK: {
        UI_WIFI_STATUS_E status = p_data[0];
        oled_set_wifi_status(status);
    } break;
    default: {
        return OPRT_INVALID_PARM;
    } break;
    }

    if (p_data != NULL) {
        tkl_system_free(p_data);
        p_data = NULL;
    }
    return OPRT_OK;
}
