/**
 * @file at_command.h
 * @brief at_command module is used to execute AT commands
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __AT_COMMAND_H__
#define __AT_COMMAND_H__

#include "tuya_cloud_types.h"

#include "tal_api.h"

#include "at_line.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define AT_CMD_MAX_LENGTH            64 // Maximum length of command string
#define AT_CMD_MAX_RESPONSE_LINES    16
#define AT_CMD_MAX_EXPECT_ITEM_COUNT 5 // Maximum number of expect items

/***********************************************************
***********************typedef define***********************
***********************************************************/
/**
 * @brief Command execution state
 */
typedef enum {
    AT_CMD_STATE_IDLE,             // Command not started
    AT_CMD_STATE_SENDING,          // Command being sent
    AT_CMD_STATE_WAITING_RESPONSE, // Waiting for response
    AT_CMD_STATE_COMPLETED,        // Command completed successfully
    AT_CMD_STATE_TIMEOUT,          // Command timeout
    AT_CMD_STATE_ERROR,            // Command execution error
    AT_CMD_STATE_CANCELLED         // Command cancelled
} AT_CMD_STATE_E;

/**
 * @brief Response validation callback
 * @param lines Array of response lines
 * @param line_count Number of lines
 * @param user_data User data passed to command
 * @return OPRT_OK if response is valid, error code otherwise
 */
typedef OPERATE_RET (*AT_RESPONSE_VALIDATOR_CB_T)(AT_LINE_T **lines, uint32_t line_count, void *user_data);

/**
 * @brief Command completion callback (async mode)
 * @param result Command execution result
 * @param lines Array of response lines
 * @param line_count Number of lines
 * @param user_data User data passed to command
 */
typedef void (*AT_CMD_COMPLETION_CB_T)(OPERATE_RET result, AT_LINE_T **lines, uint32_t line_count, void *user_data);

/**
 * @brief Expect response callback - called when a specific expect prefix is matched
 * @param line The matched response line
 * @param user_data User data passed to the expect item
 */
typedef void (*AT_EXPECT_CB_T)(AT_LINE_T *line, void *user_data);

/**
 * @brief Expect item structure - defines a single expected response with its callback
 */
typedef struct {
    char *prefix;            // Expected response prefix (e.g., "+MIPRD:")
    AT_EXPECT_CB_T callback; // Callback to execute when this prefix is matched (can be NULL)
    void *user_data;         // User data passed to the callback
} AT_EXPECT_ITEM_T;

/**
 * @brief Command context structure
 */
typedef struct at_cmd_context {
    // Command identification
    uint32_t cmd_id;                 // Unique command ID
    char cmd_str[AT_CMD_MAX_LENGTH]; // Command string to send
    char *cmd_str_ptr;               // Pointer to command string
    AT_CMD_STATE_E state;            // Current state

    // Response expectation
    AT_EXPECT_ITEM_T *expect_item_list_ptr; // Pointer to expect item list
    AT_EXPECT_ITEM_T
    expect_item_list[AT_CMD_MAX_EXPECT_ITEM_COUNT]; // Array of expect items (each with prefix and callback)
    uint32_t expect_item_count;                     // Number of expect items in the array

    // Response collection
    AT_LINE_T *response_lines[AT_CMD_MAX_RESPONSE_LINES]; // Collected response lines
    uint32_t line_count;                                  // Number of lines collected

    // Synchronization (for blocking mode)
    SEM_HANDLE completion_sem; // Semaphore for blocking wait
    MUTEX_HANDLE mutex;        // Protect context data

    // Timeout management
    SYS_TIME_T timeout_ms; // Command timeout in milliseconds
    SYS_TIME_T start_time; // Command start timestamp

    // Validation and callback
    AT_RESPONSE_VALIDATOR_CB_T validator; // Optional response validator
    AT_CMD_COMPLETION_CB_T completion_cb; // Optional completion callback (async)
    void *user_data;                      // User data passed to callbacks (e.g., command parameters)

    // Result
    OPERATE_RET result; // Execution result
    int error_code;     // Error code (for +CME ERROR)

    // Internal
    bool in_use; // Whether this context is in use
} AT_CMD_CONTEXT_T;

/**
 * @brief Command context configuration
 */
typedef struct {
    uint32_t cmd_id;                      // Unique command ID
    char *cmd_str;                        // Command string to send
    AT_EXPECT_ITEM_T *expect_item_list;   // Array of expect items (each with prefix and callback)
    uint32_t expect_item_count;           // Number of expect items in the array
    uint32_t timeout_ms;                  // Command timeout in milliseconds
    AT_RESPONSE_VALIDATOR_CB_T validator; // Optional response validator
    AT_CMD_COMPLETION_CB_T completion_cb; // Optional completion callback (async)
    void *user_data;                      // User data passed to callbacks (e.g., command parameters)
} AT_CMD_CONFIG_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Initialize command context system
 * @return OPRT_OK on success, error code otherwise
 */
