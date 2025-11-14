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
#define TAL_AT_MALLOC tal_psram_malloc
#define TAL_AT_FREE   tal_psram_free

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    int is_used; // Indicates if the socket is in use

    MUTEX_HANDLE mutex; // Mutex for thread safety

    // Socket information
    uint16_t port;             // Port number of the socket
    TUYA_PROTOCOL_TYPE_E type; // Protocol type (TCP/UDP)
    BOOL_T is_block;           // Indicates if the socket is blocking
    BOOL_T is_connected;       // Indicates if the socket is connected

    // Peer information
    uint16_t peer_port;     // Port number of the peer
    char peer_ip_str[16];   // IP address of the socket
    TUYA_IP_ADDR_T peer_ip; // IP address structure for the peer

    // timeout
    int send_timeout_ms; // Send timeout in milliseconds
    int recv_timeout_ms; // Receive timeout in milliseconds
} AT_SOCKET_T;

typedef struct {
    AT_MODEM_EVENT_CB event_cb;
    TAL_AT_MODEM_TYPE_T type;

    TDL_TRANSPORT_HANDLE transport_hdl;

    // socket
    int socket_num_max;
    AT_SOCKET_T *socket;

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
// -------------------------------
// at modem tools start
// -------------------------------
static int __find_unused_socket(AT_SOCKET_T *socket, uint8_t socket_num)
{
    if (NULL == socket || 0 == socket_num) {
        return -1;
    }

    for (uint8_t i = 0; i < socket_num; i++) {
        tal_mutex_lock(socket[i].mutex);
        if (0 == socket[i].is_used) {
            tal_mutex_unlock(socket[i].mutex);
            return i;
        }
        tal_mutex_unlock(socket[i].mutex);
    }

    return -1; // No free socket found
}

static void __at_socket_ctx_destroy(AT_SOCKET_T *socket, uint8_t socket_num)
{

    if (NULL == socket) {
        return;
    }

    for (uint8_t i = 0; i < socket_num; i++) {
        if (NULL != socket[i].mutex) {
            tal_mutex_release(socket[i].mutex);
            socket[i].mutex = NULL;
        }
    }

    TAL_AT_FREE(socket);

    return;
}

static AT_SOCKET_T *__at_socket_ctx_create(uint8_t socket_num)
{
    OPERATE_RET rt = OPRT_OK;

    AT_SOCKET_T *p_socket = NULL;

    if (0 == socket_num) {
        PR_ERR("Socket number is 0");
        return NULL;
    }

    p_socket = TAL_AT_MALLOC(sizeof(AT_SOCKET_T) * socket_num);
    if (NULL == p_socket) {
        return NULL;
    }
    memset(p_socket, 0, sizeof(AT_SOCKET_T) * socket_num);

    for (int i = 0; i < socket_num; i++) {
        p_socket[i].send_timeout_ms = 5000; // default 5s
        p_socket[i].recv_timeout_ms = 5000; // default 5s
        TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&p_socket[i].mutex), __EXIT);
    }

__EXIT:
    if (OPRT_OK != rt) {
        __at_socket_ctx_destroy(p_socket, socket_num);
        p_socket = NULL;
    }

    return p_socket;
}

static void __at_modem_free_socket(AT_SOCKET_T *socket)
{
    if (NULL == socket) {
        return;
    }

    // Socket information
    socket->port = 0;
    socket->is_block = 1; // default block
    socket->is_connected = 0;

    // Peer information
    socket->peer_port = 0;
    memset(socket->peer_ip_str, 0, sizeof(socket->peer_ip_str));
    socket->peer_ip = 0;

    // timeout
    socket->send_timeout_ms = 5 * 1000; // default 5s
    socket->recv_timeout_ms = 5 * 1000; // default 5s

    socket->is_used = 0;

    return;
}

// -------------------------------
// at modem tools end
// -------------------------------

