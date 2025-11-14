/**
 * @file at_command.c
 * @brief at_command module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "at_command.h"
#include "at_client.h"

#include "tal_api.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define AT_CMD_DEBUG PR_TRACE

#define AT_CMD_CONTEXT_POOL_SIZE 8 // Maximum concurrent command contexts

#define AT_CMD_MALLOC tal_psram_malloc
#define AT_CMD_FREE   tal_psram_free

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    MUTEX_HANDLE global_mutex;                               // Global mutex for command serialization
    AT_CMD_CONTEXT_T *active_ctx;                            // Currently executing command context
    AT_CMD_CONTEXT_T context_pool[AT_CMD_CONTEXT_POOL_SIZE]; // Pre-allocated contexts
    uint32_t next_cmd_id;                                    // Next command ID
    uint8_t initialized;                                     // Whether system is initialized
} AT_CMD_MANAGER_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static AT_CMD_MANAGER_T *sg_cmd_manager = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Context pool initialization
 */
static void __context_pool_deinit(void)
{
    for (int i = 0; i < AT_CMD_CONTEXT_POOL_SIZE; i++) {
        if (sg_cmd_manager->context_pool[i].mutex) {
            tal_mutex_release(sg_cmd_manager->context_pool[i].mutex);
            sg_cmd_manager->context_pool[i].mutex = NULL;
        }
        if (sg_cmd_manager->context_pool[i].completion_sem) {
            tal_semaphore_release(sg_cmd_manager->context_pool[i].completion_sem);
            sg_cmd_manager->context_pool[i].completion_sem = NULL;
        }
    }

    memset(sg_cmd_manager->context_pool, 0, sizeof(sg_cmd_manager->context_pool));
    sg_cmd_manager->next_cmd_id = 0;

    return;
}

/**
 * @brief Context pool initialization
 */
static OPERATE_RET __context_pool_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    memset(sg_cmd_manager->context_pool, 0, sizeof(sg_cmd_manager->context_pool));
    sg_cmd_manager->next_cmd_id = 0;

    for (int i = 0; i < AT_CMD_CONTEXT_POOL_SIZE; i++) {
        TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&sg_cmd_manager->context_pool[i].mutex), __ERR);
        TUYA_CALL_ERR_GOTO(tal_semaphore_create_init(&sg_cmd_manager->context_pool[i].completion_sem, 0, 1), __ERR);
    }

    AT_CMD_DEBUG("Context pool initialized successfully");

    return rt;
__ERR:
    __context_pool_deinit();
    AT_CMD_DEBUG("Context pool initialization failed");
    return rt;
}

/**
 * @brief Initialize command context system
 * @return OPRT_OK on success, error code otherwise
 */
OPERATE_RET at_cmd_context_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == sg_cmd_manager) {
        sg_cmd_manager = AT_CMD_MALLOC(sizeof(AT_CMD_MANAGER_T));
        if (NULL == sg_cmd_manager) {
            PR_ERR("Failed to allocate memory for command manager");
            return OPRT_MALLOC_FAILED;
        }
        memset(sg_cmd_manager, 0, sizeof(AT_CMD_MANAGER_T));
    }

    if (sg_cmd_manager->initialized) {
        PR_WARN("AT command context system already initialized");
        return OPRT_OK;
    }

    TUYA_CALL_ERR_GOTO(__context_pool_init(), __ERR);

    // Initialize global mutex
    TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&sg_cmd_manager->global_mutex), __ERR);

    sg_cmd_manager->initialized = true;

    AT_CMD_DEBUG("AT command context system initialized successfully");

    return rt;

__ERR:
    at_cmd_context_deinit();
    AT_CMD_DEBUG("AT command context system initialization failed");
    return rt;
}

/**
 * @brief Deinitialize command context system
 * @return OPRT_OK on success, error code otherwise
 */
