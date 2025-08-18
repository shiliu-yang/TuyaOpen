/**
 * @file at_module.h
 * @brief at_module module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __AT_MODULE_H__
#define __AT_MODULE_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef uint8_t TAL_AT_MODULE_CMD_T;
#define TAL_AT_MODULE_CMD_READY          (0x00)
#define TAL_AT_MODULE_CMD_SIM            (0x01)
#define TAL_AT_MODULE_CMD_NETWORK        (0x02)
#define TAL_AT_MODULE_CMD_CONNECT_STATUS (0x03)
#define TAL_AT_MODULE_CMD_SOCKET_RECV    (0x04)

typedef void (*AT_MODULE_CB)(TAL_AT_MODULE_CMD_T cmd, void *args);

typedef struct {
    TUYA_ERRNO (*at_get_errno)(void);
    TUYA_ERRNO (*at_net_close)(const int fd);
    int (*at_net_socket_create)(const TUYA_PROTOCOL_TYPE_E type);
    TUYA_ERRNO (*at_net_connect)(const int fd, const TUYA_IP_ADDR_T addr, const uint16_t port);
    TUYA_ERRNO (*at_net_connect_raw)(const int fd, void *p_socket, const int len);
    TUYA_ERRNO (*at_net_bind)(const int fd, const TUYA_IP_ADDR_T addr, const uint16_t port);
    TUYA_ERRNO (*at_net_listen)(const int fd, const int backlog);
    TUYA_ERRNO (*at_net_send)(const int fd, const void *buf, const uint32_t nbytes);
    TUYA_ERRNO (*at_net_send_to)(const int fd, const void *buf, const uint32_t nbytes, const TUYA_IP_ADDR_T addr,
                                 const uint16_t port);
    int (*at_net_accept)(const int fd, TUYA_IP_ADDR_T *addr, uint16_t *port);
    OPERATE_RET (*at_net_set_bufsize)(const int fd, const int buf_size, const TUYA_TRANS_TYPE_E type);
    OPERATE_RET (*at_net_set_reuse)(const int fd);
    OPERATE_RET (*at_net_gethostbyname)(const char *domain, TUYA_IP_ADDR_T *addr);
    OPERATE_RET (*at_net_set_keepalive)(int fd, const BOOL_T alive, const uint32_t idle, const uint32_t intr,
                                        const uint32_t cnt);
} AT_MODULE_OPS_T;

/***********************************************************
********************function declaration********************
***********************************************************/

#ifdef __cplusplus
}
#endif

#endif /* __AT_MODULE_H__ */
