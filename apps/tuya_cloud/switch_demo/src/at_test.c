/**
 * @file at_test.c
 * @brief at_test module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "at_test.h"
#include "at_client.h"
#include "at_parser.h"

#include "tal_log.h"
#include "tal_mutex.h"
#include "tal_system.h"
#include "tal_thread.h"

#include "tdd_transport_uart.h"
#include "tdl_transport_manage.h"

#include "tkl_network.h"

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
// static THREAD_HANDLE at_transport_thread_hdl = NULL;
// static TDL_TRANSPORT_HANDLE g_at_transport_hdl = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/

void at_test_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    TDD_TRANSPORT_UART_CFG_T uart_cfg = {
        .uart_num = TUYA_UART_NUM_0, // Use UART 2 for /dev/ttyUSB0
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
#if 0
    char *send_cmd = "AT\r";
    rt = at_client_send(send_cmd, strlen(send_cmd), 1000, NULL);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to send AT command: %d", rt);
        return;
    }

    // AT+MDNSGIP="h6.iot-dns.com"
    send_cmd = "AT+MDNSGIP=\"h6.iot-dns.com\"\r";
    rt = at_client_send(send_cmd, strlen(send_cmd), 1000, NULL); // Send a DNS query command
    if (rt != OPRT_OK) {
        PR_ERR("Failed to send DNS query command: %d", rt);
        return;
    }

    //  AT+MIPOPEN=0,"TCP","47.103.71.77",443
    send_cmd = "AT+MIPOPEN=0,\"TCP\",\"47.103.71.77\",443\r";
    rt = at_client_send(send_cmd, strlen(send_cmd), 1000, NULL);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to open TCP connection: %d", rt);
        return;
    }

    //  AT+MIPCFG="encoding",0,1,1
    send_cmd = "AT+MIPCFG=\"encoding\",0,1,1\r";
    rt = at_client_send(send_cmd, strlen(send_cmd), 1000, NULL);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to configure encoding: %d", rt);
        return;
    }

    //  AT+MIPSEND=0,118,"16030300710100006D0303CEC602610BDE261A593BED29F2DC9B5D18E1C49E96B82EFB97A7AAD9D6CEAF6B000008C027C02BC02F00FF0100003C00000013001100000E68362E696F742D646E732E636F6D000D0006000404030401000A000400020017000B0002010000010001020016000000170000"
    send_cmd = "AT+MIPSEND=0,118,"
               "\"16030300710100006D0303CEC602610BDE261A593BED29F2DC9B5D18E1C49E96B82EFB97A7AAD9D6CEAF6B000008C027C02BC"
               "02F00FF0100003C00000013001100000E68362E696F742D646E732E636F6D000D0006000404030401000A000400020017000B00"
               "02010000010001020016000000170000\"\r";
    rt = at_client_send(send_cmd, strlen(send_cmd), 1000, NULL);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to send data: %d", rt);
        return;
    }
#else
    char *send_cmd = "AT\r";
    AT_LINE_T *line = NULL;
    uint32_t line_num = 0;
    rt = at_client_send(send_cmd, strlen(send_cmd), 1000, &line, &line_num);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to send AT command: %d", rt);
        return;
    }
    PR_DEBUG("Received %d lines after sending command: %s", line_num, send_cmd);
    AT_LINE_T *current = line;
    for (uint32_t i = 0; i < line_num; i++) {
        PR_DEBUG("Received line %d: %.*s", i, current->length, current->data);
        current = current->next;
    }
    AT_LINE_T *tmp_line = line;
    do {
        AT_LINE_T *next_line = tmp_line->next;
        at_parser_free_line(tmp_line);
        tmp_line = next_line;
    } while (tmp_line != NULL);

    // AT+MDNSGIP="h6.iot-dns.com"

    // rt = at_parser_response_pattern_regist(sg_at_client.parser_hdl, &dns_pattern);

#if 0 // tkl network test

#if 0
    send_cmd = "AT+MDNSGIP=\"h6.iot-dns.com\"\r";
    rt = at_client_send(send_cmd, strlen(send_cmd), 1000, &line, &line_num);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to send DNS query command: %d", rt);
        return;
    }

    PR_DEBUG("Received %d lines after sending command: %s", line_num, send_cmd);
    current = line;
    for (uint32_t i = 0; i < line_num; i++) {
        PR_DEBUG("Received line %d: %.*s", i, current->length, current->data);
        current = current->next;
    }
    at_client_free_lines(line);
    at_client_get_one_line(&line);
    at_client_free_lines(line);

    //  AT+MIPOPEN=0,"TCP","47.103.71.77",443
    send_cmd = "AT+MIPOPEN=0,\"TCP\",\"47.103.71.77\",443\r";
    rt = at_client_send(send_cmd, strlen(send_cmd), 1000, &line, &line_num);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to open TCP connection: %d", rt);
        return;
    }
    PR_DEBUG("Received %d lines after sending command: %s", line_num, send_cmd);
    current = line;
    for (uint32_t i = 0; i < line_num; i++) {
        PR_DEBUG("Received line %d: %.*s", i, current->length, current->data);
        current = current->next;
    }
    at_client_free_lines(line);
    at_client_get_one_line(&line);
    at_client_free_lines(line);

    //  AT+MIPCFG="encoding",0,1,1
    send_cmd = "AT+MIPCFG=\"encoding\",0,1,1\r";
    rt = at_client_send(send_cmd, strlen(send_cmd), 1000, &line, &line_num);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to configure encoding: %d", rt);
        return;
    }
    PR_DEBUG("Received %d lines after sending command: %s", line_num, send_cmd);
    current = line;
    for (uint32_t i = 0; i < line_num; i++) {
        PR_DEBUG("Received line %d: %.*s", i, current->length, current->data);
        current = current->next;
    }
    at_client_free_lines(line);

    //  AT+MIPSEND=0,118,"16030300710100006D0303CEC602610BDE261A593BED29F2DC9B5D18E1C49E96B82EFB97A7AAD9D6CEAF6B000008C027C02BC02F00FF0100003C00000013001100000E68362E696F742D646E732E636F6D000D0006000404030401000A000400020017000B0002010000010001020016000000170000"
    send_cmd = "AT+MIPSEND=0,118,"
               "\"16030300710100006D0303CEC602610BDE261A593BED29F2DC9B5D18E1C49E96B82EFB97A7AAD9D6CEAF6B000008C027C02BC"
               "02F00FF0100003C00000013001100000E68362E696F742D646E732E636F6D000D0006000404030401000A000400020017000B00"
               "02010000010001020016000000170000\"\r";
    rt = at_client_send(send_cmd, strlen(send_cmd), 1000, &line, &line_num);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to send data: %d", rt);
        return;
    }
    PR_DEBUG("Received %d lines after sending command: %s", line_num, send_cmd);
    current = line;
    for (uint32_t i = 0; i < line_num; i++) {
        PR_DEBUG("Received line %d: %.*s", i, current->length, current->data);
        current = current->next;
    }
    at_client_free_lines(line);
#else
    int fd = tkl_net_socket_create(PROTOCOL_TCP);
    if (fd < 0) {
        PR_ERR("Failed to create socket: %d", fd);
        return;
    }

#if 1
    TUYA_IP_ADDR_T dns_ip = 0;
    tkl_net_gethostbyname("h6.iot-dns.com", &dns_ip);
    tkl_net_connect(fd, dns_ip, 443);
#else
    TUYA_IP_ADDR_T ip_addr = tkl_net_str2addr("47.103.71.77");
    tkl_net_connect(fd, ip_addr, 443);
#endif
    if (rt != OPRT_OK) {
        PR_ERR("Failed to connect to server: %d", rt);
        return;
    }
    PR_DEBUG("Connected to server successfully");
    // Send data
    char data[] = {0x16, 0x03, 0x03, 0x00, 0x71, 0x01, 0x00, 0x00, 0x6D, 0x03, 0x03, 0xCE, 0xC6, 0x02, 0x61, 0x0B, 0xDE,
                   0x26, 0x1A, 0x59, 0x3B, 0xED, 0x29, 0xF2, 0xDC, 0x9B, 0x5D, 0x18, 0xE1, 0xC4, 0x9E, 0x96, 0xB8, 0x2E,
                   0xFB, 0x97, 0xA7, 0xAA, 0xD9, 0xD6, 0xCE, 0xAF, 0x6B, 0x00, 0x00, 0x08, 0xC0, 0x27, 0xC0, 0x2B, 0xC0,
                   0x2F, 0x00, 0xFF, 0x01, 0x00, 0x00, 0x3C, 0x00, 0x00, 0x00, 0x13, 0x00, 0x11, 0x00, 0x00, 0x0E, 0x68,
                   0x36, 0x2E, 0x69, 0x6F, 0x74, 0x2D, 0x64, 0x6E, 0x73, 0x2E, 0x63, 0x6F, 0x6D, 0x00, 0x0D, 0x00, 0x06,
                   0x00, 0x04, 0x04, 0x03, 0x04, 0x01, 0x00, 0x0A, 0x00, 0x04, 0x00, 0x02, 0x00, 0x17, 0x00, 0x0B, 0x00,
                   0x02, 0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x02, 0x00, 0x16, 0x00, 0x00, 0x00, 0x17, 0x00, 0x00};
    tkl_net_send(fd, data, sizeof(data));
#endif

#endif
#endif // tkl network test
    // Initialize AT test module
    PR_DEBUG("AT test module initialized successfully");
}