OPERATE_RET at_cmd_context_deinit(void)
{
    if (!sg_cmd_manager->initialized) {
        return OPRT_OK;
    }

    if (sg_cmd_manager->global_mutex) {
        tal_mutex_release(sg_cmd_manager->global_mutex);
        sg_cmd_manager->global_mutex = NULL;
    }

    __context_pool_deinit();

    sg_cmd_manager->initialized = false;

    AT_CMD_DEBUG("AT command context system deinitialized");

    return OPRT_OK;
}

/**
 * @brief Allocate a command context from pool
 * @return Pointer to command context, NULL on error
 */
static AT_CMD_CONTEXT_T *__alloc_context_from_pool(void)
{
    for (int i = 0; i < AT_CMD_CONTEXT_POOL_SIZE; i++) {
        if (!sg_cmd_manager->context_pool[i].in_use) {
            sg_cmd_manager->context_pool[i].in_use = true;
            return &sg_cmd_manager->context_pool[i];
        }
    }

    PR_ERR("No available context in pool");

    return NULL;
}

/**
 * @brief Free a command context to pool
 * @param ctx Command context to free
 */
static void __free_context_to_pool(AT_CMD_CONTEXT_T *ctx)
{
    if (!ctx) {
        return;
    }

    // Free dynamically allocated command string if needed
    if (ctx->cmd_str_ptr && ctx->cmd_str_ptr != ctx->cmd_str) {
        AT_CMD_FREE(ctx->cmd_str_ptr);
    }
    ctx->cmd_str_ptr = NULL;

    // Free dynamically allocated expect item list if needed
    if (ctx->expect_item_list_ptr && ctx->expect_item_list_ptr != ctx->expect_item_list) {
        AT_CMD_FREE(ctx->expect_item_list_ptr);
    }
    ctx->expect_item_list_ptr = NULL;
    ctx->expect_item_count = 0;

    // Reset context state (preserve mutex and semaphore which are managed by pool)
    ctx->cmd_id = 0;
    ctx->state = AT_CMD_STATE_IDLE;
    ctx->timeout_ms = 0;
    ctx->start_time = 0;
    ctx->validator = NULL;
    ctx->completion_cb = NULL;
    ctx->user_data = NULL;
    ctx->result = OPRT_OK;
    ctx->in_use = false;
    // Note: mutex and semaphore are preserved as they are managed by the pool
}

/**
 * @brief Create a new command context
 * @param config Command configuration
 * @return Pointer to command context, NULL on error
 */
AT_CMD_CONTEXT_T *at_cmd_create(const AT_CMD_CONFIG_T *config)
{
    if (!sg_cmd_manager->initialized) {
        PR_ERR("Command context not initialized");
        return NULL;
    }

    if (!config || !config->cmd_str) {
        PR_ERR("Invalid config");
        return NULL;
    }

    // Allocate context from pool
    AT_CMD_CONTEXT_T *ctx = __alloc_context_from_pool();
    if (!ctx) {
        PR_ERR("Failed to allocate context from pool");
        return NULL;
    }

    // Initialize context (mutex and semaphore are already initialized by pool)
    ctx->in_use = true;
    ctx->state = AT_CMD_STATE_IDLE;
    ctx->cmd_id = sg_cmd_manager->next_cmd_id++;
    ctx->timeout_ms = config->timeout_ms;
    ctx->validator = config->validator;
    ctx->completion_cb = config->completion_cb;
    ctx->user_data = config->user_data;

    // Copy command string
    size_t cmd_str_len = strlen(config->cmd_str);
    if (cmd_str_len >= AT_CMD_MAX_LENGTH) {
        // Command string is too long, allocate dynamically
        ctx->cmd_str_ptr = AT_CMD_MALLOC(cmd_str_len + 1);
        if (!ctx->cmd_str_ptr) {
            PR_ERR("Failed to allocate command string");
            __free_context_to_pool(ctx);
            return NULL;
        }
    } else {
        // Use embedded buffer
        ctx->cmd_str_ptr = ctx->cmd_str;
    }
    // Copy string and ensure null termination
    memcpy(ctx->cmd_str_ptr, config->cmd_str, cmd_str_len);
    ctx->cmd_str_ptr[cmd_str_len] = '\0';

    // Copy expect item list if provided
    if (config->expect_item_count > 0 && config->expect_item_list) {
        size_t expect_list_size = sizeof(AT_EXPECT_ITEM_T) * config->expect_item_count;
        if (config->expect_item_count > AT_CMD_MAX_EXPECT_ITEM_COUNT) {
            // Expect item count exceeds maximum, allocate dynamically
            ctx->expect_item_list_ptr = AT_CMD_MALLOC(expect_list_size);
            if (!ctx->expect_item_list_ptr) {
                PR_ERR("Failed to allocate expect item list");
                __free_context_to_pool(ctx);
                return NULL;
            }
        } else {
            // Use embedded buffer
            ctx->expect_item_list_ptr = ctx->expect_item_list;
        }
        memcpy(ctx->expect_item_list_ptr, config->expect_item_list, expect_list_size);
        ctx->expect_item_count = config->expect_item_count;
    }

    return ctx;
}

