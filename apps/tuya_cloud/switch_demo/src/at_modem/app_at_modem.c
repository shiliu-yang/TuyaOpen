/**
 * @file app_at_modem.c
 * @brief app_at_modem module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "app_at_modem.h"

#include "tdd_transport_uart.h"
#include "tdl_transport_manage.h"

#include "tal_at_modem.h"

#include "tal_api.h"

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

/***********************************************************
***********************function define**********************
***********************************************************/

OPERATE_RET app_at_modem_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    TDD_TRANSPORT_UART_CFG_T uart_cfg = {.port_id = TUYA_UART_NUM_2,
                                         .cfg.rx_buffer_size = 10 * 1024,
                                         .cfg.open_mode = O_BLOCK,
                                         .cfg.base_cfg = {
                                             .baudrate = 921600,
                                             .databits = TUYA_UART_DATA_LEN_8BIT,
                                             .parity = TUYA_UART_PARITY_TYPE_NONE,
                                             .stopbits = TUYA_UART_STOP_LEN_1BIT,
                                         }};
    TUYA_CALL_ERR_RETURN(tdd_transport_uart_register(TRANSPORT_NAME, uart_cfg));

    // Create ML307R Client
    tal_at_modem_init(TRANSPORT_NAME, TAL_AT_MODEM_TYPE_ML307R);

    return rt;
}
