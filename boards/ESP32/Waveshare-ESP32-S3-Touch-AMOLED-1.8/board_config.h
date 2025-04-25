/**
 * @file board_config.h
 * @brief board_config module is used to
 * @version 0.1
 * @date 2025-04-23
 */

#ifndef __BOARD_CONFIG_H__
#define __BOARD_CONFIG_H__

#include "sdkconfig.h"
#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
/* Example configurations */
#define I2S_INPUT_SAMPLE_RATE     (16000)
#define I2S_OUTPUT_SAMPLE_RATE     (16000)

/* I2C port and GPIOs */
#define I2C_NUM         (0)
#define I2C_SCL_IO      (14)
#define I2C_SDA_IO      (15)

/* I2S port and GPIOs */
#define I2S_NUM         (0)
#define I2S_MCK_IO      (16)
#define I2S_BCK_IO      (9)
#define I2S_WS_IO       (45)

#define I2S_DO_IO       (8)
#define I2S_DI_IO       (10)

#define GPIO_OUTPUT_PA  (46)

#define AUDIO_CODEC_DMA_DESC_NUM (6)
#define AUDIO_CODEC_DMA_FRAME_NUM (240)
#define AUDIO_CODEC_ES8311_ADDR (0x30)

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_CONFIG_H__ */
