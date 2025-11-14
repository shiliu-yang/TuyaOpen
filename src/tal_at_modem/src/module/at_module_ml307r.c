/**
 * @file at_module_ml307r.c
 * @brief at_module_ml307r module is used to
 * @version 0.1
 * @date 2025-11-13
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "at_module_ml307r.h"

#include "at_module.h"
#include "at_client.h"
#include "at_command.h"
#include "at_utils.h"

#include "tal_api.h"
#include "tuya_ringbuf.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define AT_MODULE_DEBUG PR_TRACE

// malloc/free
#define ML307R_MALLOC tal_psram_malloc
#define ML307R_FREE   tal_psram_free

// Socket
#define ML307R_SOCKET_NUM_MAX               (6) // Maximum number of sockets supported by ML307R
#define ML307R_SOCKET_RECV_BUF_SIZE_DEFAULT (32 * 1024)

// module error code
#define ML307R_ERR_CODE_SUCCESS                0
#define ML307R_ERR_CODE_TCP_IP_UNKNOWN         550
#define ML307R_ERR_CODE_TCP_IP_NOT_USED        551
#define ML307R_ERR_CODE_TCP_IP_ALREADY_USED    552
#define ML307R_ERR_CODE_TCP_IP_NOT_CONNECTED   553
#define ML307R_ERR_CODE_SOCKET_CREATE_FAIL     554
#define ML307R_ERR_CODE_SOCKET_BIND_FAIL       555
#define ML307R_ERR_CODE_SOCKET_LISTEN_FAIL     556
#define ML307R_ERR_CODE_SOCKET_CONN_REFUSED    557
#define ML307R_ERR_CODE_SOCKET_CONN_TIMEOUT    558
#define ML307R_ERR_CODE_SOCKET_CONN_FAIL       559
#define ML307R_ERR_CODE_SOCKET_WRITE_ABNORMAL  560
#define ML307R_ERR_CODE_SOCKET_READ_ABNORMAL   561
#define ML307R_ERR_CODE_SOCKET_ACCEPT_ABNORMAL 562
#define ML307R_ERR_CODE_PDP_NOT_ACTIVATED      570
#define ML307R_ERR_CODE_PDP_ACTIVATE_FAIL      571
#define ML307R_ERR_CODE_PDP_DEACTIVATE_FAIL    572
#define ML307R_ERR_CODE_APN_NOT_CONFIGURED     575
#define ML307R_ERR_CODE_PORT_BUSY              576
#define ML307R_ERR_CODE_UNSUPPORTED_IPV4_IPV6  577
#define ML307R_ERR_CODE_DNS_RESOLVE_FAIL       580
#define ML307R_ERR_CODE_DNS_BUSY               581
#define ML307R_ERR_CODE_PING_BUSY              582

// PROTOCOL_TCP = 0,
// PROTOCOL_UDP = 1,
#define GET_PROTOCOL_TYPE(type) ((type) == PROTOCOL_TCP ? "TCP" : "UDP")

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    int fd;

    uint8_t is_connected;
    SEM_HANDLE open_sem;

    int send_len;
    SEM_HANDLE send_sem;

    MUTEX_HANDLE recv_mutex;
    TUYA_RINGBUFF_T rb_hdl;
    uint32_t unread_size;

    uint8_t is_receiving;
    DELAYED_WORK_HANDLE recv_delayed_work;
} ML307R_FD_CTX_T;

typedef struct {
    AT_MODULE_CB cb;

    MUTEX_HANDLE err_mutex;
    TUYA_ERRNO errno;

    // dns
    TUYA_IP_ADDR_T dns_ip;
    SEM_HANDLE dns_sem;

    // recv
    ML307R_FD_CTX_T fd_ctx[ML307R_SOCKET_NUM_MAX];
} ML307R_CTX_T;

/***********************************************************
********************function declaration********************
***********************************************************/
// error code
static TUYA_ERRNO __ml307r_at_errno_to_tuya_errno(int ml307r_errno);

// urc handler
static void __urc_handler_matready(char *data, uint32_t len);
static void __urc_handler_cereg(char *data, uint32_t len);
static void __urc_handler_cpin(char *data, uint32_t len);
static void __urc_handler_ip_urc(char *data, uint32_t len);
static void __urc_handler_ip_open(char *data, uint32_t len);
static void __urc_handler_ip_close(char *data, uint32_t len);
static void __urc_handler_ip_send(char *data, uint32_t len);
static void __urc_handler_mdns_gip(char *data, uint32_t len);

static void __ml307r_recv_workq(int fd);

/***********************************************************
***********************variable define**********************
***********************************************************/
static ML307R_CTX_T sg_ml307r_ctx = {0};

static AT_URC_T sg_ml307r_urc_handler[] = {
    {{NULL}, "+MATREADY", NULL, __urc_handler_matready}, {{NULL}, "+CEREG:", NULL, __urc_handler_cereg},
    {{NULL}, "+CPIN:", NULL, __urc_handler_cpin},        {{NULL}, "+MIPURC:", NULL, __urc_handler_ip_urc},
    {{NULL}, "+MIPOPEN:", NULL, __urc_handler_ip_open},  {{NULL}, "+MIPCLOSE:", NULL, __urc_handler_ip_close},
    {{NULL}, "+MIPSEND:", NULL, __urc_handler_ip_send},  {{NULL}, "+MDNSGIP:", NULL, __urc_handler_mdns_gip},
};

