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
    tmp_line = line;
    do {
        AT_LINE_T *next_line = tmp_line->next;
        at_parser_free_line(tmp_line);
        tmp_line = next_line;
    } while (tmp_line != NULL);
    at_client_get_one_line(&line);
    tmp_line = line;
    do {
        AT_LINE_T *next_line = tmp_line->next;
        at_parser_free_line(tmp_line);
        tmp_line = next_line;
    } while (tmp_line != NULL);

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
    tmp_line = line;
    do {
        AT_LINE_T *next_line = tmp_line->next;
        at_parser_free_line(tmp_line);
        tmp_line = next_line;
    } while (tmp_line != NULL);
    at_client_get_one_line(&line);
    tmp_line = line;
    do {
        AT_LINE_T *next_line = tmp_line->next;
        at_parser_free_line(tmp_line);
        tmp_line = next_line;
    } while (tmp_line != NULL);

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

#endif
    // Initialize AT test module
    PR_DEBUG("AT test module initialized successfully");
}