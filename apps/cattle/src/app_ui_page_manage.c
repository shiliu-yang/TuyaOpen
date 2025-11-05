/**
 * @file app_ui_page_manage.c
 * @brief Simple and robust UI page management system implementation
 * @version 1.0
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "app_ui_page_manage.h"
#include "ui.h"

#include "tal_api.h"

#include "tuya_lvgl.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/
static void page_stack_init(app_page_stack_t *stack);
static OPERATE_RET page_stack_push(app_page_stack_t *stack, app_page_t *page);
static OPERATE_RET page_stack_pop(app_page_stack_t *stack);
static BOOL_T page_stack_is_empty(const app_page_stack_t *stack);
static app_page_t *page_stack_top(app_page_stack_t *stack);

/***********************************************************
***********************variable define**********************
***********************************************************/
static app_page_stack_t s_page_stack = {0};
static MUTEX_HANDLE s_page_mutex = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Initialize page stack
 */
static void page_stack_init(app_page_stack_t *stack)
{
    stack->top = 0;
}

/**
 * @brief Push page to stack
 */
static OPERATE_RET page_stack_push(app_page_stack_t *stack, app_page_t *page)
{
    if (stack->top >= PAGE_STACK_MAX_DEPTH) {
        PR_ERR("Page stack is full");
        return OPRT_COM_ERROR;
    }
    stack->pages[stack->top++] = page;
    return OPRT_OK;
}

/**
 * @brief Pop page from stack
 */
static OPERATE_RET page_stack_pop(app_page_stack_t *stack)
{
    if (stack->top <= 0) {
        PR_ERR("Page stack is empty");
        return OPRT_COM_ERROR;
    }

    // // Deinitialize the current page before popping
    // app_page_t *current_page = stack->pages[stack->top - 1];
    // if (current_page && current_page->deinit) {
    //     current_page->deinit();
    // }

    stack->top--;
    return OPRT_OK;
}

/**
 * @brief Check if page stack is empty
 */
static BOOL_T page_stack_is_empty(const app_page_stack_t *stack)
{
    return stack->top == 0;
}

/**
 * @brief Get top page from stack
 */
static app_page_t *page_stack_top(app_page_stack_t *stack)
{
    if (stack->top == 0) {
        return NULL;
    }
    return stack->pages[stack->top - 1];
}

/**
 * @brief Initialize the page manager
 */
void app_pages_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    page_stack_init(&s_page_stack);

    // Create mutex for thread safety
    TUYA_CALL_ERR_LOG(tal_mutex_create_init(&s_page_mutex));

    PR_DEBUG("Page manager initialized with thread safety");
}

/**
 * @brief Deinitialize the page manager and release resources
 */
void app_pages_deinit(void)
{
    if (!s_page_mutex) {
        PR_DEBUG("Page manager already deinitialized");
        return;
    }

    tal_mutex_lock(s_page_mutex);

    // Deinitialize all pages in stack
    while (s_page_stack.top > 0) {
        // app_page_t *current_page = page_stack_top(&s_page_stack);
        // if (current_page && current_page->deinit) {
        //     current_page->deinit();
        // }
        s_page_stack.top--;
    }

    // Clear stack
    page_stack_init(&s_page_stack);

    tal_mutex_unlock(s_page_mutex);

    // Release mutex
    tal_mutex_release(s_page_mutex);
    s_page_mutex = NULL;

    PR_DEBUG("Page manager deinitialized");
}

/**
 * @brief Load initial page safely (for empty stack)
 */
void app_page_load_initial(app_page_t *initial_page)
{
    if (!initial_page) {
        PR_ERR("Invalid initial page parameter");
        return;
    }

    if (!s_page_mutex) {
        PR_ERR("Page manager not initialized");
        return;
    }

    tal_mutex_lock(s_page_mutex);

    // Clear stack first to ensure clean state
    page_stack_init(&s_page_stack);

    // Push initial page to stack
    if (page_stack_push(&s_page_stack, initial_page) != OPRT_OK) {
        PR_ERR("Failed to push initial page to stack");
        tal_mutex_unlock(s_page_mutex);
        return;
    }

    // Initialize initial page
    if (initial_page->init) {
        tuya_lvgl_mutex_lock();
        initial_page->init();
        tuya_lvgl_mutex_unlock();
    }

    // Load page UI without animation for initial page
    if (initial_page->page_obj && *initial_page->page_obj) {
        tuya_lvgl_mutex_lock();
        lv_scr_load(*initial_page->page_obj);
        tuya_lvgl_mutex_unlock();
    } else {
        PR_ERR("Initial page object is NULL");
        s_page_stack.top = 0; // Clear stack
    }

    PR_DEBUG("Initial page loaded, stack depth: %d", s_page_stack.top);
    tal_mutex_unlock(s_page_mutex);
}