/**
 * @brief Destroy a command context
 * @param ctx Command context to destroy
 * @return OPRT_OK on success, error code otherwise
 */
OPERATE_RET at_cmd_destroy(AT_CMD_CONTEXT_T *ctx)
{
    if (!ctx || !ctx->in_use) {
        PR_ERR("Invalid context to destroy");
        return OPRT_INVALID_PARM;
    }

    __free_context_to_pool(ctx);

    return OPRT_OK;
}

/**
 * @brief Execute a command
 * @param ctx Command context to execute
 * @return OPRT_OK on success, error code otherwise
 */
OPERATE_RET at_cmd_execute_sync(AT_CMD_CONTEXT_T *ctx)
{
    OPERATE_RET rt = OPRT_OK;

    if (!ctx || !ctx->in_use) {
        PR_ERR("Invalid context");
        return OPRT_INVALID_PARM;
    }

    if (!sg_cmd_manager->initialized) {
        PR_ERR("Command context not initialized");
        return OPRT_COM_ERROR;
    }

    AT_CMD_DEBUG("Executing command #%u synchronously: %s", ctx->cmd_id, ctx->cmd_str_ptr);

    // Acquire global mutex to serialize command execution
    tal_mutex_lock(sg_cmd_manager->global_mutex);

    // Set as active command
    sg_cmd_manager->active_ctx = ctx;
    ctx->state = AT_CMD_STATE_SENDING;
    ctx->start_time = tal_system_get_millisecond();

    // Send command via AT client
    rt = at_client_send(ctx->cmd_str_ptr, strlen(ctx->cmd_str_ptr));
    if (rt != OPRT_OK) {
        PR_ERR("Failed to send command via AT client, ret=%d", rt);
        // Clear active command
        sg_cmd_manager->active_ctx = NULL;
        tal_mutex_unlock(sg_cmd_manager->global_mutex);
        return rt;
    }

    ctx->state = AT_CMD_STATE_WAITING_RESPONSE;

    // Wait for completion (with timeout)
    rt = tal_semaphore_wait(ctx->completion_sem, ctx->timeout_ms);

    // Command completed or timed out
    if (rt == OPRT_OK) {
        tal_mutex_lock(ctx->mutex);
        rt = ctx->result;
        AT_CMD_STATE_E state = ctx->state;
        tal_mutex_unlock(ctx->mutex);
        AT_CMD_DEBUG("Command #%u completed with state: %s, used %u ms", ctx->cmd_id, at_cmd_state_to_string(state),
                     tal_system_get_millisecond() - ctx->start_time);
    } else {
        PR_ERR("Command #%u timed out after %u ms", ctx->cmd_id, ctx->timeout_ms);
        // Mark command as timeout
        tal_mutex_lock(ctx->mutex);
        ctx->state = AT_CMD_STATE_TIMEOUT;
        ctx->result = OPRT_TIMEOUT;
        tal_mutex_unlock(ctx->mutex);
        rt = OPRT_TIMEOUT;
    }

    // Clear active command
    sg_cmd_manager->active_ctx = NULL;
    tal_mutex_unlock(sg_cmd_manager->global_mutex);

    return rt;
}