/***********************************************************
***********************function define**********************
***********************************************************/
// ----------------------------------------------------
// ML307R module urc callback interface start
// ----------------------------------------------------
/**
 * @brief Parse URC command tokens in format: "+CMD: token1,token2,..."
 * @param data Source data string to parse (will be modified during parsing)
 * @param cmd_prefix Expected command prefix (e.g., "+MIPOPEN")
 * @param delimiters String containing delimiter characters (e.g., ": ," or ": ,\"")
 * @param out_buffer Pointer to store allocated buffer (caller should free this)
 * @param token_array Array to store pointers to parsed token strings
 * @param max_tokens Maximum number of tokens to parse
 * @return Number of tokens parsed successfully, -1 on error
 *
 * Note: The token_array will contain pointers to strings within out_buffer.
 *       Caller should only free out_buffer, not individual token strings.
 */
static int __parse_urc_tokens(char *data, const char *cmd_prefix, const char *delimiters, char **out_buffer,
                              char **token_array, int max_tokens)
{
    if (!data || !cmd_prefix || !delimiters || !out_buffer || !token_array || max_tokens <= 0) {
        return -1;
    }

    // Allocate buffer for strtok processing
    size_t len = strlen(data);
    char *buffer = ML307R_MALLOC(len + 1);
    if (!buffer) {
        PR_ERR("malloc failed for URC parsing");
        return -1;
    }
    memset(buffer, 0, len + 1);
    strncpy(buffer, data, len);
    *out_buffer = buffer;

    // compare cmd_prefix
    if (strncmp(buffer, cmd_prefix, strlen(cmd_prefix)) != 0) {
        PR_ERR("Invalid URC format, expected: %s", cmd_prefix);
        ML307R_FREE(buffer);
        *out_buffer = NULL;
        return -1;
    }
    // Skip prefix
    char *token = NULL;
    char *tok_start = buffer + strlen(cmd_prefix);
    token = strtok(tok_start, delimiters);
    if (!token) {
        PR_ERR("Invalid URC format, missing tokens after %s", cmd_prefix);
        ML307R_FREE(buffer);
        *out_buffer = NULL;
        return -1;
    }
    token_array[0] = token; // Store pointer to first token string
    int parsed_count = 1;

    // Parse tokens and store pointers
    for (int i = 1; i < max_tokens; i++) {
        token = strtok(NULL, delimiters);
        if (!token) {
            break; // End of tokens
        }
        token_array[i] = token; // Store pointer to token string
        parsed_count++;
    }

    return parsed_count;
}

static void __urc_handler_matready(char *data, uint32_t len)
{
    AT_MODULE_DEBUG("ML307R URC +MATREADY: %*s", len, data);

    // AT module is ready

    return;
}

static void __urc_handler_cereg(char *data, uint32_t len)
{
    AT_MODULE_DEBUG("ML307R URC +CEREG: %*s", len, data);

    int stat = 0;

    // +CEREG:<stat>[,<lac>,<ci>,<act>]
    char *buffer = NULL;
    char *tokens[4] = {NULL};
    int token_num = __parse_urc_tokens(data, "+CEREG:", ",", &buffer, tokens, 4);

    if (token_num > 0) {
        stat = atoi(tokens[0]);
    }

    AT_MODULE_DEBUG("ML307R CEREG stat: %d", stat);

    AT_NETWORK_STATUS_T network_status = {0};
    if (stat == 5) {
        network_status.status = 1;
    } else {
        network_status.status = 0;
    }

    if (sg_ml307r_ctx.cb) {
        sg_ml307r_ctx.cb(TAL_AT_MODULE_CMD_NETWORK, &network_status);
    }

    if (buffer) {
        ML307R_FREE(buffer);
        buffer = NULL;
    }

    return;
}

static void __urc_handler_cpin(char *data, uint32_t len)
{
    AT_MODULE_DEBUG("ML307R URC +CPIN: %*s", len, data);

    // +CPIN: <code>
    // e.g., +CPIN: READY
    // SIM PIN: is waiting UICC/SIM PIN to be given
    // SIM PUK: is waiting UICC/SIM PUK to be given
    // SIM PIN2: is waiting active application in the UICC (GSM or USIM)or SIM card PIN2 to be given

    char *buffer = NULL;
    char *tokens[4] = {NULL};

    int token_num = __parse_urc_tokens(data, "+CPIN:", ",", &buffer, tokens, 4);
    if (token_num > 0) {
        PR_DEBUG("ML307R CPIN status: %s", tokens[0]);
    }

    if (buffer) {
        ML307R_FREE(buffer);
        buffer = NULL;
    }

    return;
}

static void __miprd_expect_callback(AT_LINE_T *line, void *user_data)
{
    // +MIPRD: <connect_id>,<unread_len>,<data_len>,<data>
    char *buffer = NULL;
    char *tokens[4] = {NULL};

    uint8_t *byte_data = NULL;
    uint32_t byte_data_len = 0;

    ML307R_FD_CTX_T *fd_ctx = NULL;

    int token_num = __parse_urc_tokens(line->data, "+MIPRD:", ", ", &buffer, tokens, 4);
    if (token_num < 4) {
        AT_MODULE_DEBUG("Failed to parse +MIPRD URC, token_num=%d", token_num);
        goto __EXIT;
    }

    int fd = atoi(tokens[0]);
    int unread_len = atoi(tokens[1]);
    int data_len = atoi(tokens[2]);
    char *hex_data = tokens[3];

    if (fd >= ML307R_SOCKET_NUM_MAX) {
        PR_ERR("__miprd_expect_callback fd error, %d", fd);
        goto __EXIT;
    }
    fd_ctx = &sg_ml307r_ctx.fd_ctx[fd];

    AT_MODULE_DEBUG("MIPRD: fd=%d, unread_len=%d, data_len=%d", fd, unread_len, data_len);

    // hex to byte
    byte_data = ML307R_MALLOC(data_len);
    if (byte_data == NULL) {
        PR_ERR("malloc failed for recv data");
        goto __EXIT;
    }
    memset(byte_data, 0, data_len);
    byte_data_len = at_utils_hex_char_to_byte((char *)hex_data, data_len * 2, byte_data, data_len);

    if (NULL != buffer) {
        ML307R_FREE(buffer);
        buffer = NULL;
    }

    tal_mutex_lock(fd_ctx->recv_mutex);
    fd_ctx->unread_size = unread_len;
    tuya_ring_buff_write(fd_ctx->rb_hdl, byte_data, byte_data_len);
    tal_mutex_unlock(fd_ctx->recv_mutex);

__EXIT:
    if (NULL != buffer) {
        ML307R_FREE(buffer);
        buffer = NULL;
    }

    if (NULL != byte_data) {
        ML307R_FREE(byte_data);
        byte_data = NULL;
    }

    return;
}