/* at modem start */
static void __at_module_urc_cb(TAL_AT_MODULE_CMD_T cmd, void *args)
{
    PR_DEBUG("AT module URC: %s", AT_MODEL_CMD_TO_STR(cmd));

    switch (cmd) {
    case TAL_AT_MODULE_CMD_NETWORK: {
        if (!args) {
            PR_ERR("Invalid args for NETWORK");
            break;
        }
        AT_NETWORK_STATUS_T *network_status = (AT_NETWORK_STATUS_T *)args;

        sg_at_modem.event_cb(network_status->status == 1 ? AT_CONNECTED : AT_DISCONNECTED, NULL);

        PR_DEBUG("NETWORK: status=%d", network_status->status);
    } break;
    case TAL_AT_MODULE_CMD_SOCKET_CONNECT_STATUS: {
        if (!args) {
            PR_ERR("Invalid args for CONNECT_STATUS");
            break;
        }
        AT_SOCKET_CONNECT_STATUS_T *cnt_status = (AT_SOCKET_CONNECT_STATUS_T *)args;
        PR_DEBUG("CONNECT_STATUS: fd=%d, status=%d", cnt_status->fd, cnt_status->status);

        if (sg_at_modem.socket[cnt_status->fd].mutex == NULL) {
            PR_ERR("Socket %d mutex is NULL", cnt_status->fd);
            break;
        }
        tal_mutex_lock(sg_at_modem.socket[cnt_status->fd].mutex);
        sg_at_modem.socket[cnt_status->fd].is_connected = cnt_status->status;
        // disconnect, clear recv buffer, free recv buffer
        if (cnt_status->status == 0) {
            __at_modem_free_socket(&sg_at_modem.socket[cnt_status->fd]);
        }
        tal_mutex_unlock(sg_at_modem.socket[cnt_status->fd].mutex);
    } break;
    default: {
        PR_ERR("Unknown URC cmd: %d", cmd);
    } break;
    }

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
        sg_at_modem.type = type;
        TUYA_CALL_ERR_GOTO(at_module_ml307r_init(&sg_at_modem.ops, __at_module_urc_cb), __EXIT);
    }

    // get max socket number
    if (sg_at_modem.ops.at_net_get_socket_max_num) {
        sg_at_modem.socket_num_max = sg_at_modem.ops.at_net_get_socket_max_num();
    }
    sg_at_modem.socket = __at_socket_ctx_create(sg_at_modem.socket_num_max);
    if (NULL == sg_at_modem.socket) {
        PR_ERR("malloc for at socket fail");
        rt = OPRT_MALLOC_FAILED;
        goto __EXIT;
    }

__EXIT:
    if (OPRT_OK != rt) {
        tal_at_modem_deinit();
    }

    return rt;
}

OPERATE_RET tal_at_modem_deinit(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL != sg_at_modem.transport_hdl) {
        TUYA_CALL_ERR_LOG(tdl_transport_close(sg_at_modem.transport_hdl));
    }
    TUYA_CALL_ERR_LOG(at_client_deinit());
    TUYA_CALL_ERR_LOG(at_module_ml307r_deinit());

    if (NULL != sg_at_modem.socket) {
        __at_socket_ctx_destroy(sg_at_modem.socket, sg_at_modem.socket_num_max);
        sg_at_modem.socket = NULL;
        sg_at_modem.socket_num_max = 0;
    }

    return rt;
}
/* at modem end */

/* at modem tal network start */
TUYA_ERRNO tal_at_net_get_errno(void)
{
    AT_MODULE_OPS_T *ops = &sg_at_modem.ops;

    if (ops->at_net_get_errno) {
        return ops->at_net_get_errno();
    }

    return UNW_FAIL;
}

OPERATE_RET tal_at_net_fd_set(int fd, TUYA_FD_SET_T *fds)
{
    if (fd < 0 || fd >= sg_at_modem.socket_num_max || fds == NULL) {
        return OPRT_INVALID_PARM;
    }

    fds->placeholder[fd / 8] |= (1 << (fd % 8)); // Set the bit for the fd

    return OPRT_OK;
}

