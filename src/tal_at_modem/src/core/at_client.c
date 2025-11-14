/**
 * @file at_client.c
 * @brief at_client module is used to
 * @version 0.1
 * @date 2025-11-13
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "at_client.h"
#include "at_line.h"
#include "at_command.h"

#include "tal_api.h"

#include "tuya_ringbuf.h"

/***********************************************************
************************macro define************************
***********************************************************/
// debug
#define AT_CLIENT_DEBUG PR_TRACE

// memory manage
#define AT_MALLOC tal_psram_malloc
#define AT_FREE   tal_psram_free

// tdl transport receive buffer size
#define AT_CLIENT_RECV_BUF_SIZE (16 * 1024) // 16KB

// thread sleep time ms
#define IDLE_SLEEP_MS 30
#define BUSY_SLEEP_MS 10

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    THREAD_HANDLE thread_hdl;

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
    MUTEX_HANDLE urc_mutex;
} AT_CLIENT_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static AT_CLIENT_T sg_at_client = {0};

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Check if line matches any registered URC
 */
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

    static uint32_t last_available = 0;
    static uint32_t wait_cnt = 0;

    uint32_t available = tdl_transport_available(at_client->transport_hdl);

    // wait data recv over
    if (last_available != available) {
        AT_CLIENT_DEBUG("AT client avaliable %d -> %d bytes", last_available, available);
        last_available = available;
        wait_cnt = 0;
        return 0;
    }
    wait_cnt++;

    if (wait_cnt * BUSY_SLEEP_MS <= 30 || available == 0) {
        // Waiting for data to be fully received
        // AT_CLIENT_DEBUG("Waiting for data to be fully received, wait_cnt: %d, available: %d", wait_cnt, available);
        return 0;
    }
    AT_CLIENT_DEBUG("Get data");

    // Reset static variables after data is stable
    wait_cnt = 0;
    last_available = 0;

    uint8_t *p_recv = at_client->recv_buf + at_client->recv_buf_len;
    uint32_t free_space = at_client->recv_buf_size - at_client->recv_buf_len;

    if (free_space == 0) {
        // no space to receive data
        AT_CLIENT_DEBUG("AT client recv buffer full, size: %d", at_client->recv_buf_size);
        return 0;
    }

    // read new data
    uint32_t read_len = tdl_transport_read(at_client->transport_hdl, p_recv, free_space);
    if (read_len > 0) {
        at_client->recv_buf_len += read_len;
        AT_CLIENT_DEBUG("--> Read %d bytes from transport", read_len);
        AT_CLIENT_DEBUG("--> Received data: %.*s", read_len > 100 ? 100 : read_len, p_recv);
        // PR_HEXDUMP_DEBUG("AT client recv data", p_recv, read_len);
    } else {
        // no data read
        return 0;
    }

    // get new line
    char *p_start = (char *)at_client->recv_buf;
    char *p_end = NULL;
    uint32_t offset = 0;
    uint32_t end_symbol_len = strlen(at_client->end_symbol); // Cache length to avoid repeated strlen calls

    do {
        p_end = strstr(p_start, at_client->end_symbol);
        if (p_end && p_end > p_start) {
            // Found a valid line
            new_line = NULL;
            // PR_HEXDUMP_DEBUG("AT client new line", (uint8_t *)p_start, p_end - p_start);
            AT_CLIENT_DEBUG("--> Received line: %.*s", (p_end - p_start) > 1024 ? 1024 : (p_end - p_start), p_start);
            new_line = at_line_create(p_start, p_end - p_start);
            if (NULL == new_line) {
                PR_ERR("Failed to create new AT line");
                goto __EXIT;
            }
            void *p_extra = (void *)__at_line_get_urc_handler(&at_client->urc_list, new_line);
            if (p_extra) {
                AT_CLIENT_DEBUG("AT client new line is URC, handler: %p", p_extra);
            } else {
                AT_CLIENT_DEBUG("AT client new line is not URC");
            }
            at_line_set_extra(at_client->recv_line_hdl, new_line, p_extra);
            at_line_add(at_client->recv_line_hdl, new_line);
            new_line_num++;
            offset += p_end - p_start + end_symbol_len;
            p_start = p_end + end_symbol_len;
            // Continue to process next line instead of break
        } else if (p_end == p_start) {
            // empty line, skip it
            p_start += end_symbol_len;
            offset += end_symbol_len;
            AT_CLIENT_DEBUG("AT client empty line");
        } else {
            // no more complete line
            // AT_CLIENT_DEBUG("No more complete line");
            break;
        }
    } while (1);

    // move remaining data to the front
    if (offset > 0) {
        if (offset == at_client->recv_buf_len) {
            // All data processed
            at_client->recv_buf_len = 0;
            memset(at_client->recv_buf, 0, offset);
        } else if (offset < at_client->recv_buf_len) {
            // Move remaining data to the front
            memmove(at_client->recv_buf, at_client->recv_buf + offset, at_client->recv_buf_len - offset);
            at_client->recv_buf_len -= offset;
            at_client->recv_buf[at_client->recv_buf_len] = 0;
        }
    }

