/**
 * @file at_client.c
 * @brief at_client module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "at_client.h"

#include "tal_api.h"

#include "tuya_ringbuf.h"

/***********************************************************
************************macro define************************
***********************************************************/
// memory manage
#define AT_MALLOC tal_malloc
#define AT_FREE   tal_free

#define AT_CLIENT_RECV_BUF_SIZE (5 * 1024) // Default size of the receive buffer

#define IDLE_SLEEP_MS    (100)
#define WAITING_SLEEP_MS (20)

typedef uint8_t AT_CLIENT_STATUS_T;
#define AT_CLIENT_STATUS_IDLE       0x00 // Client is idle, not processing any command
#define AT_CLIENT_STATUS_WAITING    0x01 // Client waiting for response
#define AT_CLIENT_STATUS_PROCESSING 0x02 // Client processing response

#define AT_CLIENT_STATUS_CHANGE(new_status)                                                                            \
    do {                                                                                                               \
        PR_DEBUG("AT client status changed: [%s] --> [%s]", AT_CLIENT_STATUS_STR[sg_at_client.status],                 \
                 AT_CLIENT_STATUS_STR[new_status]);                                                                    \
        sg_at_client.status = new_status;                                                                              \
    } while (0)

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    THREAD_HANDLE thread_hdl;
    MUTEX_HANDLE mutex;

    AT_CLIENT_STATUS_T status; // Status of the AT client

    TDL_TRANSPORT_HANDLE transport_hdl;

    char end_symbol[LINE_END_SYMBOL_MAX_LEN]; // End symbol for AT commands, e.g., "\r\n"

    // List of received lines
    AT_LINE_HANDLE recv_line_hdl;

    // Buffer for received data
    uint8_t *recv_buf;      // Buffer to store received data
    uint32_t recv_buf_size; // Size of the received data buffer
    uint32_t recv_buf_len;  // Current length of the received data buffer

    // URC list
    SLIST_HEAD urc_list;
} AT_CLIENT_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static char *AT_CLIENT_STATUS_STR[] = {
    "IDLE",
    "WAITING",
    "PROCESSING",
};

static AT_CLIENT_T sg_at_client = {0};

/***********************************************************
***********************function define**********************
***********************************************************/

static URC_HANDLER __at_line_get_urc_handler(SLIST_HEAD *urc_list, AT_LINE_T *line)
{
    if (NULL == urc_list || NULL == line) {
        PR_ERR("URC list or line is NULL");
        return NULL;
    }

    SLIST_HEAD *node = urc_list->next;
    while (node != NULL) {
        AT_URC_T *urc = (AT_URC_T *)node;

        if (urc->prefix) {
            if (strncmp(line->data, urc->prefix, strlen(urc->prefix)) == 0) {
                return urc->handler;
            }
        }

        if (urc->suffix) {
            if (strncmp(line->data + line->len - strlen(urc->suffix), urc->suffix, strlen(urc->suffix)) == 0) {
                return urc->handler;
            }
        }

        node = node->next;
    }

    return NULL;
}

static uint32_t __at_line_split(AT_CLIENT_T *at_client)
{
    uint32_t new_line_num = 0;
    AT_LINE_T *new_line = NULL;

    uint32_t available = tdl_transport_available(at_client->transport_hdl);
    uint8_t *p_recv = at_client->recv_buf + at_client->recv_buf_len;
    uint32_t free_space = at_client->recv_buf_size - at_client->recv_buf_len;

    if (0 == available) {
        return 0;
    }

    tal_mutex_lock(at_client->mutex);

    // read new data
    uint32_t read_len = tdl_transport_read(at_client->transport_hdl, p_recv, free_space);
    if (read_len > 0) {
        at_client->recv_buf_len += read_len;
        // PR_DEBUG("--> Read %d bytes from transport", read_len);
        // PR_DEBUG("--> Received data: %.*s", read_len, p_recv);
    }

    // get new line
    char *p_start = (char *)at_client->recv_buf;
    char *p_end = NULL;
    uint32_t offset = 0;
    do {
        p_end = strstr(p_start, at_client->end_symbol);
        if (p_end && p_end > p_start) {
            new_line = NULL;
            new_line = at_line_create(p_start, p_end - p_start);
            if (NULL == new_line) {
                PR_ERR("Failed to create new AT line");
                goto __EXIT;
            }
            // new_line->is_urc = __at_line_is_urc(&at_client->urc_list, new_line);
            new_line->extra = (void *)__at_line_get_urc_handler(&at_client->urc_list, new_line);
            at_line_add(at_client->recv_line_hdl, new_line);
            offset = p_end - p_start + strlen(at_client->end_symbol);
            p_start = (char *)at_client->recv_buf + offset;
            break;
        } else if (p_end == p_start) {
            p_start += strlen(at_client->end_symbol);
            offset += strlen(at_client->end_symbol);
        } else {
            break;
        }
    } while (1);

    // move remaining data to the front
    if (offset > 0) {
        if (offset == at_client->recv_buf_len) {
            at_client->recv_buf_len = 0;
            memset(at_client->recv_buf, 0, at_client->recv_buf_size);
        } else if (offset < at_client->recv_buf_len) {
            memmove(at_client->recv_buf, at_client->recv_buf + offset, at_client->recv_buf_len - offset);
            at_client->recv_buf_len -= offset;
            memset(at_client->recv_buf + at_client->recv_buf_len, 0, offset);
        }
    }

__EXIT:
    tal_mutex_unlock(at_client->mutex);

    return new_line_num;
}

