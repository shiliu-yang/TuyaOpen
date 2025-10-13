/**
 * @file cattle_ai_tracker_app.h
 */

#ifndef CATTLE_AI_TRACKER_APP_H
#define CATTLE_AI_TRACKER_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/* Target marker colors */
typedef enum {
    TARGET_COLOR_GREEN = 0x7ED643,  /* 绿 - Green */
    TARGET_COLOR_YELLOW = 0xFFA000, /* 黄 - Yellow */
    TARGET_COLOR_PINK = 0xEC8FD4,   /* 粉 - Pink */
    TARGET_COLOR_CYAN = 0x78CFD1,   /* 青 - Cyan */
    TARGET_COLOR_PURPLE = 0x8A66F9, /* 紫 - Purple */
    TARGET_COLOR_COW = 0x6A6AF2     /* 牛 - Cow (special marker) */
} target_color_t;

#ifndef CATTLE_SCREEN_WIDTH
#define CATTLE_SCREEN_WIDTH 466
#endif
#ifndef CATTLE_SCREEN_HEIGHT
#define CATTLE_SCREEN_HEIGHT 466
#endif

void lv_demo_cattle_ai_tracker(void);
void update_idle_bottom_text(const char *text);

/* GPS API Functions */
void gps_add_target(float lat, float lon, uint32_t color);
void gps_add_target_at_distance(float distance_meters, float bearing_degrees, uint32_t color);
void gps_remove_target(int index);
void gps_clear_all_targets(void);
void gps_set_tracker_position(float lat, float lon);
int gps_get_target_count(void);
int gps_get_target_distance(int index);
void gps_update_target_markers(void);
void gps_mark_data_dirty(void);

/* Dummy data access functions */
float gps_get_dummy_self_lat(void);
float gps_get_dummy_self_lon(void);
int gps_get_dummy_target_count(void);
const void *gps_get_dummy_target(int index);

/* Zoom/Scale control functions */
void animate_distance_scale(int target_scale);
void tracker_set_distance_scale(int scale_meters);
int tracker_get_distance_scale(void);

#ifdef __cplusplus
}
#endif

#endif /* CATTLE_AI_TRACKER_APP_H */
