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

#if 0
#if 1
    int fd = tal_at_net_socket_create(PROTOCOL_TCP);
    if (fd < 0) {
        PR_ERR("Failed to create socket, fd: %d", fd);
        return OPRT_COM_ERROR;
    }

    // dns
    char *domain = "h6.iot-dns.com";
    TUYA_IP_ADDR_T dns_addr = 0;
    rt = tal_at_net_gethostbyname(domain, &dns_addr);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to resolve domain %s, error: %d", domain, rt);
        return OPRT_COM_ERROR;
    }
    PR_DEBUG("Resolved domain %s to IP: 0x%08X", domain, dns_addr);

#if 0
    char *ip = "47.103.71.77";
    TUYA_IP_ADDR_T addr = 0;
    addr = tal_at_net_str2addr(ip);
    if (addr < 0) {
        PR_ERR("Failed to convert IP string to address, ip: %s", ip);
        return OPRT_COM_ERROR;
    }
#else
    TUYA_IP_ADDR_T addr = dns_addr;
#endif
    TUYA_ERRNO rt_err = tal_at_net_connect(fd, addr, 443);
    if (rt_err != UNW_SUCCESS) {
        PR_ERR("Failed to connect to server, fd: %d, port: 443, error: %d", fd, rt_err);
        return OPRT_COM_ERROR;
    }
#endif

    char data[] = {0x16, 0x03, 0x03, 0x00, 0x71, 0x01, 0x00, 0x00, 0x6D, 0x03, 0x03, 0xCE, 0xC6, 0x02, 0x61, 0x0B, 0xDE,
                   0x26, 0x1A, 0x59, 0x3B, 0xED, 0x29, 0xF2, 0xDC, 0x9B, 0x5D, 0x18, 0xE1, 0xC4, 0x9E, 0x96, 0xB8, 0x2E,
                   0xFB, 0x97, 0xA7, 0xAA, 0xD9, 0xD6, 0xCE, 0xAF, 0x6B, 0x00, 0x00, 0x08, 0xC0, 0x27, 0xC0, 0x2B, 0xC0,
                   0x2F, 0x00, 0xFF, 0x01, 0x00, 0x00, 0x3C, 0x00, 0x00, 0x00, 0x13, 0x00, 0x11, 0x00, 0x00, 0x0E, 0x68,
                   0x36, 0x2E, 0x69, 0x6F, 0x74, 0x2D, 0x64, 0x6E, 0x73, 0x2E, 0x63, 0x6F, 0x6D, 0x00, 0x0D, 0x00, 0x06,
                   0x00, 0x04, 0x04, 0x03, 0x04, 0x01, 0x00, 0x0A, 0x00, 0x04, 0x00, 0x02, 0x00, 0x17, 0x00, 0x0B, 0x00,
                   0x02, 0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x02, 0x00, 0x16, 0x00, 0x00, 0x00, 0x17, 0x00, 0x00};
    tal_at_net_send(fd, data, sizeof(data));

    char recv_data[5 * 1024] = {0};
    int recv_len = tal_at_net_recv(fd, recv_data, sizeof(recv_data));
    PR_DEBUG("--> Received data length: %d <--", recv_len);
#endif
    return rt;
}
