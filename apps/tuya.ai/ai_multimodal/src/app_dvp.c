/**
 * @file app_dvp.c
 * @brief app_dvp module is used to
 * @version 0.1
 * @date 2025-06-04
 */

#include "app_dvp.h"

#include "tal_log.h"

#include "tkl_video_in.h"
#include "tkl_video_enc.h"

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

static int __h264_cb(TKL_VENC_FRAME_T *pframe)
{
    OPERATE_RET rt = OPRT_OK;

    if (pframe->pbuf == NULL || pframe->buf_size == 0) {
        return 0;
    }

    if (pframe->frametype != TKL_VIDEO_I_FRAME) {
        return 0;
    }

    PR_DEBUG("H264 frame: width=%d, height=%d, size=%d, pts=%llu, timestamp=%llu", pframe->width, pframe->height,
             pframe->used_size, pframe->pts, pframe->timestamp);
}

OPERATE_RET app_dvp_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    // Initialize the DVP driver
    TKL_VI_CONFIG_T vi_config;
    TKL_VI_EXT_CONFIG_T ext_conf;

    ext_conf.type = TKL_VI_EXT_CONF_CAMERA;
    ext_conf.camera.camera_type = TKL_VI_CAMERA_TYPE_DVP;
    ext_conf.camera.fmt = TKL_CODEC_VIDEO_H264;
    ext_conf.camera.power_pin = TUYA_GPIO_NUM_51;
    ext_conf.camera.active_level = TUYA_GPIO_LEVEL_HIGH;
    ext_conf.camera.i2c.clk = TUYA_GPIO_NUM_13;
    ext_conf.camera.i2c.sda = TUYA_GPIO_NUM_15;

    vi_config.isp.width = 480;
    vi_config.isp.height = 480;
    vi_config.isp.fps = 15;

    vi_config.pdata = &ext_conf;

    rt = tkl_vi_init(&vi_config, 0);
    if (rt != OPRT_OK) {
        PR_ERR("tkl_vi_init failed: %d", rt);
        return rt;
    }

    TKL_VENC_CONFIG_T h264_config;
    // DVP:0; UVC:1
    h264_config.enable_h264_pipeline = 0; // dvp
    h264_config.put_cb = __h264_cb;
    rt = tkl_venc_init(0, &h264_config, 0);
    if (rt != OPRT_OK) {
        PR_ERR("tkl_venc_init failed: %d", rt);
        return rt;
    }

    return rt;
}

void app_dvp_deinit(void)
{
    OPERATE_RET rt = OPRT_OK;

    TKL_VENC_CONFIG_T h264_config;
    // DVP:0; UVC:1
    h264_config.enable_h264_pipeline = 0; // dvp
    h264_config.put_cb = __h264_cb;
    rt = tkl_venc_uninit(0, &h264_config);
    if (rt != OPRT_OK) {
        PR_ERR("tkl_venc_uninit failed: %d", rt);
    }

    tkl_vi_uninit(TKL_VI_CAMERA_TYPE_DVP);

    return;
}