static void __ml307r_recv_workq_callback(void *data)
{
    OPERATE_RET rt = OPRT_OK;
    int fd = 0;

    ML307R_FD_CTX_T *fd_ctx = (ML307R_FD_CTX_T *)data;
    if (!fd_ctx) {
        PR_ERR("ML307R recv workq callback arg is NULL");
        return;
    }
    fd = fd_ctx->fd;

    uint32_t rb_free_size = tuya_ring_buff_free_size_get(fd_ctx->rb_hdl);

    if (rb_free_size == 0 || fd_ctx->unread_size == 0) {
        PR_WARN("rb_free_size: %d, unread_size: %d", rb_free_size, fd_ctx->unread_size);
        goto __EXIT;
    }

    fd_ctx->is_receiving = 1;

    AT_EXPECT_ITEM_T expect_items[] = {
        {
            .prefix = "+MIPRD:",
            .callback = __miprd_expect_callback,
            .user_data = NULL,
        },
    };

    char tmp_buf[64] = {0};

    // AT+MIPRD=<connect_id>[,<read_len>]
    memset(tmp_buf, 0, sizeof(tmp_buf));

    uint32_t read_len = fd_ctx->unread_size;

    if (read_len > rb_free_size) {
        read_len = rb_free_size;
    }
    read_len = read_len > (3 * 1024) ? (3 * 1024) : read_len;

    AT_MODULE_DEBUG("unread: %d, rb_free_size: %d, read_len: %d", fd_ctx->unread_size, rb_free_size, read_len);

    snprintf(tmp_buf, sizeof(tmp_buf), "AT+MIPRD=%d,%d\r\n", fd, read_len);

    int error_code = 0;
    rt = at_cmd_execute_complex(tmp_buf, expect_items, 1, 10000, NULL, NULL, NULL, &error_code);
    if (rt != OPRT_OK) {
        PR_ERR("ML307R AT+MIPRD execute failed, error code: %d", error_code);
        tal_mutex_lock(sg_ml307r_ctx.err_mutex);
        sg_ml307r_ctx.errno = __ml307r_at_errno_to_tuya_errno(error_code);
        tal_mutex_unlock(sg_ml307r_ctx.err_mutex);
    }

    fd_ctx->is_receiving = 0;

__EXIT:
    rb_free_size = tuya_ring_buff_free_size_get(fd_ctx->rb_hdl);
    if (fd_ctx->unread_size > 0 && rb_free_size > 0) {
        __ml307r_recv_workq(fd);
    } else if (fd_ctx->unread_size > 0 && rb_free_size == 0 && fd_ctx->recv_delayed_work) {
        tal_workq_start_delayed(fd_ctx->recv_delayed_work, 20, LOOP_ONCE);
    }

    return;
}

static void __ml307r_recv_workq(int fd)
{
    if (sg_ml307r_ctx.fd_ctx[fd].is_receiving) {
        return;
    }

    tal_workq_schedule(WORKQ_SYSTEM, __ml307r_recv_workq_callback, &sg_ml307r_ctx.fd_ctx[fd]);

    return;
}

static void __urc_handler_ip_urc(char *data, uint32_t len)
{
    char *urc_type = NULL;
    int fd = -1;

    AT_MODULE_DEBUG("ML307R URC +MIPURC: %*s", len, data);

    char *buffer = NULL;
    char *tokens[4] = {NULL};
    int token_num = __parse_urc_tokens(data, "+MIPURC:", "\" ,", &buffer, tokens, 4);

    if (token_num == 0) {
        PR_ERR("Failed to parse +MIPURC URC");
        goto __EXIT;
    }

    urc_type = tokens[0];
    AT_MODULE_DEBUG("MIPURC type: [%s]", urc_type);

    if (strcmp(urc_type, "disconn") == 0) {
        // +MIPURC: "disconn",<connect_id>,<connect_state>
        AT_MODULE_DEBUG("URC +MIPURC disconn");
        if (token_num < 3) {
            PR_ERR("Invalid +MIPURC disconn URC format");
            goto __EXIT;
        }
        fd = atoi(tokens[1]);

        AT_SOCKET_CONNECT_STATUS_T socket_conn = {0};
        socket_conn.fd = fd;
        socket_conn.status = 0;
        sg_ml307r_ctx.cb(TAL_AT_MODULE_CMD_SOCKET_CONNECT_STATUS, &socket_conn);

        sg_ml307r_ctx.fd_ctx[fd].is_connected = 0;
    } else if (strcmp(urc_type, "rtcp") == 0) {
        // +MIPURC : "rtcp", <connect_id>, <recv_length>, <total_length>
        AT_MODULE_DEBUG("URC +MIPURC rtcp");
        if (token_num < 4) {
            PR_ERR("Invalid +MIPURC rtcp URC format");
            goto __EXIT;
        }
        fd = atoi(tokens[1]);
        int recv_length = atoi(tokens[2]);
        int total_length = atoi(tokens[3]);
        AT_MODULE_DEBUG("URC +MIPURC rtcp: fd=%d, recv_length=%d, total_length=%d", fd, recv_length, total_length);

        tal_mutex_lock(sg_ml307r_ctx.fd_ctx[fd].recv_mutex);
        sg_ml307r_ctx.fd_ctx[fd].unread_size = total_length;
        tal_mutex_unlock(sg_ml307r_ctx.fd_ctx[fd].recv_mutex);

        // work queue read
        __ml307r_recv_workq(fd);
    } else {
        PR_ERR("Unknown MIPURC type: %s", urc_type);
    }

__EXIT:
    if (buffer) {
        ML307R_FREE(buffer);
        buffer = NULL;
    }

    return;
}

