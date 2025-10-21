/**
 * @file example_lvgl.c
 * @brief LVGL (Light and Versatile Graphics Library) example for SDK.
 *
 * This file provides an example implementation of using the LVGL library with the Tuya SDK.
 * It demonstrates the initialization and usage of LVGL for graphical user interface (GUI) development.
 * The example covers setting up the display port, initializing LVGL, and running a demo application.
 *
 * The LVGL example aims to help developers understand how to integrate LVGL into their Tuya IoT projects for
 * creating graphical user interfaces on embedded devices. It includes detailed examples of setting up LVGL,
 * handling display updates, and integrating these functionalities within a multitasking environment.
 *
 * @note This example is designed to be adaptable to various Tuya IoT devices and platforms, showcasing fundamental LVGL
 * operations critical for GUI development on embedded systems.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_spi.h"
#include "tkl_system.h"

#include "lvgl.h"
#include "demos/lv_demos.h"
#include "examples/lv_examples.h"
#include "lv_vendor.h"
#include "board_com_api.h"
/***********************************************************
*************************micro define***********************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/


// Declare external font defined in font_puhui_18_2.c
extern lv_font_t font_puhui_18_2;

// Example usage: set as default font for LVGL (call in your UI init or main)
void example_set_font(void) {
    lv_obj_set_style_text_font(lv_scr_act(), &font_puhui_18_2, LV_PART_MAIN | LV_STATE_DEFAULT);
}

#include "lvgl/lvgl.h"
#include <math.h>

void create_circular_compass(lv_obj_t *parent) {
    /* 1. Create Circular Container */
    lv_obj_t *compass = lv_obj_create(parent);
    lv_obj_set_size(compass, 466, 466);
    lv_obj_set_align(compass, LV_ALIGN_CENTER);
    lv_obj_set_style_radius(compass, LV_RADIUS_CIRCLE, 0); // Make it a perfect circle
    lv_obj_set_style_bg_color(compass, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(compass, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(compass, 0, 0);

    /* Center Coordinates */
    int16_t center_x = 466 / 2;
    int16_t center_y = 466 / 2;
    int16_t tick_radius = 220;   // Radius for tick lines
    int16_t text_radius = 200;   // Radius for degree labels
    int16_t cardinal_text_radius = 150; // Radius for cardinal directions

    /* 2. Create tick lines using LVGL objects */
    for (int angle = 0; angle < 360; angle += 5) { // Every 5 degrees for performance
        double rad = angle * M_PI / 180.0;
        int16_t x1 = center_x + cos(rad) * tick_radius;
        int16_t y1 = center_y - sin(rad) * tick_radius;
        int16_t x2, y2;

        if (angle % 30 == 0) {
            // Long tick for every 30 degrees
            x2 = center_x + cos(rad) * (tick_radius - 10);
            y2 = center_y - sin(rad) * (tick_radius - 10);
        } else {
            // Short tick for other degrees
            x2 = center_x + cos(rad) * (tick_radius - 5);
            y2 = center_y - sin(rad) * (tick_radius - 5);
        }

        // Create line object
        lv_obj_t *line = lv_line_create(compass);
        lv_point_precise_t points[2] = {{x1, y1}, {x2, y2}};
        lv_line_set_points(line, points, 2);
        lv_obj_set_style_line_color(line, lv_color_white(), 0);
        lv_obj_set_style_line_width(line, 1, 0);
        lv_obj_set_style_line_opa(line, LV_OPA_COVER, 0);
    }

    /* 3. Create degree labels */
    for (int angle = 0; angle < 360; angle += 30) {
        double rad = angle * M_PI / 180.0;
        int16_t label_x = center_x + cos(rad) * text_radius;
        int16_t label_y = center_y - sin(rad) * text_radius;

        lv_obj_t *label = lv_label_create(compass);
        char buf[4];
        sprintf(buf, "%d", angle);
        lv_label_set_text(label, buf);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(label, label_x - 10, label_y - 6); // Center the label
    }

    /* 4. Create Cardinal Directions (北, 东, 南, 西) */
    const char *cardinals[4] = {"北", "东", "南", "西"};
    // const char *cardinals[4] = {"N", "E", "S", "W"};
    int cardinal_angles[4] = {0, 90, 180, 270};

    for (int i = 0; i < 4; i++) {
        double rad = cardinal_angles[i] * M_PI / 180.0;
        int16_t label_x = center_x + cos(rad) * cardinal_text_radius;
        int16_t label_y = center_y - sin(rad) * cardinal_text_radius;

        lv_obj_t *cardinal_label = lv_label_create(compass);
        lv_label_set_text(cardinal_label, cardinals[i]);
        lv_obj_set_style_text_color(cardinal_label, lv_color_white(), 0);
        lv_obj_set_style_text_font(cardinal_label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_align(cardinal_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(cardinal_label, label_x - 8, label_y - 8); // Center the label
    }

    /* 5. Create North Indicator (Red Triangle) using a canvas */
    lv_obj_t *triangle = lv_canvas_create(compass);
    lv_canvas_set_buffer(triangle, NULL, 20, 20, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_pos(triangle, center_x - 10, center_y - 20);
    
    // Draw triangle on canvas (simplified approach)
    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_color = lv_color_hex(0xFF0000); // Red
    rect_dsc.bg_opa = LV_OPA_COVER;
    
    // For simplicity, create a red square as north indicator
    lv_obj_set_size(triangle, 20, 20);
    lv_obj_set_style_bg_color(triangle, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_bg_opa(triangle, LV_OPA_COVER, 0);
}

// To use it, call this function in your LVGL setup (e.g., in `lv_app_init`):
// create_circular_compass(lv_scr_act());


/**
 * @brief user_main
 *
 * @param[in] param:Task parameters
 * @return none
 */
void user_main(void)
{
    /* basic init */
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 4096, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    /*hardware register*/
    board_register_hardware();

    lv_vendor_init(DISPLAY_NAME);

    lv_vendor_disp_lock();

    // Create the circular compass instead of the default demo
    create_circular_compass(lv_scr_act());
    
    lv_vendor_disp_unlock();

    lv_vendor_start(5, 1024*8);
}

/**
 * @brief main
 *
 * @param argc
 * @param argv
 * @return void
 */
#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    user_main();

    while (1) {
        tal_system_sleep(500);
    }
}
#else

/* Tuya thread handle */
static THREAD_HANDLE ty_app_thread = NULL;

/**
 * @brief  task thread
 *
 * @param[in] arg:Parameters when creating a task
 * @return none
 */
static void tuya_app_thread(void *arg)
{
    (void) arg;

    user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {1024 * 4, 4, "tuya_app_main"};
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif