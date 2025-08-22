/**
 * @file tal_at_modem.c
 * @brief tal_at_modem module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tal_at_modem.h"

#include "tal_api.h"

#include "tdl_transport_manage.h"

#include "at_utils.h"
#include "at_client.h"

#include "at_module_ml307r.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    int is_used; // Indicates if the socket is in use

    // Socket information
    uint16_t port;             // Port number of the socket
    TUYA_PROTOCOL_TYPE_E type; // Protocol type (TCP/UDP)
    BOOL_T is_block;           // Indicates if the socket is blocking
    BOOL_T is_connected;       // Indicates if the socket is connected

    // Recv information

    // Peer information
    uint16_t peer_port;     // Port number of the peer
    char peer_ip_str[16];   // IP address of the socket
    TUYA_IP_ADDR_T peer_ip; // IP address structure for the peer

} AT_SOCKET_T;

typedef struct {
    AT_MODEM_EVENT_CB event_cb;
    TAL_AT_MODEM_TYPE_T type;

    TDL_TRANSPORT_HANDLE transport_hdl;

    AT_MODULE_OPS_T ops;
} TAL_AT_MODEM_CFG_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static TAL_AT_MODEM_CFG_T sg_at_modem = {0};

/***********************************************************
***********************function define**********************
***********************************************************/
/* at modem start */
static void __at_module_urc_cb(TAL_AT_MODULE_CMD_T cmd, void *args)
{
    return;
}

OPERATE_RET tal_at_modem_init(const char *transport_name, TAL_AT_MODEM_TYPE_T type, AT_MODEM_EVENT_CB event_cb)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(transport_name, OPRT_INVALID_PARM);

    sg_at_modem.event_cb = event_cb;

    TUYA_CALL_ERR_GOTO(tdl_transport_find(transport_name, &sg_at_modem.transport_hdl), __EXIT);
    TUYA_CALL_ERR_GOTO(tdl_transport_open(sg_at_modem.transport_hdl), __EXIT);

    TUYA_CALL_ERR_GOTO(at_client_init(sg_at_modem.transport_hdl), __EXIT);

    // init 4G module
    if (type == TAL_AT_MODEM_TYPE_ML307R) {
        // Initialize ML307R specific settings
        TUYA_CALL_ERR_GOTO(at_module_ml307r_init(&sg_at_modem.ops, __at_module_urc_cb), __EXIT);
    }

__EXIT:
    return rt;
}

OPERATE_RET tal_at_modem_deinit(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_LOG(tdl_transport_close(sg_at_modem.transport_hdl));
    TUYA_CALL_ERR_LOG(at_client_deinit());
    TUYA_CALL_ERR_LOG(at_module_ml307r_deinit());

    return rt;
}
/* at modem end */

/* at modem tal network start */
TUYA_ERRNO tal_at_net_get_errno(void)
{
    AT_MODULE_OPS_T *ops = &sg_at_modem.ops;

    if (ops->at_get_errno) {
        return ops->at_get_errno();
    }

    return UNW_FAIL;
}

OPERATE_RET tal_at_net_fd_set(int fd, TUYA_FD_SET_T *fds)
{
    OPERATE_RET rt = OPRT_OK;
    return rt;
}

OPERATE_RET tal_at_net_fd_clear(int fd, TUYA_FD_SET_T *fds)
{
    OPERATE_RET rt = OPRT_OK;
    return rt;
}

OPERATE_RET tal_at_net_fd_isset(int fd, TUYA_FD_SET_T *fds)
{
    OPERATE_RET rt = OPRT_OK;
    return rt;
}

OPERATE_RET tal_at_net_fd_zero(TUYA_FD_SET_T *fds)
{
    OPERATE_RET rt = OPRT_OK;
    return rt;
}

int tal_at_net_select(const int maxfd, TUYA_FD_SET_T *readfds, TUYA_FD_SET_T *writefds, TUYA_FD_SET_T *errorfds,
                      const uint32_t ms_timeout)
{
    OPERATE_RET rt = OPRT_OK;
    return rt;
}

int tal_at_net_get_nonblock(const int fd)
{
    OPERATE_RET rt = OPRT_OK;
    return rt;
}

OPERATE_RET tal_at_net_set_block(const int fd, const BOOL_T block)
{
    OPERATE_RET rt = OPRT_OK;
    return rt;
}

TUYA_ERRNO tal_at_net_close(const int fd)
{
    OPERATE_RET rt = OPRT_OK;
    return rt;
}