static void __urc_handler_ip_open(char *data, uint32_t len)
{
    char *buffer = NULL;
    char *tokens[2] = {NULL};

    AT_MODULE_DEBUG("ML307R URC +MIPOPEN: %*s", len, data);

    // +MIPOPEN: <connect_id>,<result>
    int token_num = __parse_urc_tokens(data, "+MIPOPEN:", ",", &buffer, tokens, 2);
    if (token_num < 2 || NULL == buffer) {
        PR_ERR("ML307R +MIPOPEN URC parse error");
        if (buffer) {
            ML307R_FREE(buffer);
            buffer = NULL;
        }
        return;
    }

    int fd = atoi(tokens[0]);
    int connect_result = atoi(tokens[1]);

    if (fd < 0 || fd >= ML307R_SOCKET_NUM_MAX) {
        PR_ERR("ML307R invalid fd in +MIPOPEN: %d", fd);
        if (buffer) {
            ML307R_FREE(buffer);
            buffer = NULL;
        }
        return;
    }

    tal_mutex_lock(sg_ml307r_ctx.err_mutex);
    sg_ml307r_ctx.errno = __ml307r_at_errno_to_tuya_errno(connect_result);
    tal_mutex_unlock(sg_ml307r_ctx.err_mutex);

    ML307R_FD_CTX_T *fd_ctx = &sg_ml307r_ctx.fd_ctx[fd];
    fd_ctx->is_connected = (sg_ml307r_ctx.errno == UNW_SUCCESS) ? 1 : 0;

    // signal waiters of connect result
    tal_semaphore_post(fd_ctx->open_sem);

    if (buffer) {
        ML307R_FREE(buffer);
        buffer = NULL;
    }

    AT_MODULE_DEBUG("ML307R socket %d open result: %d (errno=%d)", fd, connect_result, sg_ml307r_ctx.errno);

    return;
}

static void __urc_handler_ip_close(char *data, uint32_t len)
{
    char *buffer = NULL;
    char *tokens[2] = {NULL};
    // +MIPCLOSE: <connect_id>[,<ret_code>]

    AT_MODULE_DEBUG("ML307R URC +MIPCLOSE: %*s", len, data);

    int token_num = __parse_urc_tokens(data, "+MIPCLOSE:", ",", &buffer, tokens, 2);
    if (token_num < 1 || NULL == buffer) {
        PR_ERR("ML307R +MIPCLOSE URC parse error");
        if (buffer) {
            ML307R_FREE(buffer);
            buffer = NULL;
        }
        return;
    }

    int fd = atoi(tokens[0]);
    if (fd < 0 || fd >= ML307R_SOCKET_NUM_MAX) {
        PR_ERR("ML307R invalid fd in +MIPCLOSE: %d", fd);
        if (buffer) {
            ML307R_FREE(buffer);
            buffer = NULL;
        }
        return;
    }
    ML307R_FD_CTX_T *fd_ctx = &sg_ml307r_ctx.fd_ctx[fd];
    tal_mutex_lock(fd_ctx->recv_mutex);
    fd_ctx->is_connected = 0;
    fd_ctx->unread_size = 0;
    if (fd_ctx->rb_hdl) {
        tuya_ring_buff_reset(fd_ctx->rb_hdl);
    }
    tal_mutex_unlock(fd_ctx->recv_mutex);

    if (buffer) {
        ML307R_FREE(buffer);
        buffer = NULL;
    }

    return;
}

static void __urc_handler_ip_send(char *data, uint32_t len)
{
    // +MIPSEND: <connect_id>,<send_len>

    AT_MODULE_DEBUG("ML307R URC +MIPSEND: %*s", len, data);

    char *buffer = NULL;
    char *tokens[2] = {NULL};
    int token_num = __parse_urc_tokens(data, "+MIPSEND:", ",", &buffer, tokens, 2);
    if (token_num < 2 || NULL == buffer) {
        PR_ERR("ML307R +MIPSEND URC parse error");
        if (buffer) {
            ML307R_FREE(buffer);
            buffer = NULL;
        }
        return;
    }

    int fd = atoi(tokens[0]);
    int send_len = atoi(tokens[1]);

    if (fd < 0 || fd >= ML307R_SOCKET_NUM_MAX) {
        PR_ERR("ML307R invalid fd in +MIPSEND: %d", fd);
        if (buffer) {
            ML307R_FREE(buffer);
            buffer = NULL;
        }
        return;
    }

    ML307R_FD_CTX_T *fd_ctx = &sg_ml307r_ctx.fd_ctx[fd];
    fd_ctx->send_len = send_len;

    if (buffer) {
        ML307R_FREE(buffer);
        buffer = NULL;
    }

    // wake up send waiter
    tal_semaphore_post(fd_ctx->send_sem);

    return;
}