__EXIT:
    return new_line_num;
}

/**
 * @brief Check if line is a command terminator (OK, ERROR, +CME ERROR)
 */
static bool __is_command_terminator(const char *line_data)
{
    return (strcmp(line_data, "OK") == 0 || strcmp(line_data, "ERROR") == 0 ||
            strncmp(line_data, "+CME ERROR:", 11) == 0);
}

/**
 * @brief Extract error code from +CME ERROR response
 */
static int __extract_cme_error_code(const char *line_data)
{
    if (strncmp(line_data, "+CME ERROR:", 11) == 0) {
        return atoi(line_data + 11);
    }
    return 0;
}

static void __at_line_process(AT_CLIENT_T *at_client)
{
    OPERATE_RET rt = OPRT_OK;

    AT_LINE_T *at_line = at_line_get(at_client->recv_line_hdl);
    if (at_line == NULL) {
        return;
    }

    AT_CMD_CONTEXT_T *active_ctx = at_cmd_get_active_context();

    if (active_ctx) {
        if (__is_command_terminator(at_line->data)) {
            // 1. Check if it's a terminator (OK/ERROR)
            if (strcmp(at_line->data, "OK") == 0) {
                at_cmd_context_complete(active_ctx);
            } else if (strncmp(at_line->data, "+CME ERROR:", 11) == 0) {
                int error_code = __extract_cme_error_code(at_line->data);
                at_cmd_context_set_error(active_ctx, error_code);
            } else {
                at_cmd_context_set_error(active_ctx, 0);
            }
        } else if (at_cmd_context_match_response(active_ctx, at_line)) {
            // 2. Check if it's the expected response for this command
            // add to response lines
            rt = at_cmd_context_add_response(active_ctx, at_line);
            if (rt != OPRT_OK) {
                PR_ERR("Failed to add response line to command context, freeing line");
                at_line_free(at_line);
                at_line = NULL;
            }
        } else if (at_line->extra) {
            // 3. Check if it's a URC (can arrive during command execution)
            URC_HANDLER urc_handler = (URC_HANDLER)(at_line->extra);
            urc_handler(at_line->data, at_line->len);
            at_line_free(at_line);
            at_line = NULL;
        } else {
            // 4. Unknown response during command - log warning and discard
            PR_WARN("AT client: unexpected line during command: %.*s", (at_line->len > 64 ? 64 : at_line->len),
                    at_line->data);
            at_line_free(at_line);
            at_line = NULL;
        }
    } else {
        if (at_line->extra) {
            URC_HANDLER urc_handler = (URC_HANDLER)(at_line->extra);
            urc_handler(at_line->data, at_line->len);
            at_line_free(at_line);
            at_line = NULL;
        } else {
            // No active command - unknown line, discard
            PR_WARN("AT client: unexpected line with no active command: %.*s", (at_line->len > 64 ? 64 : at_line->len),
                    at_line->data);
            at_line_free(at_line);
            at_line = NULL;
        }
    }
    return;
}

