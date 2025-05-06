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

#if USE_8311
#include "tdd_audio_8311_codec.h"
#else
#include "tdd_audio_atk_no_codec.h"
#endif

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
#if USE_8311
    TDD_AUDIO_8311_CODEC_T cfg = {0};
#else
    TDD_AUDIO_ATK_NO_CODEC_T cfg = {0};
#endif

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

#if USE_8311
    return tdd_audio_8311_codec_register(name, cfg);
#else
    return tdd_audio_atk_no_codec_register(name, cfg);
#endif
}

OPERATE_RET app_display_init(void)
{
    return OPRT_OK;
}

OPERATE_RET app_display_send_msg(TY_DISPLAY_TYPE_E tp, uint8_t *data, int len)
{
    return OPRT_OK;
}