static void __urc_handler_mdns_gip(char *data, uint32_t len)
{
    AT_MODULE_DEBUG("ML307R URC +MDNSGIP: %*s", len, data);

    // +MDNSGIP: <hostname>,<ip_address>

    char *buffer = NULL;
    char *tokens[8] = {NULL};
    int token_num = __parse_urc_tokens(data, "+MDNSGIP:", ",", &buffer, tokens, 8);
    if (token_num < 2 || NULL == buffer) {
        PR_ERR("ML307R +MDNSGIP URC parse error");
        if (buffer) {
            ML307R_FREE(buffer);
            buffer = NULL;
        }
        return;
    }

    const char *hostname = tokens[0];

    for (int i = 1; i < token_num; i++) {
        AT_MODULE_DEBUG("ML307R MDNSGIP additional ip[%d]: %s", i, tokens[i]);
        char *domain_ip = tokens[i];
        if (at_utils_is_ipv4(domain_ip)) {
            tal_mutex_lock(sg_ml307r_ctx.err_mutex);
            sg_ml307r_ctx.dns_ip = at_utils_str2addr(domain_ip);
            tal_mutex_unlock(sg_ml307r_ctx.err_mutex);
            AT_MODULE_DEBUG("Domain ip: %s, 0x%08X", domain_ip, sg_ml307r_ctx.dns_ip);
            break;
        }
    }

    if (buffer) {
        ML307R_FREE(buffer);
        buffer = NULL;
    }

    // signal dns waiters
    tal_semaphore_post(sg_ml307r_ctx.dns_sem);

    return;
}

// ----------------------------------------------------
// ML307R module urc callback interface end
// ----------------------------------------------------

// ----------------------------------------------------
// ML307R module operations interface start
// ----------------------------------------------------
static uint8_t __at_net_get_socket_max_num(void)
{
    return ML307R_SOCKET_NUM_MAX;
}

static TUYA_ERRNO __ml307r_at_errno_to_tuya_errno(int ml307r_errno)
{
    switch (ml307r_errno) {
    case ML307R_ERR_CODE_SUCCESS:
        return UNW_SUCCESS;
    case ML307R_ERR_CODE_TCP_IP_NOT_USED:
        return UNW_ENOTSOCK;
    case ML307R_ERR_CODE_TCP_IP_ALREADY_USED:
        return UNW_EADDRINUSE;
    case ML307R_ERR_CODE_TCP_IP_NOT_CONNECTED:
        return UNW_ENOTCONN;
    case ML307R_ERR_CODE_SOCKET_CREATE_FAIL:
        return UNW_ENOBUFS;
    case ML307R_ERR_CODE_SOCKET_BIND_FAIL:
        return UNW_EADDRNOTAVAIL;
    case ML307R_ERR_CODE_SOCKET_LISTEN_FAIL:
        return UNW_EINVAL;
    case ML307R_ERR_CODE_SOCKET_CONN_REFUSED:
        return UNW_ECONNREFUSED;
    case ML307R_ERR_CODE_SOCKET_CONN_TIMEOUT:
        return UNW_ETIMEDOUT;
    case ML307R_ERR_CODE_SOCKET_CONN_FAIL:
        return UNW_ECONNRESET;
    case ML307R_ERR_CODE_SOCKET_WRITE_ABNORMAL:
        return UNW_EPIPE;
    case ML307R_ERR_CODE_SOCKET_READ_ABNORMAL:
        return UNW_EPIPE;
    case ML307R_ERR_CODE_SOCKET_ACCEPT_ABNORMAL:
        return UNW_EINVAL;
    case ML307R_ERR_CODE_PDP_NOT_ACTIVATED:
        return UNW_EINVAL;
    case ML307R_ERR_CODE_PDP_ACTIVATE_FAIL:
        return UNW_EINVAL;
    case ML307R_ERR_CODE_PDP_DEACTIVATE_FAIL:
        return UNW_EINVAL;
    case ML307R_ERR_CODE_APN_NOT_CONFIGURED:
        return UNW_EINVAL;
    case ML307R_ERR_CODE_PORT_BUSY:
        return UNW_EBUSY;
    case ML307R_ERR_CODE_UNSUPPORTED_IPV4_IPV6:
        return UNW_EINVAL;
    case ML307R_ERR_CODE_DNS_RESOLVE_FAIL:
        return UNW_EINVAL;
    case ML307R_ERR_CODE_DNS_BUSY:
        return UNW_EBUSY;
    case ML307R_ERR_CODE_PING_BUSY:
        return UNW_EBUSY;
    case ML307R_ERR_CODE_TCP_IP_UNKNOWN:
        return TUYA_ERRNO_NOT_SUPPORT;
    default:
        return TUYA_ERRNO_NOT_SUPPORT;
    }
}

static TUYA_ERRNO __at_net_get_errno(void)
{
    return sg_ml307r_ctx.errno;
}