static void __at_client_thread(void *arg)
{
    uint32_t delay_ms = IDLE_SLEEP_MS;
    uint32_t available = 0;
    uint32_t split_num = 0;

    while (1) {
        // check transport data available
        available = tdl_transport_available(sg_at_client.transport_hdl);
        if (available == 0) {
            // no data, sleep
            delay_ms = IDLE_SLEEP_MS;
            goto __SLEEP;
        } else {
            delay_ms = BUSY_SLEEP_MS;
        }

        // slip the data to the line handle
        split_num = __at_line_split(&sg_at_client);

        // Response routing and URC handling - process all available lines
        while (at_line_get_count(sg_at_client.recv_line_hdl) > 0) {
            __at_line_process(&sg_at_client);
        }

    __SLEEP:
        tal_system_sleep(delay_ms);

        // check thread is delete
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

    // set default end symbol
    memset(sg_at_client.end_symbol, 0, sizeof(sg_at_client.end_symbol));
    snprintf(sg_at_client.end_symbol, sizeof(sg_at_client.end_symbol), LINE_END_SYMBOL_CRLF);

    // Initialize at command system
    TUYA_CALL_ERR_GOTO(at_cmd_context_init(), __ERR);

    // Initialize URC mutex
    TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&sg_at_client.urc_mutex), __ERR);

    // Initialize the line handle
    TUYA_CALL_ERR_GOTO(at_line_init(&sg_at_client.recv_line_hdl), __ERR);

    // Allocate receive buffer
    sg_at_client.recv_buf = (uint8_t *)AT_MALLOC(AT_CLIENT_RECV_BUF_SIZE);
    if (NULL == sg_at_client.recv_buf) {
        PR_ERR("Failed to allocate memory for AT client receive buffer");
        rt = OPRT_MALLOC_FAILED;
        goto __ERR;
    }
    sg_at_client.recv_buf_size = AT_CLIENT_RECV_BUF_SIZE;
    sg_at_client.recv_buf_len = 0;

    // Initialize thread handle
    THREAD_CFG_T thrd_param = {6 * 1024, THREAD_PRIO_1, "at_client"};
    TUYA_CALL_ERR_GOTO(
        tal_thread_create_and_start(&sg_at_client.thread_hdl, NULL, NULL, __at_client_thread, NULL, &thrd_param),
        __ERR);

    AT_CLIENT_DEBUG("AT client initialized successfully");

    return rt;

__ERR:
    at_client_deinit();
    AT_CLIENT_DEBUG("AT client initialization failed");
    return rt;
}

OPERATE_RET at_client_deinit(void)
{
    OPERATE_RET rt = OPRT_OK;

    // delete thread
    if (NULL != sg_at_client.thread_hdl) {
        tal_thread_delete(sg_at_client.thread_hdl);
    }

    // wait thread exit
    while (NULL != sg_at_client.thread_hdl) {
        tal_system_sleep(10);
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

    // free URC mutex
    if (NULL != sg_at_client.urc_mutex) {
        TUYA_CALL_ERR_LOG(tal_mutex_release(sg_at_client.urc_mutex));
        sg_at_client.urc_mutex = NULL;
    }

    // deinit at command system
    at_cmd_context_deinit();

    return OPRT_OK;
}

OPERATE_RET at_client_add_urc_handler(AT_URC_T *urc_handler)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(urc_handler, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(sg_at_client.urc_mutex, OPRT_INVALID_PARM);

    tal_mutex_lock(sg_at_client.urc_mutex);
    tuya_init_slist_node(&urc_handler->node);
    tuya_slist_add_head(&sg_at_client.urc_list, &urc_handler->node);
    tal_mutex_unlock(sg_at_client.urc_mutex);

    return rt;
}

OPERATE_RET at_client_send(const char *cmd, uint32_t len)
{
    return tdl_transport_send(sg_at_client.transport_hdl, (const uint8_t *)cmd, len);
}