/**
 * @brief Load a new page to stack top
 */
void app_page_load(app_page_t *new_page, lv_screen_load_anim_t anim_type, uint8_t need_lvgl_mutex)
{
    PR_DEBUG("Loading new page..., need_lvgl_mutex: %d", need_lvgl_mutex);

    if (!new_page) {
        PR_ERR("Invalid page parameter");
        return;
    }

    if (!s_page_mutex) {
        PR_ERR("Page manager not initialized");
        return;
    }

    tal_mutex_lock(s_page_mutex);

    // Check if stack is empty - should use app_page_load_initial instead
    if (s_page_stack.top == 0) {
        PR_WARN("Stack is empty, use app_page_load_initial() for first page");
        tal_mutex_unlock(s_page_mutex);
        return;
    }

    // Check if stack is full
    if (s_page_stack.top >= PAGE_STACK_MAX_DEPTH) {
        PR_ERR("Page stack is full");
        tal_mutex_unlock(s_page_mutex);
        return;
    }

    // lv_scr_load_anim will auto delete old screen
    // // Deinitialize current page if exists
    // if (s_page_stack.top > 0) {
    //     app_page_t *current_page = page_stack_top(&s_page_stack);
    //     if (current_page && current_page->deinit) {
    //         current_page->deinit();
    //     }
    // }

    // Push new page to stack
    if (page_stack_push(&s_page_stack, new_page) != OPRT_OK) {
        PR_ERR("Failed to push page to stack");
        tal_mutex_unlock(s_page_mutex);
        return;
    }

    // Initialize new page
    if (new_page->init) {
        if (need_lvgl_mutex) {
            tuya_lvgl_mutex_lock();
        }
        new_page->init();
        if (need_lvgl_mutex) {
            tuya_lvgl_mutex_unlock();
        }
    }

    // Load page UI with animation
    if (new_page->page_obj && *new_page->page_obj) {
        // Apply animation here if using LVGL
        if (need_lvgl_mutex) {
            tuya_lvgl_mutex_lock();
        }
        if (anim_type == LV_SCR_LOAD_ANIM_NONE) {
            lv_scr_load(*new_page->page_obj);
        } else {
            lv_scr_load_anim(*new_page->page_obj, anim_type, 300, 0, false);
        }
        if (need_lvgl_mutex) {
            tuya_lvgl_mutex_unlock();
        }
    } else {
        PR_ERR("Page object is NULL, cannot load page");
        // Pop the page back out since we can't display it
        if (s_page_stack.top > 0) {
            s_page_stack.top--;
        }
    }

    PR_DEBUG("Page loaded, stack depth: %d", s_page_stack.top);
    tal_mutex_unlock(s_page_mutex);
}

/**
 * @brief Go back to previous page
 */
