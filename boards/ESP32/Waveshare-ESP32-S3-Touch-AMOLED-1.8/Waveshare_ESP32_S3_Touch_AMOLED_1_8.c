/**
 * @file bread_compact_wifi.c
 * @brief bread_compact_wifi module is used to
 * @version 0.1
 * @date 2025-04-23
 */

#include "tuya_cloud_types.h"

#include "app_board_api.h"

#include "board_config.h"

#include "tdd_audio_8311_codec.h"

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

int app_audio_driver_init(const char *name)
{
    TDD_AUDIO_8311_CODEC_T cfg = {0};
    cfg.i2c_id = I2C_NUM;
    cfg.i2c_scl_io = I2C_SCL_IO;
    cfg.i2c_sda_io = I2C_SDA_IO;
    cfg.mic_sample_rate = I2S_INPUT_SAMPLE_RATE;
    cfg.spk_sample_rate = I2S_OUTPUT_SAMPLE_RATE;
    cfg.i2s_id = I2S_NUM;
    cfg.i2s_mck_io = I2S_MCK_IO;
    cfg.i2s_bck_io = I2S_BCK_IO;
    cfg.i2s_ws_io = I2S_WS_IO;
    cfg.i2s_do_io = I2S_DO_IO;
    cfg.i2s_di_io = I2S_DI_IO;
    cfg.gpio_output_pa = GPIO_OUTPUT_PA;
    cfg.es8311_addr = AUDIO_CODEC_ES8311_ADDR;
    cfg.dma_desc_num = AUDIO_CODEC_DMA_DESC_NUM;
    cfg.dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM;
    cfg.defaule_volume = 80;

    return tdd_audio_8311_codec_register(name, cfg);
}

OPERATE_RET app_display_init(void)
{

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

    // switch (tp) {
    // case TY_DISPLAY_TP_USER_MSG: {
    //     oled_set_chat_message(CHAT_ROLE_USER, p_data);
    // } break;
    // case TY_DISPLAY_TP_ASSISTANT_MSG: {
    //     oled_set_chat_message(CHAT_ROLE_ASSISTANT, p_data);
    // } break;
    // case TY_DISPLAY_TP_SYSTEM_MSG: {
    //     oled_set_chat_message(CHAT_ROLE_SYSTEM, p_data);
    // } break;
    // case TY_DISPLAY_TP_EMOTION: {
    //     oled_set_emotion(p_data);
    // } break;
    // case TY_DISPLAY_TP_STATUS: {
    //     oled_set_status(p_data);
    // } break;
    // case TY_DISPLAY_TP_NOTIFICATION: {
    //     oled_show_notification(p_data);
    // } break;
    // case TY_DISPLAY_TP_NETWORK: {
    //     UI_WIFI_STATUS_E status = p_data[0];
    //     oled_set_wifi_status(status);
    // } break;
    // default: {
    //     return OPRT_INVALID_PARM;
    // } break;
    // }

    if (p_data != NULL) {
        tkl_system_free(p_data);
        p_data = NULL;
    }

    return OPRT_OK;
}
