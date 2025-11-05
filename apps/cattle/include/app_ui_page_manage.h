/**
 * @file app_ui_page_manage.h
 * @brief Simple and robust UI page management system
 * @version 1.0
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __APP_UI_PAGE_MANAGE_H__
#define __APP_UI_PAGE_MANAGE_H__

#include "tuya_cloud_types.h"
#include "ui.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define PAGE_STACK_MAX_DEPTH 6 /**< Maximum page stack depth */

/***********************************************************
***********************typedef define***********************
***********************************************************/

/**
 * @brief Page structure (similar to original PageManager)
 */
typedef struct {
    void (*init)(void);   /**< Page initialization function */
    void (*deinit)(void); /**< Page deinitialization function */
    lv_obj_t **page_obj;  /**< Page UI object pointer (e.g., lv_obj_t**) */
} app_page_t;

/**
 * @brief Page stack structure
 */
typedef struct {
    app_page_t *pages[PAGE_STACK_MAX_DEPTH]; /**< Page stack */
    uint8_t top;                             /**< Stack top index */
} app_page_stack_t;

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Initialize the page manager
 */
void app_pages_init(void);

/**
 * @brief Deinitialize the page manager and release resources
 */
void app_pages_deinit(void);

/**
 * @brief Load initial page safely (for empty stack)
 * @param initial_page Pointer to the initial page
 */
void app_page_load_initial(app_page_t *initial_page);

/**
 * @brief Load a new page to stack top
 * @param new_page Pointer to the new page
 */
void app_page_load(app_page_t *new_page, lv_screen_load_anim_t anim_type, uint8_t need_lvgl_mutex);

/**
 * @brief Go back to previous page
 */
void app_page_back(lv_screen_load_anim_t anim_type, uint8_t need_lvgl_mutex);

/**
 * @brief Go back to bottom page (home page)
 */
void app_page_back_to_bottom(lv_screen_load_anim_t anim_type, uint8_t need_lvgl_mutex);

/**
 * @brief Get current active page
 * @return Pointer to current page, NULL if stack is empty
 */
app_page_t *app_page_get_current(void);

/**
 * @brief Get current stack depth
 * @return Current stack depth
 */
uint8_t app_page_get_stack_depth(void);

/**
 * @brief Check if page stack is empty
 * @return TRUE if empty, FALSE otherwise
 */
BOOL_T app_page_is_empty(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_UI_PAGE_MANAGE_H__ */