void app_page_back(lv_screen_load_anim_t anim_type, uint8_t need_lvgl_mutex)
{
    if (!s_page_mutex) {
        PR_ERR("Page manager not initialized");
        return;
    }

    tal_mutex_lock(s_page_mutex);

    if (page_stack_is_empty(&s_page_stack)) {
        PR_WARN("Page stack is empty, cannot go back");
        tal_mutex_unlock(s_page_mutex);
        return;
    }

    // Pop current page
    if (page_stack_pop(&s_page_stack) != OPRT_OK) {
        PR_ERR("Failed to pop page from stack");
        tal_mutex_unlock(s_page_mutex);
        return;
    }

    // If stack becomes empty, handle appropriately
    if (page_stack_is_empty(&s_page_stack)) {
        PR_DEBUG("Stack is now empty");
        tal_mutex_unlock(s_page_mutex);
        return;
    }

    // Initialize and show previous page
    app_page_t *prev_page = page_stack_top(&s_page_stack);
    if (prev_page) {
        if (prev_page->init) {
            if (need_lvgl_mutex) {
                tuya_lvgl_mutex_lock();
            }
            prev_page->init();
            if (need_lvgl_mutex) {
                tuya_lvgl_mutex_unlock();
            }
        }

        // Load previous page UI with animation
        if (prev_page->page_obj && *prev_page->page_obj) {
            // Apply animation here if using LVGL
            if (need_lvgl_mutex) {
                tuya_lvgl_mutex_lock();
            }
            if (anim_type == LV_SCR_LOAD_ANIM_NONE) {
                lv_scr_load(*prev_page->page_obj);
            } else {
                lv_scr_load_anim(*prev_page->page_obj, anim_type, 300, 0, false);
            }
            if (need_lvgl_mutex) {
                tuya_lvgl_mutex_unlock();
            }
        } else {
            PR_ERR("Previous page object is NULL, cannot display page");
        }
    }

    PR_DEBUG("Went back, stack depth: %d", s_page_stack.top);
    tal_mutex_unlock(s_page_mutex);
}

/**
 * @brief Go back to bottom page (home page)
 */
void app_page_back_to_bottom(lv_screen_load_anim_t anim_type, uint8_t need_lvgl_mutex)
{
    if (!s_page_mutex) {
        PR_ERR("Page manager not initialized");
        return;
    }

    tal_mutex_lock(s_page_mutex);

    if (page_stack_is_empty(&s_page_stack)) {
        PR_WARN("Page stack is empty");
        tal_mutex_unlock(s_page_mutex);
        return;
    }

    // Pop all pages except the bottom one
    while (s_page_stack.top > 1) {
        if (page_stack_pop(&s_page_stack) != OPRT_OK) {
            PR_ERR("Failed to pop page from stack");
            break;
        }
    }

    // Initialize and show bottom page
    app_page_t *bottom_page = page_stack_top(&s_page_stack);
    if (bottom_page) {
        if (bottom_page->init) {
            if (need_lvgl_mutex) {
                tuya_lvgl_mutex_lock();
            }
            bottom_page->init();
            if (need_lvgl_mutex) {
                tuya_lvgl_mutex_unlock();
            }
        }

        // Load bottom page UI with animation
        if (bottom_page->page_obj && *bottom_page->page_obj) {
            // Apply animation here if using LVGL
            if (need_lvgl_mutex) {
                tuya_lvgl_mutex_lock();
            }
            if (anim_type == LV_SCR_LOAD_ANIM_NONE) {
                lv_scr_load(*bottom_page->page_obj);
            } else {
                lv_scr_load_anim(*bottom_page->page_obj, anim_type, 300, 0, false);
            }
            if (need_lvgl_mutex) {
                tuya_lvgl_mutex_unlock();
            }
        } else {
            PR_ERR("Bottom page object is NULL, cannot display page");
        }
    }

    PR_DEBUG("Returned to bottom page, stack depth: %d", s_page_stack.top);
    tal_mutex_unlock(s_page_mutex);
}

/**
 * @brief Get current active page
 */
app_page_t *app_page_get_current(void)
{
    if (!s_page_mutex) {
        return NULL;
    }

    tal_mutex_lock(s_page_mutex);
    app_page_t *current = page_stack_top(&s_page_stack);
    tal_mutex_unlock(s_page_mutex);

    return current;
}

/**
 * @brief Get current stack depth
 */
uint8_t app_page_get_stack_depth(void)
{
    if (!s_page_mutex) {
        return 0;
    }

    tal_mutex_lock(s_page_mutex);
    uint8_t depth = s_page_stack.top;
    tal_mutex_unlock(s_page_mutex);

    return depth;
}

/**
 * @brief Check if page stack is empty
 */
BOOL_T app_page_is_empty(void)
{
    if (!s_page_mutex) {
        return TRUE;
    }

    tal_mutex_lock(s_page_mutex);
    BOOL_T empty = page_stack_is_empty(&s_page_stack);
    tal_mutex_unlock(s_page_mutex);

    return empty;
}