static TUYA_ERRNO __at_net_connect(const int fd, TUYA_PROTOCOL_TYPE_E type, const TUYA_IP_ADDR_T addr,
                                   const char *addr_str, const uint16_t port)
{
    OPERATE_RET rt = OPRT_OK;
    int error_code = 0;

    if (fd < 0 || fd >= ML307R_SOCKET_NUM_MAX) {
        return UNW_FAIL;
    }

    ML307R_FD_CTX_T *fd_ctx = &sg_ml307r_ctx.fd_ctx[fd];

    char tmp_buf[64] = {0};

    // AT+MIPCFG="rcvbuf"[,<connect_id>[,<recv_buffer>]]
    memset(tmp_buf, 0, sizeof(tmp_buf));
    snprintf(tmp_buf, sizeof(tmp_buf), "AT+MIPCFG=\"rcvbuf\",%d,%d\r\n", fd, 4 * 1024);
    rt = at_cmd_execute_simple(tmp_buf, 5000, &error_code);
    if (rt != OPRT_OK) {
        PR_ERR("Set ML307R socket %d recv buffer size failed, error code: %d", fd, error_code);
        tal_mutex_lock(sg_ml307r_ctx.err_mutex);
        sg_ml307r_ctx.errno = __ml307r_at_errno_to_tuya_errno(error_code);
        tal_mutex_unlock(sg_ml307r_ctx.err_mutex);
        return sg_ml307r_ctx.errno;
    }
    AT_MODULE_DEBUG("Set ML307R socket %d recv buffer size", fd);

    // "AT+MIPOPEN=fd,\"TCP\",\"<addr>\",<port>\r";
    memset(tmp_buf, 0, sizeof(tmp_buf));
    snprintf(tmp_buf, sizeof(tmp_buf), "AT+MIPOPEN=%d,\"%s\",\"%s\",%d\r\n", fd, GET_PROTOCOL_TYPE(type), addr_str,
             port);
    rt = at_cmd_execute_simple(tmp_buf, 5000, &error_code);
    if (rt != OPRT_OK) {
        PR_ERR("ML307R socket %d connect %s:%d failed, error code: %d", fd, addr_str, port, error_code);
        tal_mutex_lock(sg_ml307r_ctx.err_mutex);
        sg_ml307r_ctx.errno = __ml307r_at_errno_to_tuya_errno(error_code);
        tal_mutex_unlock(sg_ml307r_ctx.err_mutex);
        return sg_ml307r_ctx.errno;
    }

    // wait for connection established URC
    // wait for URC notification via semaphore
    AT_MODULE_DEBUG("ML307R socket %d waiting for connect result", fd);
    tal_semaphore_wait(fd_ctx->open_sem, SEM_WAIT_FOREVER);

    if (fd_ctx->is_connected != 1) {
        PR_ERR("ML307R socket %d connect failed", fd);
        // connect failed, errno already set in URC handler
        return sg_ml307r_ctx.errno;
    }

    // ip config
    // AT+MIPCFG="encoding",<connect_id>,<send_format>,<recv_format>
    // send_format: 0: ASCII, 1: HEX, 2: Escaped string
    memset(tmp_buf, 0, sizeof(tmp_buf));
    snprintf(tmp_buf, sizeof(tmp_buf), "AT+MIPCFG=\"encoding\",%d,1,1\r\n", fd);
    rt = at_cmd_execute_simple(tmp_buf, 5000, &error_code);
    if (rt != OPRT_OK) {
        PR_ERR("Set ML307R socket %d ip encoding failed, error code: %d", fd, error_code);
        tal_mutex_lock(sg_ml307r_ctx.err_mutex);
        sg_ml307r_ctx.errno = __ml307r_at_errno_to_tuya_errno(error_code);
        tal_mutex_unlock(sg_ml307r_ctx.err_mutex);
        // TODO: close socket?
        return sg_ml307r_ctx.errno;
    }
    AT_MODULE_DEBUG("Set ML307R socket %d ip encoding", fd);

    // AT+MIPCFG="autofree",<connect_id>,<free_mode>
    // free_mode: 0: Automatically release after disconnection;
    // 1: Do not release after disconnection, manual release
    memset(tmp_buf, 0, sizeof(tmp_buf));
    snprintf(tmp_buf, sizeof(tmp_buf), "AT+MIPCFG=\"autofree\",%d,0\r\n", fd);
    rt = at_cmd_execute_simple(tmp_buf, 5000, &error_code);
    if (rt != OPRT_OK) {
        PR_ERR("Set ML307R socket %d autofree failed, error code: %d", fd, error_code);
        tal_mutex_lock(sg_ml307r_ctx.err_mutex);
        sg_ml307r_ctx.errno = __ml307r_at_errno_to_tuya_errno(error_code);
        tal_mutex_unlock(sg_ml307r_ctx.err_mutex);
        // TODO: close socket?
        return sg_ml307r_ctx.errno;
    }
    AT_MODULE_DEBUG("Set ML307R socket %d autofree", fd);

    // AT+MIPMODE=<connect_id>[,<access_mode>...]
    memset(tmp_buf, 0, sizeof(tmp_buf));
    snprintf(tmp_buf, sizeof(tmp_buf), "AT+MIPMODE=%d,2\r\n", fd);
    rt = at_cmd_execute_simple(tmp_buf, 5000, &error_code);
    if (rt != OPRT_OK) {
        PR_ERR("Set ML307R socket %d access mode failed, error code: %d", fd, error_code);
        tal_mutex_lock(sg_ml307r_ctx.err_mutex);
        sg_ml307r_ctx.errno = __ml307r_at_errno_to_tuya_errno(error_code);
        tal_mutex_unlock(sg_ml307r_ctx.err_mutex);
        // TODO: close socket?
        return sg_ml307r_ctx.errno;
    }
    AT_MODULE_DEBUG("Set ML307R socket %d access mode", fd);

    return UNW_SUCCESS;
}

static TUYA_ERRNO __at_net_close(const int fd)
{
    char tmp_buf[64] = {0};
    int error_code = 0;

    // AT+MIPCLOSE=<fd>
    memset(tmp_buf, 0, sizeof(tmp_buf));
    snprintf(tmp_buf, sizeof(tmp_buf), "AT+MIPCLOSE=%d\r\n", fd);
    OPERATE_RET rt = at_cmd_execute_simple(tmp_buf, 5000, &error_code);
    if (rt != OPRT_OK) {
        PR_ERR("ML307R socket %d close failed, error code: %d", fd, error_code);
        ML307R_FD_CTX_T *fd_ctx = &sg_ml307r_ctx.fd_ctx[fd];
        tal_mutex_lock(sg_ml307r_ctx.err_mutex);
        sg_ml307r_ctx.errno = __ml307r_at_errno_to_tuya_errno(error_code);
        tal_mutex_unlock(sg_ml307r_ctx.err_mutex);
        return sg_ml307r_ctx.errno;
    }

    // wait for socket closed

    return UNW_SUCCESS;
}