static void __at_client_thread(void *arg)
{
    uint32_t delay_ms = IDLE_SLEEP_MS;

    for (;;) {
        switch (sg_at_client.status) {
        case (AT_CLIENT_STATUS_IDLE): {
            if (IDLE_SLEEP_MS != delay_ms) {
                delay_ms = IDLE_SLEEP_MS;
            }

            uint32_t available = tdl_transport_available(sg_at_client.transport_hdl);
            if (available > 0) {
                AT_CLIENT_STATUS_CHANGE(AT_CLIENT_STATUS_WAITING);
            }
        } break;
        case (AT_CLIENT_STATUS_WAITING): {
            if (WAITING_SLEEP_MS != delay_ms) {
                delay_ms = WAITING_SLEEP_MS;
            }

            __at_line_split(&sg_at_client);

            if (at_line_first_get_extra(sg_at_client.recv_line_hdl)) {
                // have urc data
                AT_CLIENT_STATUS_CHANGE(AT_CLIENT_STATUS_PROCESSING);
            }
        } break;
        case (AT_CLIENT_STATUS_PROCESSING): {
            AT_LINE_T *urc_line = at_line_get(sg_at_client.recv_line_hdl);
            URC_HANDLER urc_handler = (URC_HANDLER)at_line_first_get_extra(sg_at_client.recv_line_hdl);
            if (urc_handler) {
                urc_handler(urc_line->data, urc_line->len);
            }
        } break;
        default:
            break;
        }

        tal_system_sleep(delay_ms);

        // check thread is delete?
        if (tal_thread_get_state(sg_at_client.thread_hdl) == THREAD_STATE_DELETE) {
            PR_NOTICE("AT client thread exit");
            break;
        }
    }

    // thread will delete
    sg_at_client.thread_hdl = NULL;
}

OPERATE_RET at_client_init(TDL_TRANSPORT_HANDLE transport_hdl)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(transport_hdl, OPRT_INVALID_PARM);

    if (NULL != sg_at_client.thread_hdl) {
        PR_WARN("AT client already initialized");
        return OPRT_OK;
    }

    sg_at_client.transport_hdl = transport_hdl;

    // Initialize mutex for thread safety
    TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&sg_at_client.mutex), __ERR);

    // Initialize the AT client status
    sg_at_client.status = AT_CLIENT_STATUS_IDLE;

    // Initialize the line handle
    TUYA_CALL_ERR_GOTO(at_line_init(&sg_at_client.recv_line_hdl), __ERR);

    // Allocate receive buffer
    sg_at_client.recv_buf = AT_MALLOC(AT_CLIENT_RECV_BUF_SIZE);
    if (NULL == sg_at_client.recv_buf) {
        PR_ERR("Failed to allocate memory for AT client receive buffer");
        rt = OPRT_MALLOC_FAILED;
        goto __ERR;
    }
    memset(sg_at_client.recv_buf, 0, AT_CLIENT_RECV_BUF_SIZE);
    sg_at_client.recv_buf_size = AT_CLIENT_RECV_BUF_SIZE;
    sg_at_client.recv_buf_len = 0;

    // Initialize thread handle
    THREAD_CFG_T thrd_param = {4 * 1024, THREAD_PRIO_1, "at_client"};
    TUYA_CALL_ERR_GOTO(
        tal_thread_create_and_start(&sg_at_client.thread_hdl, NULL, NULL, __at_client_thread, NULL, &thrd_param),
        __ERR);

    PR_DEBUG("AT client initialized successfully");

    return rt;

__ERR:
    at_client_deinit();

    return rt;
}

OPERATE_RET at_client_deinit(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == sg_at_client.mutex) {
        PR_WARN(" AT client mutex is NULL, maybe not init");
        return OPRT_OK;
    }

    // lock
    tal_mutex_lock(sg_at_client.mutex);

    // delete thread
    if (NULL != sg_at_client.thread_hdl) {
        tal_thread_delete(sg_at_client.thread_hdl);
    }

    // free all recv line
    if (NULL != sg_at_client.recv_line_hdl) {
        at_line_deinit(sg_at_client.recv_line_hdl);
        sg_at_client.recv_line_hdl = NULL;
    }

    // free recv buff
    if (NULL != sg_at_client.recv_buf) {
        AT_FREE(sg_at_client.recv_buf);
        sg_at_client.recv_buf = NULL;
        sg_at_client.recv_buf_size = 0;
        sg_at_client.recv_buf_len = 0;
    }

    // unlock
    tal_mutex_unlock(sg_at_client.mutex);

    // free mutex
    tal_mutex_release(sg_at_client.mutex);
    sg_at_client.mutex = NULL;

    return rt;
}

OPERATE_RET at_client_send(const char *cmd, uint32_t len)
{
    return tdl_transport_send(sg_at_client.transport_hdl, (const uint8_t *)cmd, len);
}

OPERATE_RET at_client_add_urc_handler(AT_URC_T *urc_handler)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(sg_at_client.thread_hdl, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(urc_handler, OPRT_INVALID_PARM);

    tal_mutex_lock(sg_at_client.mutex);
    tuya_init_slist_node(&urc_handler->node);
    tuya_slist_add_head(&sg_at_client.urc_list, &urc_handler->node);
    tal_mutex_unlock(sg_at_client.mutex);

    return rt;
}
