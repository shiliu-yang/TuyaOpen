/**
 * @file app_at_modem.c
 * @brief app_at_modem module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"

#include "tdd_transport_uart.h"
#include "tdl_transport_manage.h"

/***********************************************************
************************macro define************************
***********************************************************/
#ifndef AT_TRANSPORT_NAME
#define AT_TRANSPORT_NAME "at_modem"
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static TDL_TRANSPORT_HANDLE sg_ts_hdl = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/

void app_at_modem_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    TDD_TRANSPORT_UART_CFG_T uart_cfg = {
        .port_id = TUYA_UART_NUM_2,
        .cfg.rx_buffer_size = 10 * 1024,
        .cfg.open_mode = O_BLOCK,
        .cfg.base_cfg =
            {
                .baudrate = 921600,
                .databits = TUYA_UART_DATA_LEN_8BIT,
                .parity = TUYA_UART_PARITY_TYPE_NONE,
                .stopbits = TUYA_UART_STOP_LEN_1BIT,
            },
    };
    TUYA_CALL_ERR_LOG(tdd_transport_uart_register(AT_TRANSPORT_NAME, uart_cfg));

    TUYA_CALL_ERR_LOG(tdl_transport_find(AT_TRANSPORT_NAME, &sg_ts_hdl));

    TUYA_CALL_ERR_LOG(tdl_transport_open(sg_ts_hdl));

    for (;;) {
        rt = tdl_transport_send(sg_ts_hdl, (uint8_t *)"AT\r", 4);

        PR_DEBUG("ts write: AT, %d", rt);

        uint8_t recv_data[125] = {0};
        uint32_t recv_len = tdl_transport_read(sg_ts_hdl, recv_data, sizeof(recv_data) - 1);

        // PR_HEXDUMP_DEBUG("ts read", recv_data, recv_len);

        tal_system_sleep(3 * 1000);
    }
}