typedef struct {
    int fd;
    const void *buf;
    uint32_t nbytes;
} AT_EXPECT_SEND_USER_DATA_T;

static void at_expect_send_callback(AT_LINE_T *line, void *user_data)
{
    AT_EXPECT_SEND_USER_DATA_T *send_data = (AT_EXPECT_SEND_USER_DATA_T *)user_data;

    AT_MODULE_DEBUG("ML307R socket %d line data: %s", send_data->fd, line->data);
    AT_MODULE_DEBUG("ML307R socket %d send data len: %d", send_data->fd, (int)send_data->nbytes);

    at_client_send((const char *)send_data->buf, (uint32_t)send_data->nbytes);

    return;
}

static TUYA_ERRNO __at_net_send(const int fd, const void *buf, const uint32_t nbytes, const int timeout_ms)
{
    char tmp_buf[32] = {0};
    OPERATE_RET rt = OPRT_OK;
    int error_code = 0;

    AT_EXPECT_SEND_USER_DATA_T user_data = {
        .fd = fd,
        .buf = buf,
        .nbytes = nbytes,
    };

    AT_EXPECT_ITEM_T expect_items[] = {
        {
            ">",
            at_expect_send_callback,
            &user_data,
        },
    };
    uint32_t expect_item_count = sizeof(expect_items) / sizeof(AT_EXPECT_ITEM_T);

    // AT+MIPSEND=fd,len
    memset(tmp_buf, 0, sizeof(tmp_buf));
    snprintf(tmp_buf, sizeof(tmp_buf), "AT+MIPSEND=%d,%d\r\n", fd, nbytes);

    ML307R_FD_CTX_T *fd_ctx = &sg_ml307r_ctx.fd_ctx[fd];

    fd_ctx->send_len = 0;
    rt = at_cmd_execute_complex(tmp_buf, expect_items, expect_item_count, timeout_ms, NULL, NULL, &user_data,
                                &error_code);
    if (rt != OPRT_OK) {
        PR_ERR("ML307R socket %d send data failed, error code: %d", fd, error_code);
        tal_mutex_lock(sg_ml307r_ctx.err_mutex);
        sg_ml307r_ctx.errno = __ml307r_at_errno_to_tuya_errno(error_code);
        tal_mutex_unlock(sg_ml307r_ctx.err_mutex);
        return sg_ml307r_ctx.errno;
    }

    // wait for send length URC
    AT_MODULE_DEBUG("ML307R socket %d waiting for send length", fd);
    tal_semaphore_wait(fd_ctx->send_sem, SEM_WAIT_FOREVER);

    int actual = fd_ctx->send_len;
    if (actual != (int)nbytes) {
        PR_WARN("ML307R socket %d send length mismatch, expect %u got %d", fd, nbytes, actual);
    }
    AT_MODULE_DEBUG("__at_net_send actual: %d", actual);

    return actual;
}

static TUYA_ERRNO __at_net_recv(const int fd, void *buf, const uint32_t nbytes, const int timeout_ms)
{
    AT_MODULE_DEBUG("-----> %s called, fd: %d, nbytes: %d", __func__, fd, nbytes);

    int recv_len = 0;
    ML307R_FD_CTX_T *fd_ctx = &sg_ml307r_ctx.fd_ctx[fd];

    SYS_TIME_T start_time = tal_system_get_millisecond();
    SYS_TIME_T current_time = start_time;

    do {
        uint32_t cache_data_len = tuya_ring_buff_used_size_get(fd_ctx->rb_hdl);
        // 当前数据大于要读取的
        if (cache_data_len >= nbytes) {
            tal_mutex_lock(fd_ctx->recv_mutex);
            recv_len = tuya_ring_buff_read(fd_ctx->rb_hdl, buf, nbytes);
            tal_mutex_unlock(fd_ctx->recv_mutex);
            break;
        }

        tal_system_sleep(20);

        current_time = tal_system_get_millisecond();
    } while (current_time - start_time < timeout_ms);

    AT_MODULE_DEBUG("__at_net_recv len: %d, cache data len: %d, unread size: %d", recv_len,
                    tuya_ring_buff_used_size_get(fd_ctx->rb_hdl), fd_ctx->unread_size);

    return recv_len;
}

static OPERATE_RET __at_net_gethostbyname(const char *domain, TUYA_IP_ADDR_T *addr)
{
    // AT+MDNSGIP=<domainname>[,<cid>]
    char tmp_buf[88] = {0};
    OPERATE_RET rt = OPRT_OK;
    int error_code = 0;

    AT_MODULE_DEBUG("Gethostbyname: domain=%s", domain);

    sg_ml307r_ctx.dns_ip = 0;

    memset(tmp_buf, 0, sizeof(tmp_buf));
    snprintf(tmp_buf, sizeof(tmp_buf), "AT+MDNSGIP=\"%s\"\r\n", domain);
    rt = at_cmd_execute_simple(tmp_buf, 10 * 1000, &error_code);
    if (rt != OPRT_OK) {
        PR_ERR("ML307R gethostbyname %s failed, error code: %d", domain, error_code);
        tal_mutex_lock(sg_ml307r_ctx.err_mutex);
        sg_ml307r_ctx.errno = __ml307r_at_errno_to_tuya_errno(error_code);
        tal_mutex_unlock(sg_ml307r_ctx.err_mutex);
        return OPRT_COM_ERROR;
    }

    // wait dns URC
    tal_semaphore_wait(sg_ml307r_ctx.dns_sem, SEM_WAIT_FOREVER);

    *addr = sg_ml307r_ctx.dns_ip;

    return OPRT_OK;
}

