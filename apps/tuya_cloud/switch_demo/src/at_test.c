/**
 * @file at_test.c
 * @brief at_test module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "at_test.h"
#include "at_client.h"

#include "tal_log.h"
#include "tal_mutex.h"
#include "tal_system.h"
#include "tal_thread.h"

#include "tdd_transport_uart.h"
#include "tdl_transport_manage.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define TRANSPORT_NAME "AT_UART"
/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static THREAD_HANDLE at_transport_thread_hdl = NULL;
static TDL_TRANSPORT_HANDLE g_at_transport_hdl = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/

void at_test_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    TDD_TRANSPORT_UART_CFG_T uart_cfg = {
        .uart_num = TUYA_UART_NUM_2, // Use UART 2 for /dev/ttyUSB0
        .baud_rate = 921600          // Example baud rate
    };

    PR_DEBUG("Initializing AT test with UART_%d at %d baud", uart_cfg.uart_num,
             uart_cfg.baud_rate); // Register UART transport driver
    rt = tdd_transport_uart_register(TRANSPORT_NAME, uart_cfg);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to register UART transport driver: %d", rt);
        if (rt == OPRT_INVALID_PARM) {
            PR_ERR("This could be due to UART_%d already being in use", uart_cfg.uart_num);
        }
        return;
    }
    PR_DEBUG("UART transport driver registered successfully");

    rt = at_client_init(TRANSPORT_NAME);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to initialize AT client: %d", rt);
        return;
    }

    // Initialize AT test module
    PR_DEBUG("AT test module initialized successfully");
}