bool at_cmd_context_match_response(AT_CMD_CONTEXT_T *ctx, const AT_LINE_T *line)
{
    if (!ctx || !line) {
        return false;
    }

    for (uint32_t i = 0; i < ctx->expect_item_count; i++) {
        AT_EXPECT_ITEM_T *item = &ctx->expect_item_list_ptr[i];
        size_t prefix_len = strlen(item->prefix);
        if (line->len >= prefix_len && strncmp(line->data, item->prefix, prefix_len) == 0) {
            AT_CMD_DEBUG("Command #%u matched expect prefix: %s", ctx->cmd_id, item->prefix);
            // Call expect callback if provided
            if (item->callback) {
                item->callback((AT_LINE_T *)line, item->user_data);
            }
            return true;
        }
    }

    return false;
}

OPERATE_RET at_cmd_context_add_response(AT_CMD_CONTEXT_T *ctx, AT_LINE_T *line)
{
    OPERATE_RET rt = OPRT_OK;

    if (!ctx || !line) {
        return OPRT_INVALID_PARM;
    }

    if (ctx->line_count >= AT_CMD_MAX_RESPONSE_LINES) {
        PR_WARN("Command #%u response lines exceed maximum count", ctx->cmd_id);
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(ctx->mutex);

    AT_CMD_DEBUG("Command #%u adding response line %u", ctx->cmd_id, ctx->line_count);
    ctx->response_lines[ctx->line_count++] = line;

    tal_mutex_unlock(ctx->mutex);

    return rt;
}

OPERATE_RET at_cmd_context_complete(AT_CMD_CONTEXT_T *ctx)
{
    if (!ctx) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(ctx->mutex);

    ctx->state = AT_CMD_STATE_COMPLETED;
    ctx->result = OPRT_OK;

    AT_CMD_DEBUG("Command #%u marked as completed", ctx->cmd_id);

    // Validate response if validator provided
    if (ctx->validator) {
        ctx->result = ctx->validator(ctx->response_lines, ctx->line_count, ctx->user_data);
        if (ctx->result != OPRT_OK) {
            PR_WARN("Command #%u: response validation failed: %d", ctx->cmd_id, ctx->result);
            ctx->state = AT_CMD_STATE_ERROR;
        }
    }

    tal_mutex_unlock(ctx->mutex);

    // Post semaphore to wake up waiting thread
    if (ctx->completion_sem) {
        tal_semaphore_post(ctx->completion_sem);
        AT_CMD_DEBUG("Command #%u semaphore posted for completion", ctx->cmd_id);
    }

    // Call completion callback if provided
    if (ctx->completion_cb) {
        ctx->completion_cb(ctx->result, ctx->response_lines, ctx->line_count, ctx->user_data);
        AT_CMD_DEBUG("Command #%u completion callback invoked", ctx->cmd_id);
    }

    // Free all response lines
    AT_CMD_DEBUG("Command #%u freeing all response lines, count: %u", ctx->cmd_id, ctx->line_count);
    for (uint32_t i = 0; i < ctx->line_count; i++) {
        at_line_free(ctx->response_lines[i]);
        ctx->response_lines[i] = NULL;
    }
    ctx->line_count = 0;

    return OPRT_OK;
}

OPERATE_RET at_cmd_context_set_error(AT_CMD_CONTEXT_T *ctx, int error_code)
{
    if (!ctx) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(ctx->mutex);

    ctx->state = AT_CMD_STATE_ERROR;
    ctx->error_code = error_code;
    ctx->result = OPRT_COM_ERROR;

    PR_ERR("Command #%u error: code=%d", ctx->cmd_id, error_code);

    tal_mutex_unlock(ctx->mutex);

    // Post semaphore to wake up waiting thread
    if (ctx->completion_sem) {
        tal_semaphore_post(ctx->completion_sem);
        AT_CMD_DEBUG("Command #%u semaphore posted for error", ctx->cmd_id);
    }

    // Call completion callback if provided
    if (ctx->completion_cb) {
        ctx->completion_cb(ctx->result, ctx->response_lines, ctx->line_count, ctx->user_data);
        AT_CMD_DEBUG("Command #%u completion callback invoked for error", ctx->cmd_id);
    }

    // Free all response lines
    AT_CMD_DEBUG("Command #%u freeing all response lines, count: %u", ctx->cmd_id, ctx->line_count);
    for (uint32_t i = 0; i < ctx->line_count; i++) {
        at_line_free(ctx->response_lines[i]);
        ctx->response_lines[i] = NULL;
    }
    ctx->line_count = 0;

    return OPRT_OK;
}

AT_CMD_CONTEXT_T *at_cmd_get_active_context(void)
{
    return sg_cmd_manager->active_ctx;
}

const char *at_cmd_state_to_string(AT_CMD_STATE_E state)
{
    switch (state) {
    case AT_CMD_STATE_IDLE:
        return "IDLE";
    case AT_CMD_STATE_SENDING:
        return "SENDING";
    case AT_CMD_STATE_WAITING_RESPONSE:
        return "WAITING_RESPONSE";
    case AT_CMD_STATE_COMPLETED:
        return "COMPLETED";
    case AT_CMD_STATE_TIMEOUT:
        return "TIMEOUT";
    case AT_CMD_STATE_ERROR:
        return "ERROR";
    case AT_CMD_STATE_CANCELLED:
        return "CANCELLED";
    default:
        return "UNKNOWN";
    }
}

//
// Helper functions
//

OPERATE_RET at_cmd_execute_simple(const char *cmd, uint32_t timeout_ms, int *error_code)
{
    OPERATE_RET rt = OPRT_OK;
    AT_CMD_CONFIG_T config = {0};

    config.cmd_str = (char *)cmd;
    config.timeout_ms = timeout_ms;

    AT_CMD_CONTEXT_T *ctx = at_cmd_create(&config);
    if (!ctx) {
        PR_ERR("Failed to create command context for simple command");
        return OPRT_COM_ERROR;
    }
    TUYA_CALL_ERR_LOG(at_cmd_execute_sync(ctx));

    if (error_code) {
        *error_code = ctx->error_code;
    }

    at_cmd_destroy(ctx);
    ctx = NULL;

    return rt;
}

OPERATE_RET at_cmd_execute_complex(const char *cmd, AT_EXPECT_ITEM_T *expect_items, uint32_t expect_item_count,
                                   uint32_t timeout_ms, AT_RESPONSE_VALIDATOR_CB_T validator,
                                   AT_CMD_COMPLETION_CB_T completion_cb, void *user_data, int *error_code)
{
    OPERATE_RET rt = OPRT_OK;
    AT_CMD_CONFIG_T config = {0};

    config.cmd_str = (char *)cmd;
    config.expect_item_list = expect_items;
    config.expect_item_count = expect_item_count;
    config.timeout_ms = timeout_ms;
    config.validator = validator;
    config.completion_cb = completion_cb;
    config.user_data = user_data;

    AT_CMD_CONTEXT_T *ctx = at_cmd_create(&config);
    if (!ctx) {
        PR_ERR("Failed to create command context for complex command");
        return OPRT_COM_ERROR;
    }
    TUYA_CALL_ERR_LOG(at_cmd_execute_sync(ctx));

    if (error_code) {
        *error_code = ctx->error_code;
    }

    at_cmd_destroy(ctx);
    ctx = NULL;

    return rt;
}