int tal_at_net_socket_create(const TUYA_PROTOCOL_TYPE_E type)
{
    OPERATE_RET rt = OPRT_OK;
    return rt;
}

TUYA_ERRNO tal_at_net_connect(const int fd, const TUYA_IP_ADDR_T addr, const uint16_t port)
{
    OPERATE_RET rt = OPRT_OK;
    return rt;
}

TUYA_ERRNO tal_at_net_connect_raw(const int fd, void *p_socket, const int len)
{
    OPERATE_RET rt = OPRT_OK;
    return rt;
}

TUYA_ERRNO tal_at_net_bind(const int fd, const TUYA_IP_ADDR_T addr, const uint16_t port)
{
    OPERATE_RET rt = OPRT_OK;
    return rt;
}

TUYA_ERRNO tal_at_net_listen(const int fd, const int backlog)
{
    OPERATE_RET rt = OPRT_OK;
    return rt;
}

TUYA_ERRNO tal_at_net_send(const int fd, const void *buf, const uint32_t nbytes)
{
    OPERATE_RET rt = OPRT_OK;
    return rt;
}

TUYA_ERRNO tal_at_net_send_to(const int fd, const void *buf, const uint32_t nbytes, const TUYA_IP_ADDR_T addr,
                              const uint16_t port)
{
    OPERATE_RET rt = OPRT_OK;
    return rt;
}

int tal_at_net_accept(const int fd, TUYA_IP_ADDR_T *addr, uint16_t *port)
{
    OPERATE_RET rt = OPRT_OK;
    return rt;
}

TUYA_ERRNO tal_at_net_recv(const int fd, void *buf, const uint32_t nbytes)
{
    OPERATE_RET rt = OPRT_OK;
    return rt;
}

int tal_at_net_recv_nd_size(const int fd, void *buf, const uint32_t buf_size, const uint32_t nd_size)
{
    OPERATE_RET rt = OPRT_OK;
    return rt;
}

TUYA_ERRNO tal_at_net_recvfrom(const int fd, void *buf, const uint32_t nbytes, TUYA_IP_ADDR_T *addr, uint16_t *port)
{
    OPERATE_RET rt = OPRT_OK;
    return rt;
}

OPERATE_RET tal_at_net_set_timeout(const int fd, const int ms_timeout, const TUYA_TRANS_TYPE_E type)
{
    OPERATE_RET rt = OPRT_OK;
    return rt;
}

OPERATE_RET tal_at_net_set_bufsize(const int fd, const int buf_size, const TUYA_TRANS_TYPE_E type)
{
    OPERATE_RET rt = OPRT_OK;
    return rt;
}

OPERATE_RET tal_at_net_set_reuse(const int fd)
{
    OPERATE_RET rt = OPRT_OK;
    return rt;
}

OPERATE_RET tal_at_net_disable_nagle(const int fd)
{
    OPERATE_RET rt = OPRT_OK;
    return rt;
}

OPERATE_RET tal_at_net_set_broadcast(const int fd)
{
    OPERATE_RET rt = OPRT_OK;
    return rt;
}

OPERATE_RET tal_at_net_gethostbyname(const char *domain, TUYA_IP_ADDR_T *addr)
{
    OPERATE_RET rt = OPRT_OK;
    return rt;
}

OPERATE_RET tal_at_net_set_keepalive(int fd, const BOOL_T alive, const uint32_t idle, const uint32_t intr,
                                     const uint32_t cnt)
{
    OPERATE_RET rt = OPRT_OK;
    return rt;
}

OPERATE_RET tal_at_net_get_socket_ip(int fd, TUYA_IP_ADDR_T *addr)
{
    OPERATE_RET rt = OPRT_OK;
    return rt;
}

TUYA_IP_ADDR_T tal_at_net_str2addr(const char *ip_str)
{
    OPERATE_RET rt = OPRT_OK;
    return rt;
}

char *tal_at_net_addr2str(TUYA_IP_ADDR_T ipaddr)
{
    OPERATE_RET rt = OPRT_OK;
    return rt;
}

OPERATE_RET tal_at_net_setsockopt(const int fd, const TUYA_OPT_LEVEL level, const TUYA_OPT_NAME optname,
                                  const void *optval, const int optlen)
{
    OPERATE_RET rt = OPRT_OK;
    return rt;
}

OPERATE_RET tal_at_net_getsockopt(const int fd, const TUYA_OPT_LEVEL level, const TUYA_OPT_NAME optname, void *optval,
                                  int *optlen)
{
    OPERATE_RET rt = OPRT_OK;
    return rt;
}
/* at modem tal network end */