OPERATE_RET at_cmd_context_init(void);

/**
 * @brief Deinitialize command context system
 * @return OPRT_OK on success, error code otherwise
 */
OPERATE_RET at_cmd_context_deinit(void);

/**
 * @brief Create a new command context
 * @param config Command configuration
 * @return Pointer to command context, NULL on error
 */
AT_CMD_CONTEXT_T *at_cmd_create(const AT_CMD_CONFIG_T *config);

/**
 * @brief Destroy a command context
 * @param ctx Command context to destroy
 * @return OPRT_OK on success, error code otherwise
 */
OPERATE_RET at_cmd_destroy(AT_CMD_CONTEXT_T *ctx);

/**
 * @brief Execute a command
 * @param ctx Command context to execute
 * @return OPRT_OK on success, error code otherwise
 */
OPERATE_RET at_cmd_execute_sync(AT_CMD_CONTEXT_T *ctx);

/**
 * @brief Check if a response line matches any expect item in the command context
 *
 * This function checks if the given response line starts with any of the
 * expected prefixes defined in the command context's expect item list.
 * If a match is found, the corresponding callback is invoked.
 *
 * @param ctx Command context
 * @param line Response line to check
 * @return true if a match was found and callback invoked, false otherwise
 */
bool at_cmd_context_match_response(AT_CMD_CONTEXT_T *ctx, const AT_LINE_T *line);

/**
 * @brief Add a response line to the command context
 *
 * This function adds a received response line to the command context's
 * collection of response lines. It is typically called by the AT client
 * when a new line is received from the modem.
 *
 * @param ctx Command context
 * @param line Response line to add
 * @return OPRT_OK on success, error code otherwise
 */
OPERATE_RET at_cmd_context_add_response(AT_CMD_CONTEXT_T *ctx, AT_LINE_T *line);

/**
 * @brief Mark a command context as completed successfully
 *
 * This function is typically called by the AT client when it receives
 * a successful response terminator (e.g., "OK") from the modem.
 * It updates the command state to AT_CMD_STATE_COMPLETED and sets
 * the result to OPRT_OK.
 *
 * @param ctx Command context to mark as completed
 * @return OPRT_OK on success, OPRT_INVALID_PARM if ctx is NULL
 */
OPERATE_RET at_cmd_context_complete(AT_CMD_CONTEXT_T *ctx);

/**
 * @brief Mark a command context as errored
 *
 * This function is typically called by the AT client when it receives
 * an error response terminator (e.g., "ERROR" or "+CME ERROR") from the modem.
 * It updates the command state to AT_CMD_STATE_ERROR, sets the error code,
 * and sets the result to OPRT_COM_ERROR.
 *
 * @param ctx Command context to mark as errored
 * @param error_code Error code associated with the failure
 * @return OPRT_OK on success, OPRT_INVALID_PARM if ctx is NULL
 */
OPERATE_RET at_cmd_context_set_error(AT_CMD_CONTEXT_T *ctx, int error_code);

/**
 * @brief Get the current active command context
 *
 * This is used internally by the AT client to route responses.
 *
 * @return Pointer to active command context, NULL if no command is active
 */
AT_CMD_CONTEXT_T *at_cmd_get_active_context(void);

/**
 * @brief Get command state as string (for debugging)
 * @param state Command state
 * @return String representation of state
 */
const char *at_cmd_state_to_string(AT_CMD_STATE_E state);

/**
 * @brief Helper: Execute a simple AT command and wait for OK
 *
 * Convenience function for simple commands that just need OK response.
 *
 * @param cmd Command string (e.g., "AT")
 * @param timeout_ms Timeout in milliseconds (0 = default)
 * @param error_code Returned error code from command, if any (can be NULL)
 * @return OPRT_OK on success, error code otherwise
 */
OPERATE_RET at_cmd_execute_simple(const char *cmd, uint32_t timeout_ms, int *error_code);

/**
 * @brief Helper: Execute a complex AT command with expect items and optional validator
 *
 * @param cmd Command string (e.g., "AT+MIPRD?")
 * @param expect_items Array of expect items defining expected responses and callbacks
 * @param expect_item_count Number of expect items in the array
 * @param timeout_ms Timeout in milliseconds (0 = default)
 * @param validator Optional response validator callback (can be NULL)
 * @param completion_cb Optional completion callback for async mode (can be NULL)
 * @param user_data User data passed to callbacks
 * @param error_code Returned error code from command, if any (can be NULL)
 * @return OPRT_OK on success, error code otherwise
 */
OPERATE_RET at_cmd_execute_complex(const char *cmd, AT_EXPECT_ITEM_T *expect_items, uint32_t expect_item_count,
                                   uint32_t timeout_ms, AT_RESPONSE_VALIDATOR_CB_T validator,
                                   AT_CMD_COMPLETION_CB_T completion_cb, void *user_data, int *error_code);

#ifdef __cplusplus
}
#endif

#endif /* __AT_COMMAND_H__ */
