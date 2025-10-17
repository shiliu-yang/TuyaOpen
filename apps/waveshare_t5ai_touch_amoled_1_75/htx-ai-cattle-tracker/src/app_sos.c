/**
 * @file app_sos.c
 * @brief app_sos module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "app_sos.h"

#include "app_dp.h"

#include "cattle_ai_tracker_app.h"

#include "tal_api.h"

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
static bool sg_sos_status = false;
static MUTEX_HANDLE sg_sos_mutex = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/

void app_sos_set(bool sos_status)
{
    if (sg_sos_mutex == NULL) {
        OPERATE_RET ret = tal_mutex_create_init(&sg_sos_mutex);
        if (ret != OPRT_OK) {
            PR_ERR("[SOS] Failed to create mutex (error: %d)", ret);
        }
    }

    if (sg_sos_mutex) {
        tal_mutex_lock(sg_sos_mutex);
    }

    // update dp
    app_dp_sos_set(sos_status);

    // update ui
    set_sos_visible(sos_status);

    sg_sos_status = sos_status;

    if (sg_sos_mutex) {
        tal_mutex_unlock(sg_sos_mutex);
    }
}

bool app_sos_get(void)
{
    return sg_sos_status;
}
