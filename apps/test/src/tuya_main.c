/**
 * @file tuya_main.c
 * @brief tuya_main module is used to manage the Tuya device application.
 *
 * This file provides the implementation of the tuya_main module,
 * which is responsible for managing the Tuya device application.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 * 2025-07-11   yangjie     Add cellular network support.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"

#include "app_at_modem.h"

#include "tkl_output.h"

void user_main(void)
{
    int rt = OPRT_OK;

    //! open iot development kit runtim init
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("TuyaOpen commit-id:  %s", OPEN_COMMIT);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("Platform commit-id:  %s", PLATFORM_COMMIT);

    tal_kv_init(&(tal_kv_cfg_t){
        .seed = "vmlkasdh93dlvlcy",
        .key = "dflfuap134ddlduq",
    });
    tal_sw_timer_init();
    tal_workq_init();

#if (!defined(PLATFORM_UBUNTU) || (PLATFORM_UBUNTU == 0)) && (!(defined(ENABLE_AT_MODEM) && (ENABLE_AT_MODEM == 1)))
    tal_cli_init();
    tuya_authorize_init();
    tuya_app_cli_init();
#endif

    app_at_modem_init();

    for (;;) {
        tal_system_sleep(1000);
    }
}

/**
 * @brief main
 *
 * @param argc
 * @param argv
 * @return void
 */
void main(int argc, char *argv[])
{
    user_main();
}