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

#include "tdd_audio_8311_codec.h"

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

void app_display_init(void)
{
    return;
}

void app_display_set_status(const char *status)
{
    return;
}

void app_display_show_notification(const char *notification)
{
    return;
}

void app_display_set_emotion(const char *emotion)
{
    return;
}

void app_display_set_chat_massage(CHAT_ROLE_E role, const char *content)
{
    return;
}

void app_display_set_wifi_status(DIS_WIFI_STATUS_E status)
{
    return;
}