// ----------------------------------------------------
// ML307R module operations interface end
// ----------------------------------------------------

// ----------------------------------------------------
// ML307R module interface start
// ----------------------------------------------------
static OPERATE_RET __ml307r_urc_register(void)
{
    OPERATE_RET rt = OPRT_OK;

    uint32_t urc_count = sizeof(sg_ml307r_urc_handler) / sizeof(AT_URC_T);

    for (uint32_t i = 0; i < urc_count; i++) {
        TUYA_CALL_ERR_RETURN(at_client_add_urc_handler(&sg_ml307r_urc_handler[i]));
    }

    AT_MODULE_DEBUG("Registering ML307R URC handler successfully");

    return rt;
}

static OPERATE_RET __ml307r_software_reboot(void)
{
    int error_code = 0;
    return at_cmd_execute_simple("AT+MREBOOT=0\r\n", 5000, &error_code);
}

OPERATE_RET at_module_ml307r_init(AT_MODULE_OPS_T *ops, AT_MODULE_CB cb)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(ops, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(cb, OPRT_INVALID_PARM);

    sg_ml307r_ctx.cb = cb;

    // assign function pointers
    ops->at_net_get_socket_max_num = __at_net_get_socket_max_num;
    ops->at_net_get_errno = __at_net_get_errno;
    ops->at_net_connect = __at_net_connect;
    ops->at_net_close = __at_net_close;
    ops->at_net_send = __at_net_send;
    ops->at_net_recv = __at_net_recv;
    ops->at_net_gethostbyname = __at_net_gethostbyname;

    // register urc handler
    TUYA_CALL_ERR_GOTO(__ml307r_urc_register(), __ERR);

    // error mutex init
    TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&sg_ml307r_ctx.err_mutex), __ERR);

    // dns sem init
    TUYA_CALL_ERR_GOTO(tal_semaphore_create_init(&sg_ml307r_ctx.dns_sem, 0, 1), __ERR);

    // recv buffer init
    for (int i = 0; i < ML307R_SOCKET_NUM_MAX; i++) {
        ML307R_FD_CTX_T *fd_ctx = &sg_ml307r_ctx.fd_ctx[i];
        fd_ctx->fd = i;
        // sem
        TUYA_CALL_ERR_GOTO(tal_semaphore_create_init(&fd_ctx->open_sem, 0, 1), __ERR);
        TUYA_CALL_ERR_GOTO(tal_semaphore_create_init(&fd_ctx->send_sem, 0, 1), __ERR);
        // fd_ctx->mutex
        TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&fd_ctx->recv_mutex), __ERR);
        // fd_ctx->rb_hdl
        TUYA_CALL_ERR_GOTO(
            tuya_ring_buff_create(ML307R_SOCKET_RECV_BUF_SIZE_DEFAULT, OVERFLOW_PSRAM_STOP_TYPE, &fd_ctx->rb_hdl),
            __ERR);
        fd_ctx->unread_size = 0;
        // tal_workq_init_delayed
        fd_ctx->is_receiving = 0;
        TUYA_CALL_ERR_GOTO(
            tal_workq_init_delayed(WORKQ_SYSTEM, __ml307r_recv_workq_callback, fd_ctx, &fd_ctx->recv_delayed_work),
            __ERR);
    }

    // reboot module
    __ml307r_software_reboot();

    AT_MODULE_DEBUG("at_module_ml307r_init success");

    return rt;

__ERR:
    at_module_ml307r_deinit();
    AT_MODULE_DEBUG("at_module_ml307r_init failed");

    return rt;
}

OPERATE_RET at_module_ml307r_deinit(void)
{
    OPERATE_RET rt = OPRT_OK;
    // release per socket context resources
    for (int i = 0; i < ML307R_SOCKET_NUM_MAX; i++) {
        ML307R_FD_CTX_T *fd_ctx = &sg_ml307r_ctx.fd_ctx[i];

        if (fd_ctx->open_sem) {
            TUYA_CALL_ERR_LOG(tal_semaphore_release(fd_ctx->open_sem));
            fd_ctx->open_sem = NULL;
        }
        if (fd_ctx->send_sem) {
            TUYA_CALL_ERR_LOG(tal_semaphore_release(fd_ctx->send_sem));
            fd_ctx->send_sem = NULL;
        }
        if (fd_ctx->recv_mutex) {
            TUYA_CALL_ERR_LOG(tal_mutex_release(fd_ctx->recv_mutex));
            fd_ctx->recv_mutex = NULL;
        }
        if (fd_ctx->rb_hdl) {
            TUYA_CALL_ERR_LOG(tuya_ring_buff_free(fd_ctx->rb_hdl));
            fd_ctx->rb_hdl = NULL;
        }
        fd_ctx->unread_size = 0;
        fd_ctx->is_connected = 0;
        fd_ctx->send_len = 0;
        fd_ctx->fd = -1; // mark invalid
    }

    // release error mutex
    if (sg_ml307r_ctx.err_mutex) {
        TUYA_CALL_ERR_LOG(tal_mutex_release(sg_ml307r_ctx.err_mutex));
        sg_ml307r_ctx.err_mutex = NULL;
    }

    sg_ml307r_ctx.errno = 0;
    sg_ml307r_ctx.cb = NULL;

    return rt;
}
// ----------------------------------------------------
// ML307R module interface end
// ----------------------------------------------------