OPERATE_RET tal_at_net_fd_clear(int fd, TUYA_FD_SET_T *fds)
{
    PR_ERR("[%s] not supported", __func__);
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET tal_at_net_fd_isset(int fd, TUYA_FD_SET_T *fds)
{
    if (fd < 0 || fd >= sg_at_modem.socket_num_max || fds == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (fds->placeholder[fd / 8] & (1 << (fd % 8))) {
        return OPRT_OK; // fd is set
    } else {
        return OPRT_COM_ERROR; // fd is not set
    }
    return OPRT_OK;
}

OPERATE_RET tal_at_net_fd_zero(TUYA_FD_SET_T *fds)
{
    if (fds == NULL) {
        return OPRT_INVALID_PARM;
    }

    memset(fds->placeholder, 0, sizeof(fds->placeholder));
    return OPRT_OK;
}

int tal_at_net_select(const int maxfd, TUYA_FD_SET_T *readfds, TUYA_FD_SET_T *writefds, TUYA_FD_SET_T *errorfds,
                      const uint32_t ms_timeout)
{
    // TODO: Implement select for AT modem
    return 1;
}

int tal_at_net_get_nonblock(const int fd)
{
    if (fd < 0 || fd >= sg_at_modem.socket_num_max) {
        return -3000 + fd;
    }

    AT_SOCKET_T *socket = &sg_at_modem.socket[fd];
    if (socket->is_used == 0) {
        PR_ERR("socket %d not used", fd);
        return -3000 + fd;
    }

    return socket->is_block ? 0 : 1;
}

OPERATE_RET tal_at_net_set_block(const int fd, const BOOL_T block)
{
    if (fd < 0 || fd >= sg_at_modem.socket_num_max) {
        return OPRT_INVALID_PARM;
    }

    AT_SOCKET_T *socket = &sg_at_modem.socket[fd];
    if (socket->is_used == 0) {
        PR_ERR("socket %d not used", fd);
        return OPRT_INVALID_PARM;
    }

    socket->is_block = (block == TRUE) ? 1 : 0;

    return OPRT_OK;
}

TUYA_ERRNO tal_at_net_close(const int fd)
{
    TUYA_ERRNO rt_errno = UNW_SUCCESS;
    // OPERATE_RET rt = OPRT_OK;
    AT_SOCKET_T *socket = NULL;
    AT_MODULE_OPS_T *ops = &sg_at_modem.ops;

    PR_DEBUG("Close socket fd: %d", fd);

    if (fd < 0 || fd >= sg_at_modem.socket_num_max) {
        PR_ERR("Invalid socket fd: %d, max %d", fd, sg_at_modem.socket_num_max);
        return UNW_FAIL;
    }

    if (NULL == ops->at_net_close) {
        PR_ERR("at_net_close is NULL");
        return UNW_FAIL;
    }

    socket = &sg_at_modem.socket[fd];

    if (socket->is_connected == 0 || socket->is_used == 0) {
        PR_ERR("Socket %d is not connected", fd);
        return UNW_SUCCESS;
    }

    tal_mutex_lock(socket->mutex);

    rt_errno = ops->at_net_close(fd);
    if (rt_errno != UNW_SUCCESS) {
        PR_ERR("at_net_close failed: %d", rt_errno);
        goto __EXIT;
    }

    socket->is_connected = 0;
    socket->is_used = 0;

__EXIT:
    tal_mutex_unlock(socket->mutex);

    return rt_errno;
}

int tal_at_net_socket_create(const TUYA_PROTOCOL_TYPE_E type)
{
    // OPERATE_RET rt = OPRT_OK;
    int fd = -1;
    AT_SOCKET_T *socket = NULL;

    if (type == PROTOCOL_RAW) {
        return -1;
    }

    fd = __find_unused_socket(sg_at_modem.socket, sg_at_modem.socket_num_max);
    if (fd < 0) {
        return -1;
    }
    socket = &sg_at_modem.socket[fd];

    tal_mutex_lock(socket->mutex);

    socket->is_used = 1;
    socket->is_block = 1;
    socket->is_connected = 0;

    socket->port = 0;
    socket->peer_port = 0;
    memset(socket->peer_ip_str, 0, sizeof(socket->peer_ip_str));
    socket->peer_ip = 0;

    socket->type = type;

    tal_mutex_unlock(socket->mutex);

    PR_DEBUG("Create socket fd: %d, type: %d", fd, type);

    return fd;
}

TUYA_ERRNO tal_at_net_connect(const int fd, const TUYA_IP_ADDR_T addr, const uint16_t port)
{
    TUYA_ERRNO rt_errno = UNW_SUCCESS;
    // OPERATE_RET rt = OPRT_OK;

    PR_DEBUG("tal_at_net_connect: fd=%d, addr=%u, port=%d", fd, addr, port);

    if (fd < 0 || fd >= sg_at_modem.socket_num_max) {
        PR_ERR("Invalid socket fd: %d", fd);
        return UNW_FAIL;
    }

    AT_MODULE_OPS_T *ops = &sg_at_modem.ops;

    if (NULL == ops->at_net_connect) {
        PR_ERR("at_net_connect is NULL");
        return UNW_FAIL;
    }

    AT_SOCKET_T *socket = &sg_at_modem.socket[fd];

    if (0 == socket->is_used) {
        return UNW_FAIL;
    }

    tal_mutex_lock(socket->mutex);

    socket->peer_ip = addr;
    socket->peer_port = port;

    memset(socket->peer_ip_str, 0, sizeof(socket->peer_ip_str));
    char *p_ip_str = tal_at_net_addr2str(addr);
    strncpy(socket->peer_ip_str, p_ip_str, sizeof(socket->peer_ip_str) - 1);

    rt_errno = ops->at_net_connect(fd, socket->type, addr, socket->peer_ip_str, port);
    if (rt_errno != UNW_SUCCESS) {
        PR_ERR("at_net_connect failed: %d", rt_errno);
        socket->is_connected = 0;
        // goto __EXIT;
    } else {
        socket->is_connected = 1;
    }

    // __EXIT:
    tal_mutex_unlock(socket->mutex);

    return rt_errno;
}

TUYA_ERRNO tal_at_net_connect_raw(const int fd, void *p_socket, const int len)
{
    PR_ERR("[%s] not supported", __func__);
    return OPRT_NOT_SUPPORTED;
}

TUYA_ERRNO tal_at_net_bind(const int fd, const TUYA_IP_ADDR_T addr, const uint16_t port)
{
    // TODO: Implement bind if needed
    OPERATE_RET rt = OPRT_OK;
    return rt;
}

TUYA_ERRNO tal_at_net_listen(const int fd, const int backlog)
{
    PR_ERR("[%s] not supported", __func__);
    return OPRT_NOT_SUPPORTED;
}

TUYA_ERRNO tal_at_net_send(const int fd, const void *buf, const uint32_t nbytes)
{
    // OPERATE_RET rt = OPRT_OK;
    int send_len = 0;
    AT_MODULE_OPS_T *ops = &sg_at_modem.ops;

    // PR_DEBUG("tal_at_net_send: fd=%d, nbytes=%d", fd, nbytes);

    TUYA_CHECK_NULL_RETURN(buf, UNW_FAIL);

    if (fd < 0 || fd >= sg_at_modem.socket_num_max) {
        PR_ERR("Invalid socket fd: %d", fd);
        return UNW_FAIL;
    }

    AT_SOCKET_T *socket = &sg_at_modem.socket[fd];
    if (socket->is_used == 0 || socket->is_connected == 0) {
        PR_ERR("Socket %d not used or not connected", fd);
        return UNW_FAIL;
    }

    if (ops->at_net_send) {
        // tal_mutex_lock(socket->mutex);
        send_len = ops->at_net_send(fd, buf, nbytes, socket->send_timeout_ms);
        // tal_mutex_unlock(socket->mutex);
    }

    return send_len;
}

TUYA_ERRNO tal_at_net_send_to(const int fd, const void *buf, const uint32_t nbytes, const TUYA_IP_ADDR_T addr,
                              const uint16_t port)
{
    PR_ERR("[%s] not supported", __func__);
    return OPRT_NOT_SUPPORTED;
}

int tal_at_net_accept(const int fd, TUYA_IP_ADDR_T *addr, uint16_t *port)
{
    PR_ERR("[%s] not supported", __func__);
    return OPRT_NOT_SUPPORTED;
}

TUYA_ERRNO tal_at_net_recv(const int fd, void *buf, const uint32_t nbytes)
{
    TUYA_ERRNO rt_errno = UNW_SUCCESS;
    AT_MODULE_OPS_T *ops = &sg_at_modem.ops;

    // PR_DEBUG("tal_at_net_recv: fd=%d, nbytes=%d", fd, nbytes);

    if (NULL == buf || nbytes == 0) {
        PR_ERR("Invalid buffer or size");
        return UNW_FAIL;
    }

    if (fd < 0 || fd >= sg_at_modem.socket_num_max) {
        PR_ERR("Invalid socket fd: %d", fd);
        return UNW_FAIL;
    }

    AT_SOCKET_T *socket = &sg_at_modem.socket[fd];

    if (socket->is_used == 0 || socket->is_connected == 0) {
        PR_ERR("Socket %d not used or not connected", fd);
        return UNW_FAIL;
    }

    if (ops->at_net_recv) {
        // tal_mutex_lock(socket->mutex);
        rt_errno = ops->at_net_recv(fd, buf, nbytes, socket->recv_timeout_ms);
        // tal_mutex_unlock(socket->mutex);
    }

    return rt_errno;
}

int tal_at_net_recv_nd_size(const int fd, void *buf, const uint32_t buf_size, const uint32_t nd_size)
{
    PR_ERR("[%s] not supported", __func__);
    return OPRT_NOT_SUPPORTED;
}

TUYA_ERRNO tal_at_net_recvfrom(const int fd, void *buf, const uint32_t nbytes, TUYA_IP_ADDR_T *addr, uint16_t *port)
{
    PR_ERR("[%s] not supported", __func__);
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET tal_at_net_set_timeout(const int fd, const int ms_timeout, const TUYA_TRANS_TYPE_E type)
{
    OPERATE_RET rt = OPRT_OK;
    AT_SOCKET_T *socket = &sg_at_modem.socket[fd];

    if (fd < 0 || fd >= sg_at_modem.socket_num_max) {
        PR_ERR("Invalid socket fd: %d", fd);
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(socket->mutex);
    if (type == TRANS_RECV) {
        socket->recv_timeout_ms = ms_timeout;
    } else {
        socket->send_timeout_ms = ms_timeout;
    }
    tal_mutex_unlock(socket->mutex);

    return rt;
}

OPERATE_RET tal_at_net_set_bufsize(const int fd, const int buf_size, const TUYA_TRANS_TYPE_E type)
{
    OPERATE_RET rt = OPRT_OK;
    return rt;
}

OPERATE_RET tal_at_net_set_reuse(const int fd)
{
    PR_ERR("[%s] not supported", __func__);
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET tal_at_net_disable_nagle(const int fd)
{
    PR_ERR("[%s] not supported", __func__);
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET tal_at_net_set_broadcast(const int fd)
{
    PR_ERR("[%s] not supported", __func__);
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET tal_at_net_gethostbyname(const char *domain, TUYA_IP_ADDR_T *addr)
{
    OPERATE_RET rt = OPRT_OK;
    AT_MODULE_OPS_T *ops = &sg_at_modem.ops;

    if (NULL == domain || NULL == addr) {
        PR_ERR("Invalid inputs");
        return OPRT_INVALID_PARM;
    }

    if (ops->at_net_gethostbyname) {
        TUYA_CALL_ERR_LOG(ops->at_net_gethostbyname(domain, addr));
    }

    return rt;
}

OPERATE_RET tal_at_net_set_keepalive(int fd, const BOOL_T alive, const uint32_t idle, const uint32_t intr,
                                     const uint32_t cnt)
{
    PR_ERR("[%s] not supported", __func__);
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET tal_at_net_get_socket_ip(int fd, TUYA_IP_ADDR_T *addr)
{
    OPERATE_RET rt = OPRT_OK;

    if (fd < 0 || fd >= sg_at_modem.socket_num_max || addr == NULL) {
        return OPRT_INVALID_PARM;
    }

    AT_SOCKET_T *socket = &sg_at_modem.socket[fd];

    if (socket->is_used == 0) {
        PR_ERR("Socket %d not used", fd);
        return OPRT_COM_ERROR;
    }

    *addr = socket->peer_ip;

    return rt;
}

TUYA_IP_ADDR_T tal_at_net_str2addr(const char *ip_str)
{
    return at_utils_str2addr(ip_str);
}

char *tal_at_net_addr2str(TUYA_IP_ADDR_T ipaddr)
{
    return at_utils_addr2str(ipaddr);
}

OPERATE_RET tal_at_net_setsockopt(const int fd, const TUYA_OPT_LEVEL level, const TUYA_OPT_NAME optname,
                                  const void *optval, const int optlen)
{
    PR_ERR("[%s] not supported", __func__);
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET tal_at_net_getsockopt(const int fd, const TUYA_OPT_LEVEL level, const TUYA_OPT_NAME optname, void *optval,
                                  int *optlen)
{
    PR_ERR("[%s] not supported", __func__);
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET tal_at_modem_get_ip(NW_IP_S *ip)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == ip) {
        PR_ERR("Invalid parameter: ip is NULL");
        return OPRT_INVALID_PARM;
    }

    // Call module-specific implementation
    switch (sg_at_modem.type) {
    case TAL_AT_MODEM_TYPE_ML307R:
        // rt = at_module_ml307r_get_ip(ip);
        // TODO:
        break;
    default:
        PR_ERR("Unsupported modem type: %d", sg_at_modem.type);
        rt = OPRT_NOT_SUPPORTED;
        break;
    }

    return rt;
}
/* at modem tal network end */
