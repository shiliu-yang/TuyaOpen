/**
 * @file cattle_ai_tracker_app.c
 * Minimal, hardware-agnostic tracker UI demo for a 466x466 circular OLED.
 */

/*********************
 *      INCLUDES
 *********************/
 #include "cattle_ai_tracker_app.h"
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <math.h>
 #include "resources/compass_face_ring.c"
 #include "resources/compass_center_find.c"
 #include "resources/diatance.c"
 #include "resources/compass_cow_loc.c"
 #include "resources/closing_nav_arrow.c"
 #include "resources/closing_nav_ring.c"
 #include "resources/closing_nav_cow_icon.c"
 #include "resources/icons/mic_red_icon.c"

#include "ai_audio.h"
#include "tuya_lvgl.h"
#include "tal_api.h"

 /*********************
  *      DEFINES
  *********************/

 /* Dummy GPS data structure */
 typedef struct {
     float lat, lon;
     uint32_t color;
 } dummy_target_t;

 /* Dummy GPS data */
 static const float DUMMY_SELF_LAT = 30.300500622255004f; /* Tracker location */
 static const float DUMMY_SELF_LON = 120.06815327830921f;

 static const dummy_target_t DUMMY_TARGETS[] = {
     {30.300500622255004f, 120.06815327830921f, TARGET_COLOR_CYAN},
     {30.31324992866212f, 120.07040253859043f, TARGET_COLOR_YELLOW},
     {30.30420156852781f, 120.10282414801489f, TARGET_COLOR_PINK},
     {30.295801190761917f, 120.06388543982636f, TARGET_COLOR_COW}, /* Cow target */
 };

 #define DUMMY_TARGET_COUNT (sizeof(DUMMY_TARGETS) / sizeof(DUMMY_TARGETS[0]))

 /*********************
  *      DEFINES
  *********************/
 #define CIRCLE_RADIUS         (CATTLE_SCREEN_WIDTH / 2)
 #define CIRCLE_CENTER         (CIRCLE_RADIUS)
 #define SOS_HOLD_MS           3000
 #define SETTINGS_PANEL_HEIGHT CATTLE_SCREEN_HEIGHT // Full screen height

 /* Feature flags */
 #define ENABLE_CLOSE_TRACKING 0 // Set to 1 to enable close tracking mode, 0 to disable

 /**********************
  *      TYPEDEFS
  **********************/
 typedef struct {
     lv_obj_t *screen;
     lv_obj_t *viewport; /* circular clipped container */

     /* Screens */
     lv_obj_t *idle_screen;
     lv_obj_t *tracking_screen;
     lv_obj_t *sos_screen;

     /* Common overlays */
     lv_obj_t *settings_panel;
     lv_obj_t *settings_backdrop;
     lv_obj_t *settings_drag_tab;

     /* Tracking UI */
     lv_obj_t *compass_container;
     lv_obj_t *compass_dial;
     lv_obj_t *compass_labels;
     lv_obj_t *compass_face_ring_img;
     lv_obj_t *compass_center_overlay;
     lv_obj_t **interval_lines; /* Dynamic interval lines - allocated as needed */
     int interval_lines_count;  /* Number of interval lines allocated */
     lv_obj_t *distance_img;
     lv_obj_t *distance_text;
     lv_obj_t *rotation_bg;
     lv_obj_t *rotation_text;
     lv_obj_t *calib_panel;
     bool calibrated;
     float yaw_deg;
     int distance_meters;
     int distance_scale_meters; /* Current distance scale for targets */
     int target_distance_scale; /* Target distance scale for animation */
     lv_anim_t *distance_anim;  /* Animation for distance transitions */

     /* GPS System */
     int gps_sat_count;
     float self_lat, self_lon; /* Tracker position (center) */

 /* Target tracking system */
 #define MAX_TARGETS 10
     struct {
         float lat, lon;
         uint32_t color;
         bool active;
         int distance_meters;
         /* Performance optimization: cached screen coordinates */
         float cached_x_meters, cached_y_meters; /* Cached meter coordinates */
         float cached_screen_x, cached_screen_y; /* Cached screen pixel coordinates */
         bool coordinates_dirty;                 /* Flag to indicate if coordinates need recalculation */
     } targets[MAX_TARGETS];
     int target_count;

     /* Map scale and rendering */
     float map_scale; /* meters per pixel */
     lv_obj_t *map_container;
     lv_obj_t *target_markers[MAX_TARGETS];

     /* Performance optimization: cached calculations */
     float cached_lon_factor;    /* Cached longitude factor for current latitude */
     float cached_lat_factor;    /* Cached latitude factor */
     bool gps_data_dirty;        /* Flag to indicate if GPS data has changed */
     bool map_scale_dirty;       /* Flag to indicate if map scale needs recalculation */
     float cached_screen_radius; /* Cached screen radius calculation */

     /* SOS */
     lv_obj_t *sos_hold_ring;
     lv_obj_t *sos_cancel_btn;
     lv_timer_t *sos_timer;
     uint32_t sos_pressed_start_ms;
     bool sos_active;

     /* Timers */
     lv_timer_t *tick_timer;

     /* Smooth rotation animation */
     float target_yaw_deg;     /* Target angle for smooth rotation */
     float current_yaw_deg;    /* Current angle during animation */
     lv_anim_t *rotation_anim; /* Animation object for smooth rotation */
     bool is_rotating;         /* Flag to indicate if rotation is in progress */

     /* Pinch gesture detection */
     bool pinch_active;          /* Flag to indicate if pinch gesture is active */
     float pinch_start_distance; /* Initial distance between fingers */
     float pinch_start_scale;    /* Initial scale when pinch started */
     float current_scale;        /* Current zoom scale */

     /* Focus object for keyboard events */
     lv_obj_t *focus_obj;

     /* Idle screen bottom text */
     lv_obj_t *idle_bottom_text;

    /* Typewriter animation state */
    char *typewriter_full_text;      /* Full text to display */
    int typewriter_char_index;       /* Current character index being displayed */
    int typewriter_window_start;     /* Start position for sliding window */
    lv_timer_t *typewriter_timer;    /* Timer for animation */
    bool typewriter_active;          /* Animation active flag */

    /* Eye animation state */
    lv_obj_t *left_eye;              /* Left eye container */
    lv_obj_t *right_eye;             /* Right eye container */
    lv_obj_t *left_pupil;            /* Left eye white part */
    lv_obj_t *right_pupil;           /* Right eye white part */
    lv_timer_t *eye_blink_timer;     /* Blink animation timer */
    lv_timer_t *eye_look_timer;      /* Eye movement timer */
    int eye_state;                   /* 0=idle, 1=blink, 2=happy, 3=surprised, 4=sleepy, 5=wink, 6=angry */
    int blink_phase;                 /* Blink animation phase */
    int pupil_x_offset;              /* Pupil horizontal offset */
    int pupil_y_offset;              /* Pupil vertical offset */
    int target_pupil_x;              /* Target pupil X position */
    int target_pupil_y;              /* Target pupil Y position */
    int pupil_size;                  /* Current pupil size (for dilation effect) */
    int target_pupil_size;           /* Target pupil size */
    int blink_variation;             /* 0=normal, 1=double, 2=slow */

    /* Idle screen red ring indicator */
    lv_obj_t *idle_red_ring;         /* Red 5px ring overlay */
    lv_obj_t *idle_mic_icon;         /* Red microphone icon */
    bool red_ring_visible;           /* Red ring visibility state */

    /* Settings panel icon objects */
    lv_obj_t *battery_icon_img;      /* Battery icon image */
    lv_obj_t *network_icon_img;      /* Network (4G/WiFi) icon image */
    lv_obj_t *gps_icon_img;          /* GPS icon image */

    /* Settings panel control objects */
    lv_obj_t *settings_time_label;   /* Time label (HH:MM format) */
    lv_obj_t *settings_date_label;   /* Date label (YYYY / MM / DD format) */
    lv_obj_t *settings_volume_slider; /* Volume slider control */
    lv_obj_t *settings_gps_sats_label; /* GPS satellites count label */
    int current_volume;              /* Current volume value (0-100) */
    void (*volume_change_callback)(int volume); /* Callback for volume changes */

 #if ENABLE_CLOSE_TRACKING
     /* Close-range navigation mode */
     bool close_range_mode;
     lv_obj_t *close_nav_container;
     lv_obj_t *close_nav_ring_img;
     lv_obj_t *close_nav_arrow_img;
     lv_obj_t *close_nav_cow_icon_img;
     lv_obj_t *close_nav_distance_text;
     lv_obj_t *close_nav_compass_text;
     lv_obj_t *found_circle;
     lv_obj_t *found_cow_icon;
     lv_obj_t *found_text;
     lv_anim_t *close_nav_zoom_anim;
     bool close_nav_zooming_out;
 #endif
 } cattle_app_t;

 static cattle_app_t g;

 /**********************
  *  STATIC PROTOTYPES
  **********************/
 static void clamp_input_coordinates(lv_point_t *point);
static void set_distance_text(int meters);
static void update_rotation_text(float yaw_degrees);
static void __attribute__((unused)) update_distance_scale(void);
/* animate_distance_scale is now public - declared in header */
static void on_distance_anim_value(void *var, int32_t value);
static void on_distance_anim_ready(lv_anim_t *anim);
static void typewriter_timer_cb(lv_timer_t *timer);
static void update_idle_bottom_text_static(const char *text);
static int utf8_next_char_size(const char *text, int pos);
static void create_eyes(void);
static void eye_blink_timer_cb(lv_timer_t *timer);
static void eye_look_timer_cb(lv_timer_t *timer);
static void set_eye_state(int state);
static void update_eye_animation(void);
static void update_pupil_position(void);
 static void update_interval_lines(void);
 static void create_interval_lines(void);
 static float calculate_distance(float lat1, float lon1, float lat2, float lon2);
 static void add_target_coord(float lat, float lon, uint32_t color);
 static void add_target_at_distance(float distance_meters, float bearing_degrees, uint32_t color);
 static void remove_target_coord(int index);
 static void clear_all_targets(void);
 static void update_map_scale(void);
 static void render_target_markers(void);
 static void update_target_positions(void);
 static void update_target_positions_for_new_origin(void);
 static void update_cached_coordinates(void);
 static void mark_all_coordinates_dirty(void);
 static void create_root(void);
 static void create_idle_screen(void);
 static void create_tracking_screen(void);
 static void create_settings_panel(void);
 static void create_sos_screen(void);

 static void show_idle(void);
 static void __attribute__((unused)) show_tracking(void);
 static void slide_tracking(bool show);
 static void show_sos_alert(void);
 static void hide_sos_alert(void);
 #if ENABLE_CLOSE_TRACKING
 static void show_close_range_mode(void);
 static void hide_close_range_mode(void);
 static void create_close_range_ui(void);
 static void update_close_range_arrow(void);
 static void on_close_nav_zoom_anim(void *var, int32_t value);
 static void on_close_nav_zoom_ready(lv_anim_t *anim);
 #endif

 static void on_keyboard(lv_event_t *e);
 static void on_pressed(lv_event_t *e);
 static void on_pressing(lv_event_t *e);
 static void on_released(lv_event_t *e);
 static void on_gesture(lv_event_t *e);
 static void on_settings_drag(lv_event_t *e);
 static void on_volume_slider_changed(lv_event_t *e);
 static void on_tracking_drag(lv_event_t *e);
 static void on_settings_backdrop_click(lv_event_t *e);
 static void __attribute__((unused)) on_sos_cancel(lv_event_t *e);
 static void on_tick(lv_timer_t *t);
 static void on_settings_close_anim_ready(lv_anim_t *anim);
 static void on_tracking_close_anim_ready(lv_anim_t *anim);
 static void on_rotation_anim_value(void *var, int32_t value);
 static void on_rotation_anim_ready(lv_anim_t *anim);
 static void start_smooth_rotation(void);
 static float wrap_angle_smooth(float current, float target);
 static void on_pinch_gesture(lv_event_t *e);
 // static void handle_pinch_gesture(lv_event_t *e);
 // static void apply_zoom_to_compass(void);
 // static void update_interval_lines_scale(void);
 static void on_bg_opa_anim(void *var, int32_t value)
 {
     lv_obj_t *obj = (lv_obj_t *)var;
     lv_obj_set_style_bg_opa(obj, (lv_opa_t)value, 0);
 }

 static void compass_update(float yaw_deg);
 static void compass_build(lv_obj_t *parent);
 /* Calibration function removed */

 /**********************
  *   GLOBAL FUNCTIONS
  **********************/

/* Icon state management API implementations */

/**
 * Sets the battery icon based on level and charging status
 * @param level: 0=20%, 1=50%, 2=70%, 3=full (100%)
 * @param is_charging: true if battery is charging
 */
void set_battery_icon(int level, bool is_charging)
{
    if (!g.battery_icon_img) return;  /* Safety check */

    if (is_charging) {
        lv_img_set_src(g.battery_icon_img, &battery_charging);
    } else {
        switch (level) {
            case 0:
                lv_img_set_src(g.battery_icon_img, &battery_20);
                break;
            case 1:
                lv_img_set_src(g.battery_icon_img, &battery_50);
                break;
            case 2:
                lv_img_set_src(g.battery_icon_img, &battery_70);
                break;
            case 3:
            default:
                lv_img_set_src(g.battery_icon_img, &battery_full);
                break;
        }
    }
}

/**
 * Sets the network icon (4G/WiFi) based on type and status
 * @param use_4g: true for 4G, false for WiFi
 * @param is_enabled: true if connected, false if disabled
 */
void set_network_icon(bool use_4g, bool is_enabled)
{
    if (!g.network_icon_img) return;  /* Safety check */

    if (use_4g) {
        if (is_enabled) {
            lv_img_set_src(g.network_icon_img, &_4g_enable);
        } else {
            lv_img_set_src(g.network_icon_img, &_4g_disabled);
        }
    } else {
        /* WiFi always shows as enabled */
        lv_img_set_src(g.network_icon_img, &wifi_enable);
    }
}

/**
 * Updates the time display in the settings panel (24-hour format)
 * @param hour: Hour value (0-23)
 * @param minute: Minute value (0-59)
 */
void set_settings_time(int hour, int minute)
{
    if (!g.settings_time_label) return;  /* Safety check */

    /* Clamp values to valid ranges */
    if (hour < 0) hour = 0;
    if (hour > 23) hour = 23;
    if (minute < 0) minute = 0;
    if (minute > 59) minute = 59;

    char time_buf[16];
    snprintf(time_buf, sizeof(time_buf), "%02d:%02d", hour, minute);
    lv_label_set_text(g.settings_time_label, time_buf);
}

/**
 * Updates the date display in the settings panel
 * @param year: Year value (e.g., 2024)
 * @param month: Month value (1-12)
 * @param day: Day value (1-31)
 */
void set_settings_date(int year, int month, int day)
{
    if (!g.settings_date_label) return;  /* Safety check */

    /* Clamp values to valid ranges */
    if (year < 2000) year = 2000;
    if (year > 2099) year = 2099;
    if (month < 1) month = 1;
    if (month > 12) month = 12;
    if (day < 1) day = 1;
    if (day > 31) day = 31;

    char date_buf[32];
    snprintf(date_buf, sizeof(date_buf), "%04d / %02d / %02d", year, month, day);
    lv_label_set_text(g.settings_date_label, date_buf);
}

/**
 * Sets the volume value and updates the slider
 * @param volume: Volume value (0-100)
 */
void set_volume(int volume)
{
    if (!g.settings_volume_slider) return;  /* Safety check */

    /* Clamp value to valid range */
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;

    g.current_volume = volume;
    lv_slider_set_value(g.settings_volume_slider, volume, LV_ANIM_ON);
}

/**
 * Gets the current volume value
 * @return Volume value (0-100)
 */
int get_volume(void)
{
    /* If slider exists, read from it to get the most current value */
    if (g.settings_volume_slider) {
        g.current_volume = lv_slider_get_value(g.settings_volume_slider);
    }
    return g.current_volume;
}

/**
 * Sets the volume change callback
 * @param callback: Function pointer to call when volume changes
 */
void set_volume_change_callback(void (*callback)(int volume))
{
    g.volume_change_callback = callback;
}

/**
 * Volume slider event handler - called when user changes volume
 */
static void on_volume_slider_changed(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int volume = lv_slider_get_value(slider);
    g.current_volume = volume;
    
    /* Call the callback if set */
    if (g.volume_change_callback) {
        g.volume_change_callback(volume);
    }
    
    // printf("Volume changed to: %d%%\n", volume);
}

/**
 * Sets the red ring indicator visibility on the idle screen
 * @param visible: true to show red ring and microphone icon, false to hide
 */
void set_idle_red_ring(bool visible)
{
    if (!g.idle_red_ring) return;  /* Safety check */

    g.red_ring_visible = visible;

    if (visible) {
        lv_obj_clear_flag(g.idle_red_ring, LV_OBJ_FLAG_HIDDEN);
        if (g.idle_mic_icon) {
            lv_obj_clear_flag(g.idle_mic_icon, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        lv_obj_add_flag(g.idle_red_ring, LV_OBJ_FLAG_HIDDEN);
        if (g.idle_mic_icon) {
            lv_obj_add_flag(g.idle_mic_icon, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

/**
 * Toggles the red ring indicator visibility on the idle screen
 */
void toggle_idle_red_ring(void)
{
    set_idle_red_ring(!g.red_ring_visible);
}

/**
 * Sets the GPS satellite count display
 * @param count: Number of satellites (1-32)
 */
void set_gps_satellite_count(int count)
{

    if (!g.settings_gps_sats_label) return;  /* Safety check */

    /* Clamp value to valid range */
    if (count < 0) count = 0;
    if (count > 32) count = 32;

    char sats_buf[32];
    snprintf(sats_buf, sizeof(sats_buf), "%d 颗卫星", count);

    lv_label_set_text(g.settings_gps_sats_label, sats_buf);

    /* Set color based on satellite count */
    if (count < 10) {
        /* Less than 10: Red (poor signal) */
        lv_obj_set_style_text_color(g.settings_gps_sats_label, lv_color_hex(0xFF0000), 0);
    } else if (count < 20) {
        /* 10-19: Orange (moderate signal) */
        lv_obj_set_style_text_color(g.settings_gps_sats_label, lv_color_hex(0xFF8800), 0);
    } else {
        /* 20+: Green (good signal) */
        lv_obj_set_style_text_color(g.settings_gps_sats_label, lv_color_hex(0x00FF00), 0);
    }
}

void lv_demo_cattle_ai_tracker(void)
 {
     /* Save the volume callback before memset clears it */
     void (*saved_callback)(int volume) = g.volume_change_callback;
     
     memset(&g, 0, sizeof(g));
     
     /* Restore the volume callback */
     g.volume_change_callback = saved_callback;

     /* Dummy data */
     g.gps_sat_count = 7;
     g.self_lat = 22.280f;
     g.self_lon = 114.158f; /* HK */
     g.calibrated = true;   // Start as calibrated to avoid showing calibration panel
     g.yaw_deg = 0.f;

     /* Initialize smooth rotation animation */
     g.target_yaw_deg = 0.f;
     g.current_yaw_deg = 0.f;
     g.rotation_anim = NULL;
     g.is_rotating = false;

     /* Initialize pinch gesture detection */
     g.pinch_active = false;
     g.pinch_start_distance = 0.f;
     g.pinch_start_scale = 1.0f;
     g.current_scale = 1.0f;

     /* Performance optimization: Initialize cache flags */
     g.gps_data_dirty = true; /* Force initial coordinate calculation */
     g.map_scale_dirty = true;

     create_root();
     create_idle_screen();
     create_tracking_screen();
     create_settings_panel();
     create_sos_screen();

     show_idle();

     /* Create hidden focusable object for keyboard events */
     lv_obj_t *focus = lv_obj_create(g.screen);
     lv_obj_set_size(focus, 1, 1);
     lv_obj_add_flag(focus, LV_OBJ_FLAG_HIDDEN);
     lv_obj_add_flag(focus, LV_OBJ_FLAG_CLICK_FOCUSABLE);
     lv_obj_add_event_cb(focus, on_keyboard, LV_EVENT_KEY, NULL);
     lv_group_add_obj(lv_group_get_default(), focus);
     lv_group_focus_obj(focus);

     /* Store focus object globally for later use */
     g.focus_obj = focus;

     /* Global press handling */
     lv_obj_add_event_cb(g.screen, on_pressed, LV_EVENT_PRESSED, NULL);
     lv_obj_add_event_cb(g.screen, on_pressing, LV_EVENT_PRESSING, NULL);
     lv_obj_add_event_cb(g.screen, on_released, LV_EVENT_RELEASED, NULL);

     /* Gesture handling for settings */
     lv_obj_add_event_cb(g.screen, on_gesture, LV_EVENT_GESTURE, NULL);

     /* Pinch gesture handling for zoom */
     lv_obj_add_event_cb(g.screen, on_pinch_gesture, LV_EVENT_GESTURE, NULL);

    /* Timer-based animation - 5 second intervals, 45 degrees per rotation */
    // TODO: for test 
    // g.tick_timer = lv_timer_create(on_tick, 5000, NULL); /* 5000ms = 5 seconds */

    set_gps_satellite_count(0);
    
    /* Set default bottom text with typewriter animation */
    update_idle_bottom_text("你好，今天怎么帮你找牛？");
}

void set_sos_visible(bool visible)
{
    tuya_lvgl_mutex_lock();
    if (visible) {
        show_sos_alert();
    } else {
        hide_sos_alert();
    }
    tuya_lvgl_mutex_unlock();
}

 /**********************
  *   STATIC FUNCTIONS
  **********************/

 static void clamp_input_coordinates(lv_point_t *point)
 {
     /* Clamp coordinates to 466x466 constraint */
     if (point->x < 0)
         point->x = 0;
     if (point->x >= CATTLE_SCREEN_WIDTH)
         point->x = CATTLE_SCREEN_WIDTH - 1;
     if (point->y < 0)
         point->y = 0;
     if (point->y >= CATTLE_SCREEN_HEIGHT)
         point->y = CATTLE_SCREEN_HEIGHT - 1;
 }
 static void create_root(void)
 {
     g.screen = lv_obj_create(NULL);
     lv_obj_set_size(g.screen, CATTLE_SCREEN_WIDTH, CATTLE_SCREEN_HEIGHT);
     lv_obj_set_style_bg_color(g.screen, lv_color_hex(0x101214), 0);
     lv_obj_set_style_bg_opa(g.screen, LV_OPA_COVER, 0);

     /* Disable scrolling on main screen to enforce 466x466 constraint */
     lv_obj_clear_flag(g.screen, LV_OBJ_FLAG_SCROLLABLE);
     lv_obj_clear_flag(g.screen, LV_OBJ_FLAG_SCROLL_ELASTIC);
     lv_obj_clear_flag(g.screen, LV_OBJ_FLAG_SCROLL_MOMENTUM);
     lv_obj_clear_flag(g.screen, LV_OBJ_FLAG_SCROLL_ONE);
     lv_obj_clear_flag(g.screen, LV_OBJ_FLAG_SCROLL_CHAIN);

     /* Circular viewport centered - no border */
     g.viewport = lv_obj_create(g.screen);
     lv_obj_set_size(g.viewport, CATTLE_SCREEN_WIDTH, CATTLE_SCREEN_HEIGHT);
     lv_obj_center(g.viewport);
     lv_obj_set_style_radius(g.viewport, CIRCLE_RADIUS, 0);
     lv_obj_set_style_clip_corner(g.viewport, true, 0);
     lv_obj_set_style_border_width(g.viewport, 0, 0);
     lv_obj_set_style_bg_color(g.viewport, lv_color_black(), 0);
     lv_obj_set_style_bg_opa(g.viewport, LV_OPA_COVER, 0);

     /* Disable scrolling on viewport to enforce 466x466 constraint */
     lv_obj_clear_flag(g.viewport, LV_OBJ_FLAG_SCROLLABLE);
     lv_obj_clear_flag(g.viewport, LV_OBJ_FLAG_SCROLL_ELASTIC);
     lv_obj_clear_flag(g.viewport, LV_OBJ_FLAG_SCROLL_MOMENTUM);
     lv_obj_clear_flag(g.viewport, LV_OBJ_FLAG_SCROLL_ONE);
     lv_obj_clear_flag(g.viewport, LV_OBJ_FLAG_SCROLL_CHAIN);

     lv_screen_load(g.screen);
 }

/* Create eye animation */
static void create_eyes(void)
{
    const int EYE_SPACING = 140;    /* Distance between eyes */
    const int EYE_WIDTH = 140;      /* Eye container width (larger to prevent cropping) */
    const int EYE_HEIGHT = 140;     /* Eye container height (larger to prevent cropping) */
    const int PUPIL_SIZE = 100;     /* Pupil (white part) size */

    /* Left eye container - larger to accommodate pupil movement */
    g.left_eye = lv_obj_create(g.idle_screen);
    lv_obj_remove_style_all(g.left_eye);
    lv_obj_set_size(g.left_eye, EYE_WIDTH, EYE_HEIGHT);
    lv_obj_set_style_radius(g.left_eye, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(g.left_eye, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(g.left_eye, LV_OPA_COVER, 0);
    lv_obj_set_style_clip_corner(g.left_eye, true, 0);  /* Enable clipping for bottom crop effect */
    lv_obj_align(g.left_eye, LV_ALIGN_CENTER, -EYE_SPACING / 2, 0);

    /* Left pupil (white oval) */
    g.left_pupil = lv_obj_create(g.left_eye);
    lv_obj_remove_style_all(g.left_pupil);
    lv_obj_set_size(g.left_pupil, PUPIL_SIZE, PUPIL_SIZE);
    lv_obj_set_style_radius(g.left_pupil, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(g.left_pupil, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(g.left_pupil, LV_OPA_COVER, 0);
    lv_obj_center(g.left_pupil);

    /* Right eye container - larger to accommodate pupil movement */
    g.right_eye = lv_obj_create(g.idle_screen);
    lv_obj_remove_style_all(g.right_eye);
    lv_obj_set_size(g.right_eye, EYE_WIDTH, EYE_HEIGHT);
    lv_obj_set_style_radius(g.right_eye, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(g.right_eye, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(g.right_eye, LV_OPA_COVER, 0);
    lv_obj_set_style_clip_corner(g.right_eye, true, 0);  /* Enable clipping for bottom crop effect */
    lv_obj_align(g.right_eye, LV_ALIGN_CENTER, EYE_SPACING / 2, 0);

    /* Right pupil (white oval) */
    g.right_pupil = lv_obj_create(g.right_eye);
    lv_obj_remove_style_all(g.right_pupil);
    lv_obj_set_size(g.right_pupil, PUPIL_SIZE, PUPIL_SIZE);
    lv_obj_set_style_radius(g.right_pupil, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(g.right_pupil, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(g.right_pupil, LV_OPA_COVER, 0);
    lv_obj_center(g.right_pupil);

    /* Initialize eye state */
    g.eye_state = 0;  /* Idle */
    g.blink_phase = 0;
    g.pupil_x_offset = 0;
    g.pupil_y_offset = 0;
    g.pupil_size = 100;  /* Default pupil size */
    g.target_pupil_size = 100;
    g.blink_variation = 0;

    /* Start with a random look direction */
    int initial_direction = rand() % 5;
    if (initial_direction == 0) {
        g.target_pupil_x = -18;  /* Look left */
        g.target_pupil_y = 0;
    } else if (initial_direction == 1) {
        g.target_pupil_x = 18;   /* Look right */
        g.target_pupil_y = 0;
    } else if (initial_direction == 2) {
        g.target_pupil_x = 0;    /* Look up */
        g.target_pupil_y = -18;
    } else {
        g.target_pupil_x = 0;    /* Center */
        g.target_pupil_y = 0;
    }

    /* Create blink timer - start with random interval between 3-8 seconds */
    int initial_blink_delay = 3000 + (rand() % 5000);
    g.eye_blink_timer = lv_timer_create(eye_blink_timer_cb, initial_blink_delay, NULL);

    /* Create look around timer - 50ms continuous update */
    g.eye_look_timer = lv_timer_create(eye_look_timer_cb, 50, NULL);
}

/* Update pupil position and size based on current offsets */
static void update_pupil_position(void)
{
    /* Smoothly adjust pupil size (dilation effect) */
    if (g.pupil_size != g.target_pupil_size) {
        if (g.pupil_size < g.target_pupil_size) {
            g.pupil_size += 2;
            if (g.pupil_size > g.target_pupil_size) g.pupil_size = g.target_pupil_size;
        } else {
            g.pupil_size -= 2;
            if (g.pupil_size < g.target_pupil_size) g.pupil_size = g.target_pupil_size;
        }
    }

    /* Apply pupil offset for looking around effect */
    lv_obj_align(g.left_pupil, LV_ALIGN_CENTER, g.pupil_x_offset, g.pupil_y_offset);
    lv_obj_align(g.right_pupil, LV_ALIGN_CENTER, g.pupil_x_offset, g.pupil_y_offset);
}

/* Update eye animation based on current state */
static void update_eye_animation(void)
{
    const int EYE_WIDTH = 120;
    const int EYE_HEIGHT = 120;

    switch (g.eye_state) {
    case 0:  /* Idle state - normal round eyes with size variation */
        lv_obj_set_size(g.left_eye, EYE_WIDTH, EYE_HEIGHT);
        lv_obj_set_size(g.right_eye, EYE_WIDTH, EYE_HEIGHT);
        lv_obj_set_size(g.left_pupil, g.pupil_size, g.pupil_size);
        lv_obj_set_size(g.right_pupil, g.pupil_size, g.pupil_size);
        update_pupil_position();
        break;

    case 1:  /* Blinking state - quick oval blink */
        {
            int blink_height = EYE_HEIGHT - (g.blink_phase * 60);
            if (blink_height < 30) blink_height = 30;
            lv_obj_set_size(g.left_eye, EYE_WIDTH, blink_height);
            lv_obj_set_size(g.right_eye, EYE_WIDTH, blink_height);
            int pupil_height = blink_height - 15;
            if (pupil_height < 15) pupil_height = 15;
            lv_obj_set_size(g.left_pupil, g.pupil_size, pupil_height);
            lv_obj_set_size(g.right_pupil, g.pupil_size, pupil_height);
            update_pupil_position();
        }
        break;

    case 2:  /* Happy state - curved/squinted eyes */
        lv_obj_set_size(g.left_eye, EYE_WIDTH, 40);
        lv_obj_set_size(g.right_eye, EYE_WIDTH, 40);
        lv_obj_set_size(g.left_pupil, g.pupil_size, 30);
        lv_obj_set_size(g.right_pupil, g.pupil_size, 30);
        update_pupil_position();
        break;

    case 3:  /* Surprised state - wide open eyes with dilated pupils */
        lv_obj_set_size(g.left_eye, EYE_WIDTH + 10, EYE_HEIGHT + 10);
        lv_obj_set_size(g.right_eye, EYE_WIDTH + 10, EYE_HEIGHT + 10);
        lv_obj_set_size(g.left_pupil, g.pupil_size, g.pupil_size);
        lv_obj_set_size(g.right_pupil, g.pupil_size, g.pupil_size);
        update_pupil_position();
        break;

    case 4:  /* Sleepy state - droopy eyes */
        lv_obj_set_size(g.left_eye, EYE_WIDTH, 60);
        lv_obj_set_size(g.right_eye, EYE_WIDTH, 60);
        lv_obj_set_size(g.left_pupil, g.pupil_size, 45);
        lv_obj_set_size(g.right_pupil, g.pupil_size, 45);
        update_pupil_position();
        break;

    case 5:  /* Wink state - left eye closed, right eye open */
        lv_obj_set_size(g.left_eye, EYE_WIDTH, 20);  /* Left eye closed */
        lv_obj_set_size(g.right_eye, EYE_WIDTH, EYE_HEIGHT);  /* Right eye open */
        lv_obj_set_size(g.left_pupil, g.pupil_size, 15);
        lv_obj_set_size(g.right_pupil, g.pupil_size, g.pupil_size);
        update_pupil_position();
        break;

    case 6:  /* Angry state - narrow eyes */
        lv_obj_set_size(g.left_eye, EYE_WIDTH, 50);
        lv_obj_set_size(g.right_eye, EYE_WIDTH, 50);
        lv_obj_set_size(g.left_pupil, g.pupil_size - 10, 40);
        lv_obj_set_size(g.right_pupil, g.pupil_size - 10, 40);
        update_pupil_position();
        break;

    default:
        /* Default to idle */
        lv_obj_set_size(g.left_eye, EYE_WIDTH, EYE_HEIGHT);
        lv_obj_set_size(g.right_eye, EYE_WIDTH, EYE_HEIGHT);
        lv_obj_set_size(g.left_pupil, g.pupil_size, g.pupil_size);
        lv_obj_set_size(g.right_pupil, g.pupil_size, g.pupil_size);
        update_pupil_position();
        break;
    }
}

/* Set eye state (0=idle, 1=blinking, 2=happy) */
static void set_eye_state(int state)
{
    g.eye_state = state;
    update_eye_animation();
}

/* Eye blink timer callback - random blinking at different paces */
static void eye_blink_timer_cb(lv_timer_t *timer)
{
    static int blink_step = 0;

    if (g.eye_state == 1) {
        /* Currently blinking - single phase instant blink */
        g.blink_phase++;

        if (g.blink_phase >= 1) {  /* Only 1 phase for instant blink */
            /* Start opening immediately */
            blink_step = 1;
        }

        if (blink_step == 1) {
            g.blink_phase--;
            if (g.blink_phase <= 0) {
                /* Blink complete - set random next blink interval */
                g.eye_state = 0;  /* Return to idle */
                g.blink_phase = 0;
                blink_step = 0;
                update_eye_animation();
                int next_blink = 3000 + (rand() % 5000);  /* Random 3-8 seconds */
                lv_timer_set_period(timer, next_blink);
                return;  /* Exit - don't override timer period */
            }
        }

        /* Continue blink animation */
        update_eye_animation();

    } else if (g.eye_state == 0) {
        /* Idle - occasionally trigger special expressions or normal blink */
        int variation = rand() % 100;

        if (variation < 5) {
            /* 5% chance - Surprised expression */
            g.eye_state = 3;
            g.target_pupil_size = 110;  /* Dilated pupils */
            update_eye_animation();
            lv_timer_set_period(timer, 1000);  /* Hold for 1 second */
        } else if (variation < 8) {
            /* 3% chance - Wink */
            g.eye_state = 5;
            update_eye_animation();
            lv_timer_set_period(timer, 500);  /* Hold for 0.5 seconds */
        } else if (variation < 10) {
            /* 2% chance - Sleepy */
            g.eye_state = 4;
            g.target_pupil_size = 85;  /* Smaller pupils when sleepy */
            update_eye_animation();
            lv_timer_set_period(timer, 1500);  /* Hold for 1.5 seconds */
        } else {
            /* 90% chance - Normal instant blink */
            g.eye_state = 1;
            g.blink_phase = 0;
            blink_step = 0;

            /* Occasionally do a double blink */
            g.blink_variation = (rand() % 10) < 2 ? 1 : 0;  /* 20% double blink */

            int blink_speed = 15 + (rand() % 10);  /* Random speed 15-25ms */
            lv_timer_set_period(timer, blink_speed);
        }

    } else if (g.eye_state >= 2) {
        /* Special states (happy, surprised, sleepy, wink, angry) - return to idle after duration */
        g.eye_state = 0;  /* Return to idle */
        g.target_pupil_size = 100;  /* Reset pupil size */
        update_eye_animation();
        /* Set random next blink interval */
        int next_blink = 3000 + (rand() % 5000);
        lv_timer_set_period(timer, next_blink);
    }
}

/* Eye look around timer callback - random eye movements */
static void eye_look_timer_cb(lv_timer_t *timer)
{
    static int move_step = 0;
    static bool moving = false;
    const int MAX_OFFSET = 18;  /* Maximum pupil movement in pixels (larger for more visible movement) */

    /* Smoothly move pupils towards target position (2px per step for faster movement) */
    bool still_moving = false;

    if (g.pupil_x_offset != g.target_pupil_x) {
        if (g.pupil_x_offset < g.target_pupil_x) {
            g.pupil_x_offset += 2;
            if (g.pupil_x_offset > g.target_pupil_x) g.pupil_x_offset = g.target_pupil_x;
        } else {
            g.pupil_x_offset -= 2;
            if (g.pupil_x_offset < g.target_pupil_x) g.pupil_x_offset = g.target_pupil_x;
        }
        still_moving = true;
    }

    if (g.pupil_y_offset != g.target_pupil_y) {
        if (g.pupil_y_offset < g.target_pupil_y) {
            g.pupil_y_offset += 2;
            if (g.pupil_y_offset > g.target_pupil_y) g.pupil_y_offset = g.target_pupil_y;
        } else {
            g.pupil_y_offset -= 2;
            if (g.pupil_y_offset < g.target_pupil_y) g.pupil_y_offset = g.target_pupil_y;
        }
        still_moving = true;
    }

    update_pupil_position();

    /* If moving, keep fast update rate */
    if (still_moving) {
        moving = true;
        lv_timer_set_period(timer, 50);  /* Fast updates during movement */
        return;
    }

    /* Reached target position - hold for a moment */
    if (moving) {
        moving = false;
        move_step = 0;
    }

    move_step++;

    if (move_step >= 20) {  /* Hold position for ~1 second (20 * 50ms) */
        /* Choose new random target position and pupil size variation */
        int direction = rand() % 10;

        /* Add slight pupil size variation for breathing/life effect */
        if (g.eye_state == 0) {  /* Only in idle state */
            g.target_pupil_size = 95 + (rand() % 11);  /* Random 95-105 */
        }

        if (direction < 2) {
            /* Look left */
            g.target_pupil_x = -MAX_OFFSET;
            g.target_pupil_y = 0;
        } else if (direction < 4) {
            /* Look right */
            g.target_pupil_x = MAX_OFFSET;
            g.target_pupil_y = 0;
        } else if (direction < 5) {
            /* Look up */
            g.target_pupil_x = 0;
            g.target_pupil_y = -MAX_OFFSET;
        } else if (direction < 6) {
            /* Look down */
            g.target_pupil_x = 0;
            g.target_pupil_y = MAX_OFFSET;
        } else if (direction < 7) {
            /* Look up-left */
            g.target_pupil_x = -MAX_OFFSET / 2;
            g.target_pupil_y = -MAX_OFFSET / 2;
        } else if (direction < 8) {
            /* Look up-right */
            g.target_pupil_x = MAX_OFFSET / 2;
            g.target_pupil_y = -MAX_OFFSET / 2;
        } else {
            /* Return to center */
            g.target_pupil_x = 0;
            g.target_pupil_y = 0;
        }

        move_step = 0;
        moving = true;  /* Start moving to new target */
    }

    /* Keep timer running at 50ms during hold period */
    lv_timer_set_period(timer, 50);
 }

 static void create_idle_screen(void)
 {
     g.idle_screen = lv_obj_create(g.screen);
     lv_obj_remove_style_all(g.idle_screen);
     lv_obj_set_size(g.idle_screen, CATTLE_SCREEN_WIDTH, CATTLE_SCREEN_HEIGHT);
     lv_obj_set_style_bg_opa(g.idle_screen, LV_OPA_TRANSP, 0);
     lv_obj_clear_flag(g.idle_screen, LV_OBJ_FLAG_SCROLLABLE);

     /* Disable all scrolling flags to enforce 466x466 constraint */
     lv_obj_clear_flag(g.idle_screen, LV_OBJ_FLAG_SCROLL_ELASTIC);
     lv_obj_clear_flag(g.idle_screen, LV_OBJ_FLAG_SCROLL_MOMENTUM);
     lv_obj_clear_flag(g.idle_screen, LV_OBJ_FLAG_SCROLL_ONE);
     lv_obj_clear_flag(g.idle_screen, LV_OBJ_FLAG_SCROLL_CHAIN);

    /* Create animated eyes in center */
    create_eyes();

    /* Bottom text - manual line breaking, maximum 3 lines with ellipsis */
     g.idle_bottom_text = lv_label_create(g.idle_screen);
     lv_obj_set_style_text_color(g.idle_bottom_text, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(g.idle_bottom_text, &font_puhui_18_2, 0);
     lv_obj_set_style_text_align(g.idle_bottom_text, LV_TEXT_ALIGN_CENTER, 0);

    /* Remove padding to maximize text area */
    lv_obj_set_style_pad_all(g.idle_bottom_text, 0, 0);

    /* No automatic wrapping - we handle line breaks manually */
    lv_label_set_long_mode(g.idle_bottom_text, LV_LABEL_LONG_CLIP);

    /* Circular screen geometry calculation:
     * Screen radius: 233px, positioned at bottom (~40px from edge)
     * At y=-40: chord width ≈ 2*sqrt(233^2 - 193^2) ≈ 270px
     * Font size 18px: ~13-14 Chinese chars per line
     * 3 lines: ~40-42 characters total capacity
     */
    /* No fixed width needed - text is pre-formatted with manual line breaks */

    /* Set line spacing for better readability */
    lv_obj_set_style_text_line_space(g.idle_bottom_text, 2, 0);

    /* Position at bottom, within circular boundary
     * y=-20 places text very close to bottom edge
     * With 3 lines (~60px total height), top line extends to y=-80
     */
    lv_obj_align(g.idle_bottom_text, LV_ALIGN_BOTTOM_MID, 0, -20);

    /* Create red ring indicator (hidden by default) */
    g.idle_red_ring = lv_obj_create(g.idle_screen);
    lv_obj_remove_style_all(g.idle_red_ring);
    lv_obj_set_size(g.idle_red_ring, CATTLE_SCREEN_WIDTH, CATTLE_SCREEN_HEIGHT);
    lv_obj_center(g.idle_red_ring);
    lv_obj_set_style_radius(g.idle_red_ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(g.idle_red_ring, 5, 0);
    lv_obj_set_style_border_color(g.idle_red_ring, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_border_opa(g.idle_red_ring, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(g.idle_red_ring, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(g.idle_red_ring, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(g.idle_red_ring, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g.idle_red_ring, LV_OBJ_FLAG_HIDDEN);  /* Hidden by default */

    /* Create red microphone icon image at top (hidden by default) */
    g.idle_mic_icon = lv_img_create(g.idle_screen);
    lv_img_set_src(g.idle_mic_icon, &mic_red_icon);  /* Use custom red microphone icon */
    lv_obj_align(g.idle_mic_icon, LV_ALIGN_TOP_MID, 0, 30);  /* Top center, 30px from edge */
    lv_obj_clear_flag(g.idle_mic_icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(g.idle_mic_icon, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g.idle_mic_icon, LV_OBJ_FLAG_HIDDEN);  /* Hidden by default */

    g.red_ring_visible = false;
}

 static void compass_build(lv_obj_t *parent)
 {
     g.compass_container = lv_obj_create(parent);
     lv_obj_remove_style_all(g.compass_container);
     lv_obj_set_size(g.compass_container, CATTLE_SCREEN_WIDTH, CATTLE_SCREEN_HEIGHT);
     lv_obj_clear_flag(g.compass_container, LV_OBJ_FLAG_SCROLLABLE);

     /* Disable all scrolling flags to enforce 466x466 constraint */
     lv_obj_clear_flag(g.compass_container, LV_OBJ_FLAG_SCROLL_ELASTIC);
     lv_obj_clear_flag(g.compass_container, LV_OBJ_FLAG_SCROLL_MOMENTUM);
     lv_obj_clear_flag(g.compass_container, LV_OBJ_FLAG_SCROLL_ONE);
     lv_obj_clear_flag(g.compass_container, LV_OBJ_FLAG_SCROLL_CHAIN);

     /* Compass face ring image - OPTIMIZED for performance */
     g.compass_face_ring_img = lv_img_create(g.compass_container);
     lv_img_set_src(g.compass_face_ring_img, &compass_face_ring);
     lv_obj_set_size(g.compass_face_ring_img, 466, 466); /* Set actual image size */
     lv_obj_center(g.compass_face_ring_img);
     lv_obj_clear_flag(g.compass_face_ring_img, LV_OBJ_FLAG_CLICKABLE);
     lv_obj_clear_flag(g.compass_face_ring_img, LV_OBJ_FLAG_SCROLLABLE);

     /* Performance optimizations */
     lv_obj_set_style_opa(g.compass_face_ring_img, LV_OPA_COVER, 0);                /* Ensure full opacity */
     lv_obj_set_style_blend_mode(g.compass_face_ring_img, LV_BLEND_MODE_NORMAL, 0); /* Normal blending */
     /* Note: antialiasing control not available in this LVGL version */

     /* Set pivot point to screen center for rotation */
     lv_obj_set_style_transform_pivot_x(g.compass_face_ring_img, CATTLE_SCREEN_WIDTH / 2, 0);
     lv_obj_set_style_transform_pivot_y(g.compass_face_ring_img, CATTLE_SCREEN_HEIGHT / 2, 0);

     /* Cache the image for better performance */
     /* Note: lv_img_cache_set_size not available in this LVGL version */

     /* Pre-cache the compass face ring image */
    //  lv_img_cache_invalidate_src(&compass_face_ring);
    //  lv_img_cache_invalidate_src(&compass_face_ring); /* Force cache */

     /* Set rendering optimizations */
     lv_obj_set_style_radius(g.compass_face_ring_img, 0, 0);        /* No rounded corners */
     lv_obj_set_style_border_width(g.compass_face_ring_img, 0, 0);  /* No border */
     lv_obj_set_style_outline_width(g.compass_face_ring_img, 0, 0); /* No outline */
     lv_obj_set_style_shadow_width(g.compass_face_ring_img, 0, 0);  /* No shadow */

     /* Create dynamic interval lines */
     create_interval_lines();

     /* Ensure interval lines are visible on initial load */
     update_interval_lines();

     /* Compass center overlay - fixed, does not rotate */
     g.compass_center_overlay = lv_img_create(g.compass_container);
     lv_img_set_src(g.compass_center_overlay, &compass_center_find);
     lv_obj_set_size(g.compass_center_overlay, 466, 466); /* Set actual image size */
     lv_obj_center(g.compass_center_overlay);
     lv_obj_set_y(g.compass_center_overlay, lv_obj_get_y(g.compass_center_overlay) - 55); /* Move up 10 pixels */
     lv_obj_clear_flag(g.compass_center_overlay, LV_OBJ_FLAG_CLICKABLE);
     lv_obj_clear_flag(g.compass_center_overlay, LV_OBJ_FLAG_SCROLLABLE);

     /* Calibration panel - completely removed */
     g.calib_panel = NULL;
 }

 static void slide_tracking(bool show)
 {
     if (show) {
         lv_obj_add_flag(g.idle_screen, LV_OBJ_FLAG_HIDDEN);
         lv_obj_add_flag(g.sos_screen, LV_OBJ_FLAG_HIDDEN);
         lv_obj_add_flag(g.settings_panel, LV_OBJ_FLAG_HIDDEN);
         lv_obj_add_flag(g.settings_backdrop, LV_OBJ_FLAG_HIDDEN);

         /* Clear hidden flag and position for animation */
         lv_obj_clear_flag(g.tracking_screen, LV_OBJ_FLAG_HIDDEN);
         lv_obj_set_x(g.tracking_screen, CATTLE_SCREEN_WIDTH); /* Start from right side */

         /* Ensure interval lines and targets are properly initialized when tracking screen is shown */
         update_interval_lines();
         render_target_markers();

         /* Safety check - ensure position is never past center */
         lv_coord_t current_x = lv_obj_get_x(g.tracking_screen);
         if (current_x < 0) {
             lv_obj_set_x(g.tracking_screen, 0); /* Force to center if somehow negative */
         }

         /* Enhanced opening animation with easing */
         lv_anim_t a;
         lv_anim_init(&a);
         lv_anim_set_var(&a, g.tracking_screen);
         lv_anim_set_values(&a, CATTLE_SCREEN_WIDTH, 0); /* From right to center (0) - never past center */
         lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
         lv_anim_set_time(&a, 300); /* Optimized for smoother animation */
         lv_anim_set_early_apply(&a, true);
         lv_anim_set_path_cb(&a, lv_anim_path_ease_out); /* Ease out for natural feel */
         lv_anim_start(&a);
     } else {
         /* Enhanced closing animation with easing */
         lv_anim_t a;
         lv_anim_init(&a);
         lv_anim_set_var(&a, g.tracking_screen);
         lv_anim_set_values(&a, 0, CATTLE_SCREEN_WIDTH); /* From center (0) to right - never past center */
         lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
         lv_anim_set_time(&a, 250); /* Optimized for smoother animation */
         lv_anim_set_early_apply(&a, true);
         lv_anim_set_path_cb(&a, lv_anim_path_ease_in); /* Ease in for natural feel */
         lv_anim_set_ready_cb(&a, on_tracking_close_anim_ready);
         lv_anim_start(&a);
     }
 }

 static void on_tracking_drag(lv_event_t *e)
 {
     lv_indev_t *indev = lv_indev_get_act();
     lv_point_t point;
     lv_indev_get_point(indev, &point);

     /* Clamp input coordinates to 466x466 constraint */
     clamp_input_coordinates(&point);

     /* Get current screen position */
     lv_coord_t current_x = lv_obj_get_x(g.tracking_screen);

     /* Calculate drag distance from initial position */
     static lv_coord_t start_x = 0;
     static bool drag_started = false;

     if (lv_event_get_code(e) == LV_EVENT_PRESSING) {
         if (!drag_started) {
             start_x = current_x;
             drag_started = true;
         }

         /* Calculate new position based on drag */
         lv_coord_t drag_delta = point.x - start_x;
         lv_coord_t new_x = start_x + drag_delta;

         /* ABSOLUTE bounds checking - NEVER allow past center (x=0) */
         if (new_x < 0) {
             new_x = 0; /* Hard stop - never go past center */
         }
         if (new_x > CATTLE_SCREEN_WIDTH) {
             new_x = CATTLE_SCREEN_WIDTH;
         }

         /* Update screen position in real-time */
         lv_obj_set_x(g.tracking_screen, new_x);

         /* If dragged left significantly, close the tracking screen */
         if (new_x < CATTLE_SCREEN_WIDTH / 3) {
             // printf("Drag left - closing tracking\n");
             slide_tracking(false);
             drag_started = false;
         }
     } else if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
         if (drag_started) {
             /* Snap back to original position if not closed */
             if (current_x < CATTLE_SCREEN_WIDTH * 2 / 3) {
                 // printf("Drag released - snapping back\n");
                 lv_obj_set_x(g.tracking_screen, 0);
             }

             /* Safety check - ensure position is never past center */
             lv_coord_t final_x = lv_obj_get_x(g.tracking_screen);
             if (final_x < 0) {
                 lv_obj_set_x(g.tracking_screen, 0); /* Force to center if somehow negative */
             }

             drag_started = false;
         }
     }
 }

 static void on_tracking_close_anim_ready(lv_anim_t *anim)
 {
     (void)anim;
     lv_obj_add_flag(g.tracking_screen, LV_OBJ_FLAG_HIDDEN);

     /* Ensure focus returns to keyboard handler */
     if (g.focus_obj) {
         lv_group_focus_obj(g.focus_obj);
     }
 }

 static void set_distance_text(int meters)
 {
     char distance_str[32];

     if (meters >= 1000) {
         /* Display in kilometers */
         float km = meters / 1000.0f;
         snprintf(distance_str, sizeof(distance_str), "%.1fKM", km);
     } else {
         /* Display in meters */
         snprintf(distance_str, sizeof(distance_str), "%dM", meters);
     }

     lv_label_set_text(g.distance_text, distance_str);
 }

 static void update_rotation_text(float yaw_degrees)
 {
     char rotation_str[16];
     int degrees = (int)roundf(yaw_degrees);

     /* Mirror the angle - flip the rotation */
     degrees = 360 - degrees;

     /* Normalize to 0-360 range */
     while (degrees < 0)
         degrees += 360;
     while (degrees >= 360)
         degrees -= 360;

     /* Determine cardinal direction */
     const char *direction;
     if (degrees >= 337.5f || degrees < 22.5f) {
         direction = "N";
     } else if (degrees >= 22.5f && degrees < 67.5f) {
         direction = "NE";
     } else if (degrees >= 67.5f && degrees < 112.5f) {
         direction = "E";
     } else if (degrees >= 112.5f && degrees < 157.5f) {
         direction = "SE";
     } else if (degrees >= 157.5f && degrees < 202.5f) {
         direction = "S";
     } else if (degrees >= 202.5f && degrees < 247.5f) {
         direction = "SW";
     } else if (degrees >= 247.5f && degrees < 292.5f) {
         direction = "W";
     } else {
         direction = "NW";
     }

     snprintf(rotation_str, sizeof(rotation_str), "%d° %s", degrees, direction);
     lv_label_set_text(g.rotation_text, rotation_str);
 }

 static void __attribute__((unused)) update_distance_scale(void)
 {
     /* Update distance text to show current scale */
     set_distance_text(g.distance_scale_meters);

     /* Update map scale based on new distance scale */
     update_map_scale();

     /* Re-render all target markers with new scale */
     render_target_markers();
 }

 void animate_distance_scale(int target_scale)
 {
     /* Stop any existing animation */
     if (g.distance_anim) {
         lv_anim_del(g.distance_anim, NULL);
         g.distance_anim = NULL;
     }

     /* Set target scale */
     g.target_distance_scale = target_scale;

     /* Create new animation using static allocation */
     static lv_anim_t anim;
     g.distance_anim = &anim;
     lv_anim_init(g.distance_anim);
     lv_anim_set_var(g.distance_anim, &g.distance_scale_meters);
     lv_anim_set_values(g.distance_anim, g.distance_scale_meters, target_scale);
     lv_anim_set_exec_cb(g.distance_anim, on_distance_anim_value);
     lv_anim_set_time(g.distance_anim, 500); /* 500ms animation duration */
     lv_anim_set_ready_cb(g.distance_anim, on_distance_anim_ready);
     lv_anim_start(g.distance_anim);
 }

 static void on_distance_anim_value(void *var, int32_t value)
 {
     (void)var;
     /* Update the distance scale with the animated value */
     g.distance_scale_meters = (int)value;

 #if ENABLE_CLOSE_TRACKING
     /* Check for close-range mode transition */
     bool should_be_close_range = (g.distance_scale_meters < 100);
     if (should_be_close_range != g.close_range_mode) {
         g.close_range_mode = should_be_close_range;
         if (g.close_range_mode) {
             show_close_range_mode();
         } else {
             hide_close_range_mode();
         }
     }
 #endif

     /* Performance optimization: Only update UI elements that need it */
     static int32_t last_value = -1;
     if (last_value != value) {
         /* Update distance text and re-render markers during animation */
         set_distance_text(g.distance_scale_meters);
         update_map_scale();
         update_interval_lines();
         render_target_markers();

         /* Force refresh of the compass container to show interval line changes */
         lv_obj_invalidate(g.compass_container);
         last_value = value;
     }
 }

 static void on_distance_anim_ready(lv_anim_t *anim)
 {
     (void)anim;
     /* Animation complete - clean up */
     g.distance_anim = NULL;
 }

 static void update_interval_lines(void)
 {
     /* Calculate screen radius for maximum circle */
     float max_radius =
         (CATTLE_SCREEN_WIDTH < CATTLE_SCREEN_HEIGHT ? CATTLE_SCREEN_WIDTH : CATTLE_SCREEN_HEIGHT) / 2 - 15;
     float min_radius = 20; /* Minimum radius for visible circles */

     /* Calculate the map scale - same as used for target markers */
     static float cached_screen_radius = 0;
     static float cached_map_scale = 0;
     static int cached_distance_scale = 0;

     float screen_radius =
         (CATTLE_SCREEN_WIDTH < CATTLE_SCREEN_HEIGHT ? CATTLE_SCREEN_WIDTH : CATTLE_SCREEN_HEIGHT) / 2 - 60;
     float map_scale;

     /* Cache map scale calculation to avoid redundant computation */
     if (cached_distance_scale != g.distance_scale_meters || cached_screen_radius != screen_radius) {
         cached_screen_radius = screen_radius;
         cached_distance_scale = g.distance_scale_meters;
         cached_map_scale = g.distance_scale_meters / screen_radius;
     }
     map_scale = cached_map_scale;

     /* Dynamic interval generation with even spacing */
     float dynamic_intervals[12];
     int interval_count = 0;

    /* Optimized step size lookup table */
    static const struct {
        float max_distance;
        float step_size;
    } step_lookup[] = {
        {100.0f, 20.0f},         /* 20m steps for small scales */
        {200.0f, 50.0f},         /* 50m steps at 200m scale (doubled tick lines) */
        {500.0f, 100.0f},        /* 100m steps for medium scales (reduced from 50m) */
        {1000.0f, 200.0f},       /* 200m steps at 1KM scale (reduced tick lines by half) */
        {2000.0f, 100.0f},       /* 100m steps for larger scales */
        {10000.0f, 1000.0f},     /* 1km steps for km scales */
        {50000.0f, 5000.0f},     /* 5km steps for larger km scales */
        {200000.0f, 20000.0f},   /* 20km steps for very large scales */
        {999999999.0f, 50000.0f} /* 50km steps for huge scales */
    };

    float total_distance = g.distance_scale_meters;
    float step_size = 50000.0f; /* Default fallback */

    /* Find appropriate step size using lookup table */
    for (int i = 0; i < 9; i++) {
        if (total_distance <= step_lookup[i].max_distance) {
            step_size = step_lookup[i].step_size;
            break;
        }
    }

     /* Generate evenly spaced intervals */
     for (float interval = step_size; interval <= total_distance; interval += step_size) {
         if (interval_count < 12) { /* Limit to prevent overflow */
             dynamic_intervals[interval_count] = interval;
             interval_count++;
         }
     }

     /* Find which intervals to show (3-6 ticks) */
     int visible_count = 0;
     int selected_indices[12] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

     /* Select intervals that fit within screen bounds */
     for (int i = 0; i < interval_count && visible_count < 12; i++) {
         float distance = dynamic_intervals[i];
         float circle_radius = distance / map_scale;

         /* Only select intervals that are within reasonable bounds */
         if (circle_radius >= min_radius && circle_radius <= max_radius) {
             selected_indices[visible_count] = i;
             visible_count++;
         }
     }

     /* Ensure we have at least 3 ticks if possible */
     if (visible_count < 3) {
         /* Try to find at least 3 intervals even if they're slightly outside bounds */
         for (int i = 0; i < interval_count && visible_count < 3; i++) {
             float distance = dynamic_intervals[i];
             float circle_radius = distance / map_scale;

             /* Allow slightly smaller circles to ensure we have at least 3 */
             if (circle_radius >= min_radius * 0.8f && circle_radius <= max_radius * 1.2f) {
                 selected_indices[visible_count] = i;
                 visible_count++;
             }
         }
     }

     /* Performance optimization: Update interval lines without rotation */
     for (int i = 0; i < g.interval_lines_count; i++) {
         if (g.interval_lines[i]) {
             bool should_show = false;
             float circle_radius = 0;

             /* Check if this line index corresponds to a selected interval */
             for (int j = 0; j < visible_count; j++) {
                 if (selected_indices[j] == i) {
                     should_show = true;
                     float distance = dynamic_intervals[selected_indices[j]];
                     circle_radius = distance / map_scale;

                     /* Clamp radius to valid bounds */
                     if (circle_radius < min_radius)
                         circle_radius = min_radius;
                     else if (circle_radius > max_radius)
                         circle_radius = max_radius;
                     break;
                 }
             }

             if (should_show) {

                 int radius_int = (int)circle_radius;
                 int size = radius_int * 2;
                 int pos = CIRCLE_CENTER - radius_int;

                 lv_obj_clear_flag(g.interval_lines[i], LV_OBJ_FLAG_HIDDEN);
                 lv_obj_set_size(g.interval_lines[i], size, size);
                 lv_obj_set_pos(g.interval_lines[i], pos, pos);
                 lv_obj_set_style_radius(g.interval_lines[i], radius_int, 0);

                 /* Performance: Only invalidate if actually changed */
                 static float last_radius[12] = {0};
                 if (last_radius[i] != circle_radius) {
                     lv_obj_invalidate(g.interval_lines[i]);
                     last_radius[i] = circle_radius;
                 }
             } else {
                 lv_obj_add_flag(g.interval_lines[i], LV_OBJ_FLAG_HIDDEN);
             }
         }
     }
 }

 static void create_interval_lines(void)
 {
     /* Clear existing interval lines */
     if (g.interval_lines) {
         for (int i = 0; i < g.interval_lines_count; i++) {
             if (g.interval_lines[i]) {
                 lv_obj_del(g.interval_lines[i]);
             }
         }
         tal_free(g.interval_lines);
         g.interval_lines = NULL;
         g.interval_lines_count = 0;
     }

     /* Allocate space for interval lines (fixed size for efficiency) */
     g.interval_lines_count = 12;
     g.interval_lines = tal_malloc(g.interval_lines_count * sizeof(lv_obj_t *));

     if (!g.interval_lines) {
         g.interval_lines_count = 0;
         return;
     }

     /* Create interval lines - they will be positioned dynamically */
     for (int i = 0; i < g.interval_lines_count; i++) {
         g.interval_lines[i] = lv_obj_create(g.compass_container);
         lv_obj_set_style_bg_opa(g.interval_lines[i], LV_OPA_TRANSP, 0);
         lv_obj_set_style_border_width(g.interval_lines[i], 1, 0);
         lv_obj_set_style_border_color(g.interval_lines[i], lv_color_hex(0x666666), 0);
         lv_obj_set_style_pad_all(g.interval_lines[i], 0, 0);
         lv_obj_clear_flag(g.interval_lines[i], LV_OBJ_FLAG_CLICKABLE);
         lv_obj_clear_flag(g.interval_lines[i], LV_OBJ_FLAG_SCROLLABLE);

         /* Initially hide all lines - they will be shown by update_interval_lines */
         lv_obj_add_flag(g.interval_lines[i], LV_OBJ_FLAG_HIDDEN);
     }

     /* Position the lines dynamically based on current distance scale */
     update_interval_lines();

     /* Force initial refresh */
     lv_obj_invalidate(g.compass_container);
 }

 /* GPS Utility Functions */
 static float calculate_distance(float lat1, float lon1, float lat2, float lon2)
 {
     /* Haversine formula for distance calculation */
     const float R = 6371000.0f; /* Earth radius in meters */
     float dlat = (lat2 - lat1) * M_PI / 180.0f;
     float dlon = (lon2 - lon1) * M_PI / 180.0f;
     float a = sinf(dlat / 2) * sinf(dlat / 2) +
               cosf(lat1 * M_PI / 180.0f) * cosf(lat2 * M_PI / 180.0f) * sinf(dlon / 2) * sinf(dlon / 2);
     float c = 2 * atan2f(sqrtf(a), sqrtf(1 - a));
     return R * c;
 }

 static void add_target_coord(float lat, float lon, uint32_t color)
 {
     if (g.target_count >= MAX_TARGETS)
         return;

     int index = g.target_count;
     g.targets[index].lat = lat;
     g.targets[index].lon = lon;
     g.targets[index].color = color;
     g.targets[index].active = true;
     g.targets[index].distance_meters = (int)calculate_distance(g.self_lat, g.self_lon, lat, lon);

     g.target_count++;
     update_map_scale();
     render_target_markers();
 }

 static void add_target_at_distance(float distance_meters, float bearing_degrees, uint32_t color)
 {
     if (g.target_count >= MAX_TARGETS)
         return;

     /* Convert bearing to radians */
     float bearing_rad = bearing_degrees * M_PI / 180.0f;

     /* Calculate offset in meters */
     float x_meters = distance_meters * sinf(bearing_rad);
     float y_meters = distance_meters * cosf(bearing_rad);

     /* Convert meters to GPS coordinates */
     float lat_factor = 111320.0f;                                    /* meters per degree latitude */
     float lon_factor = 111320.0f * cosf(g.self_lat * M_PI / 180.0f); /* meters per degree longitude */

     float lat_offset = y_meters / lat_factor;
     float lon_offset = x_meters / lon_factor;

     /* Add target at calculated position */
     add_target_coord(g.self_lat + lat_offset, g.self_lon + lon_offset, color);
 }

 static void remove_target_coord(int index)
 {
     if (index < 0 || index >= g.target_count)
         return;

     /* Remove marker from screen */
     if (g.target_markers[index]) {
         lv_obj_del(g.target_markers[index]);
         g.target_markers[index] = NULL;
     }

     /* Shift remaining targets */
     for (int i = index; i < g.target_count - 1; i++) {
         g.targets[i] = g.targets[i + 1];
         g.target_markers[i] = g.target_markers[i + 1];
         g.target_markers[i + 1] = NULL;
     }

     g.target_count--;
     update_map_scale();
     render_target_markers();
 }

 static void clear_all_targets(void)
 {
     for (int i = 0; i < g.target_count; i++) {
         if (g.target_markers[i]) {
             lv_obj_del(g.target_markers[i]);
             g.target_markers[i] = NULL;
         }
     }
     g.target_count = 0;
     update_map_scale();
 }

 static void update_map_scale(void)
 {
     /* Use the current distance scale to determine map scale */
     float screen_radius =
         (CATTLE_SCREEN_WIDTH < CATTLE_SCREEN_HEIGHT ? CATTLE_SCREEN_WIDTH : CATTLE_SCREEN_HEIGHT) / 2 - 50;
     g.map_scale = g.distance_scale_meters / screen_radius;
 }

 static void render_target_markers(void)
 {
 #if ENABLE_CLOSE_TRACKING
     /* Don't render markers in close-range mode */
     if (g.close_range_mode) {
         /* Clear existing markers */
         for (int i = 0; i < MAX_TARGETS; i++) {
             if (g.target_markers[i]) {
                 lv_obj_del(g.target_markers[i]);
                 g.target_markers[i] = NULL;
             }
         }
         return;
     }
 #endif

     /* Clear existing markers */
     for (int i = 0; i < MAX_TARGETS; i++) {
         if (g.target_markers[i]) {
             lv_obj_del(g.target_markers[i]);
             g.target_markers[i] = NULL;
         }
     }

     /* Calculate screen radius for boundary checking */
     float screen_radius =
         (CATTLE_SCREEN_WIDTH < CATTLE_SCREEN_HEIGHT ? CATTLE_SCREEN_WIDTH : CATTLE_SCREEN_HEIGHT) / 2 - 50;

     /* Create new markers */
     for (int i = 0; i < g.target_count; i++) {
         if (!g.targets[i].active)
             continue;

         /* Calculate relative position */
         float delta_lat = g.targets[i].lat - g.self_lat;
         float delta_lon = g.targets[i].lon - g.self_lon;

         /* Convert to screen coordinates (meters to pixels) - dynamic calculation */
         float lat_factor = 111320.0f; /* meters per degree latitude */
         float lon_factor =
             111320.0f * cosf(g.self_lat * M_PI / 180.0f); /* meters per degree longitude at current latitude */

         float x_meters = delta_lon * lon_factor;
         float y_meters = delta_lat * lat_factor;

         /* Apply compass rotation to target positions (negate angle for correct direction) */
         float angle_rad = -g.yaw_deg * M_PI / 180.0f;
         float cos_angle = cosf(angle_rad);
         float sin_angle = sinf(angle_rad);

         /* Rotate the target position relative to compass */
         float rotated_x = x_meters * cos_angle - y_meters * sin_angle;
         float rotated_y = x_meters * sin_angle + y_meters * cos_angle;

         /* Scale to screen coordinates */
         float x_pixels = rotated_x / g.map_scale;
         float y_pixels = -rotated_y / g.map_scale; /* Negative for screen coordinates */

         /* Calculate distance from center */
         float distance_from_center = sqrtf(x_pixels * x_pixels + y_pixels * y_pixels);

         /* Create marker - special handling for cow target */
         if (g.targets[i].color == TARGET_COLOR_COW) {
             /* Create cow image marker with hollow white circle background */
             g.target_markers[i] = lv_obj_create(g.map_container);
             lv_obj_set_size(g.target_markers[i], 48, 48);
             lv_obj_set_style_radius(g.target_markers[i], 24, 0);
             lv_obj_set_style_bg_opa(g.target_markers[i], LV_OPA_TRANSP, 0); /* Transparent background */
             lv_obj_set_style_border_width(g.target_markers[i], 1, 0);       /* White border for hollow circle */
             lv_obj_set_style_border_color(g.target_markers[i], lv_color_white(), 0);

             /* Disable scrolling for cow target container */
             lv_obj_clear_flag(g.target_markers[i], LV_OBJ_FLAG_SCROLLABLE);
             lv_obj_clear_flag(g.target_markers[i], LV_OBJ_FLAG_SCROLL_ELASTIC);
             lv_obj_clear_flag(g.target_markers[i], LV_OBJ_FLAG_SCROLL_MOMENTUM);
             lv_obj_clear_flag(g.target_markers[i], LV_OBJ_FLAG_SCROLL_ONE);
             lv_obj_clear_flag(g.target_markers[i], LV_OBJ_FLAG_SCROLL_CHAIN);

             /* Create cow image as child of the circle */
             lv_obj_t *cow_img = lv_img_create(g.target_markers[i]);
             lv_img_set_src(cow_img, &compass_cow_loc);
             lv_obj_set_size(cow_img, 48, 48);
             lv_obj_center(cow_img);

             /* Disable scrolling for cow image */
             lv_obj_clear_flag(cow_img, LV_OBJ_FLAG_SCROLLABLE);
             lv_obj_clear_flag(cow_img, LV_OBJ_FLAG_SCROLL_ELASTIC);
             lv_obj_clear_flag(cow_img, LV_OBJ_FLAG_SCROLL_MOMENTUM);
             lv_obj_clear_flag(cow_img, LV_OBJ_FLAG_SCROLL_ONE);
             lv_obj_clear_flag(cow_img, LV_OBJ_FLAG_SCROLL_CHAIN);
         } else {
             /* Create regular colored circle marker */
             g.target_markers[i] = lv_obj_create(g.map_container);
             lv_obj_set_size(g.target_markers[i], 10, 10);
             lv_obj_set_style_bg_color(g.target_markers[i], lv_color_hex(g.targets[i].color), 0);
             lv_obj_set_style_radius(g.target_markers[i], 5, 0);
         }

         /* Calculate offset based on marker type */
         int offset_x, offset_y;
         if (g.targets[i].color == TARGET_COLOR_COW) {
             offset_x = 24; /* Half of 48px cow image */
             offset_y = 24;
         } else {
             offset_x = 5; /* Half of 10px circle */
             offset_y = 5;
         }

         if (distance_from_center > screen_radius) {
             /* Target exceeds circle - position at boundary and add white border */
             float angle = atan2f(y_pixels, x_pixels);
             float boundary_x = cosf(angle) * screen_radius;
             float boundary_y = sinf(angle) * screen_radius;

             lv_obj_set_pos(g.target_markers[i], CATTLE_SCREEN_WIDTH / 2 + boundary_x - offset_x,
                            CATTLE_SCREEN_HEIGHT / 2 + boundary_y - offset_y);

             /* Add border for targets beyond circle */
             if (g.targets[i].color == TARGET_COLOR_COW) {
                 /* Cow target gets thicker 3px white border when beyond circle */
                 lv_obj_set_style_border_width(g.target_markers[i], 3, 0);
                 lv_obj_set_style_border_color(g.target_markers[i], lv_color_white(), 0);
             } else {
                 /* Regular markers get 1px white border when beyond circle */
                 lv_obj_set_style_border_width(g.target_markers[i], 1, 0);
                 lv_obj_set_style_border_color(g.target_markers[i], lv_color_white(), 0);
             }
         } else {
             /* Target is within circle - normal positioning */
             lv_obj_set_pos(g.target_markers[i], CATTLE_SCREEN_WIDTH / 2 + x_pixels - offset_x,
                            CATTLE_SCREEN_HEIGHT / 2 + y_pixels - offset_y);

             /* No border for targets within circle */
             lv_obj_set_style_border_width(g.target_markers[i], 0, 0);
         }
     }
 }

 static void update_target_positions(void)
 {
 #if ENABLE_CLOSE_TRACKING
     /* Don't update markers in close-range mode */
     if (g.close_range_mode) {
         return;
     }
 #endif

     /* Only update positions of existing markers - don't recreate them */
     if (g.target_count == 0)
         return;

     /* Update cached coordinates if GPS data has changed */
     update_cached_coordinates();

     /* Pre-calculate rotation values once for all targets */
     float angle_rad = -g.yaw_deg * M_PI / 180.0f;
     float cos_angle = cosf(angle_rad);
     float sin_angle = sinf(angle_rad);

     /* Use cached screen radius */
     float screen_radius = g.cached_screen_radius;

     /* Update existing markers using cached coordinates */
     for (int i = 0; i < g.target_count; i++) {
         if (!g.targets[i].active || !g.target_markers[i])
             continue;

         /* Use cached meter coordinates if available and not dirty */
         float x_meters, y_meters;
         if (!g.targets[i].coordinates_dirty) {
             x_meters = g.targets[i].cached_x_meters;
             y_meters = g.targets[i].cached_y_meters;
         } else {
             /* Fallback to calculation if cache is dirty */
             float delta_lat = g.targets[i].lat - g.self_lat;
             float delta_lon = g.targets[i].lon - g.self_lon;
             x_meters = delta_lon * g.cached_lon_factor;
             y_meters = delta_lat * g.cached_lat_factor;
         }

         /* Rotate the target position relative to compass */
         float rotated_x = x_meters * cos_angle - y_meters * sin_angle;
         float rotated_y = x_meters * sin_angle + y_meters * cos_angle;

         /* Scale to screen coordinates */
         float x_pixels = rotated_x / g.map_scale;
         float y_pixels = -rotated_y / g.map_scale; /* Negative for screen coordinates */

         /* Calculate distance from center */
         float distance_from_center = sqrtf(x_pixels * x_pixels + y_pixels * y_pixels);

         /* Calculate offset based on marker type */
         int offset_x, offset_y;
         if (g.targets[i].color == TARGET_COLOR_COW) {
             offset_x = 24; /* Half of 48px cow image */
             offset_y = 24;
         } else {
             offset_x = 5; /* Half of 10px circle */
             offset_y = 5;
         }

         if (distance_from_center > screen_radius) {
             /* Target exceeds circle - position at boundary */
             float angle = atan2f(y_pixels, x_pixels);
             float boundary_x = cosf(angle) * screen_radius;
             float boundary_y = sinf(angle) * screen_radius;

             lv_obj_set_pos(g.target_markers[i], CATTLE_SCREEN_WIDTH / 2 + boundary_x - offset_x,
                            CATTLE_SCREEN_HEIGHT / 2 + boundary_y - offset_y);

             /* Add border for targets beyond circle */
             if (g.targets[i].color == TARGET_COLOR_COW) {
                 /* Cow target gets thicker 3px white border when beyond circle */
                 lv_obj_set_style_border_width(g.target_markers[i], 3, 0);
                 lv_obj_set_style_border_color(g.target_markers[i], lv_color_white(), 0);
             } else {
                 /* Regular markers get 1px white border when beyond circle */
                 lv_obj_set_style_border_width(g.target_markers[i], 1, 0);
                 lv_obj_set_style_border_color(g.target_markers[i], lv_color_white(), 0);
             }
         } else {
             /* Target is within circle - normal positioning */
             lv_obj_set_pos(g.target_markers[i], CATTLE_SCREEN_WIDTH / 2 + x_pixels - offset_x,
                            CATTLE_SCREEN_HEIGHT / 2 + y_pixels - offset_y);

             /* No border for targets within circle */
             lv_obj_set_style_border_width(g.target_markers[i], 0, 0);
         }
     }
 }

 static void update_target_positions_for_new_origin(void)
 {
 #if ENABLE_CLOSE_TRACKING
     /* Don't update markers in close-range mode */
     if (g.close_range_mode) {
         return;
     }
 #endif

     /* Recalculate all target positions based on new tracker origin */
     for (int i = 0; i < g.target_count; i++) {
         if (!g.targets[i].active)
             continue;

         /* Recalculate distance from new origin */
         g.targets[i].distance_meters =
             (int)calculate_distance(g.self_lat, g.self_lon, g.targets[i].lat, g.targets[i].lon);
     }

     /* Mark all coordinates as dirty when origin changes */
     mark_all_coordinates_dirty();

     /* Update map scale and re-render markers */
     update_map_scale();
     render_target_markers();
 }

 /* Performance optimization: Update cached coordinates only when GPS data changes */
 static void update_cached_coordinates(void)
 {
     if (!g.gps_data_dirty)
         return;

     /* Update cached factors for current GPS position */
     g.cached_lat_factor = 111320.0f; /* meters per degree latitude */
     g.cached_lon_factor = 111320.0f * cosf(g.self_lat * M_PI / 180.0f);

     /* Update cached screen radius */
     g.cached_screen_radius =
         (CATTLE_SCREEN_WIDTH < CATTLE_SCREEN_HEIGHT ? CATTLE_SCREEN_WIDTH : CATTLE_SCREEN_HEIGHT) / 2 - 50;

     /* Update cached coordinates for all active targets */
     for (int i = 0; i < g.target_count; i++) {
         if (!g.targets[i].active)
             continue;

         /* Calculate relative position in meters */
         float delta_lat = g.targets[i].lat - g.self_lat;
         float delta_lon = g.targets[i].lon - g.self_lon;

         g.targets[i].cached_x_meters = delta_lon * g.cached_lon_factor;
         g.targets[i].cached_y_meters = delta_lat * g.cached_lat_factor;
         g.targets[i].coordinates_dirty = false;
     }

     g.gps_data_dirty = false;
 }

 /* Mark all target coordinates as dirty when GPS data changes */
 static void mark_all_coordinates_dirty(void)
 {
     for (int i = 0; i < g.target_count; i++) {
         g.targets[i].coordinates_dirty = true;
     }
     g.gps_data_dirty = true;
 }

 /* Public function to mark GPS data as dirty when it changes */
 void gps_mark_data_dirty(void)
 {
     mark_all_coordinates_dirty();
 }

 static void create_tracking_screen(void)
 {
     g.tracking_screen = lv_obj_create(g.screen);
     lv_obj_remove_style_all(g.tracking_screen);
     lv_obj_set_size(g.tracking_screen, CATTLE_SCREEN_WIDTH, CATTLE_SCREEN_HEIGHT);
     lv_obj_set_style_bg_opa(g.tracking_screen, LV_OPA_TRANSP, 0);
     lv_obj_clear_flag(g.tracking_screen, LV_OBJ_FLAG_SCROLLABLE);

    /* Add circular masking to tracking screen */
    lv_obj_set_style_radius(g.tracking_screen, CIRCLE_RADIUS, 0);
    lv_obj_set_style_clip_corner(g.tracking_screen, true, 0);

     /* Disable all scrolling flags to enforce 466x466 constraint and prevent scrolling during swipe gestures */
     lv_obj_clear_flag(g.tracking_screen, LV_OBJ_FLAG_SCROLL_ELASTIC);
     lv_obj_clear_flag(g.tracking_screen, LV_OBJ_FLAG_SCROLL_MOMENTUM);
     lv_obj_clear_flag(g.tracking_screen, LV_OBJ_FLAG_SCROLL_ONE);
     lv_obj_clear_flag(g.tracking_screen, LV_OBJ_FLAG_SCROLL_CHAIN);

     /* Add drag functionality to tracking screen */
     lv_obj_add_event_cb(g.tracking_screen, on_tracking_drag, LV_EVENT_PRESSING, NULL);
     lv_obj_add_event_cb(g.tracking_screen, on_tracking_drag, LV_EVENT_RELEASED, NULL);

     compass_build(g.tracking_screen);

     /* Create map container for GPS targets */
     g.map_container = lv_obj_create(g.tracking_screen);
     lv_obj_remove_style_all(g.map_container);
     lv_obj_set_size(g.map_container, CATTLE_SCREEN_WIDTH, CATTLE_SCREEN_HEIGHT);
     lv_obj_set_style_bg_opa(g.map_container, LV_OPA_TRANSP, 0);
     lv_obj_set_style_border_width(g.map_container, 0, 0);
     lv_obj_set_style_pad_all(g.map_container, 0, 0);
     lv_obj_clear_flag(g.map_container, LV_OBJ_FLAG_CLICKABLE);
     lv_obj_clear_flag(g.map_container, LV_OBJ_FLAG_SCROLLABLE);

    /* Add circular masking to map container */
    lv_obj_set_style_radius(g.map_container, CIRCLE_RADIUS, 0);
    lv_obj_set_style_clip_corner(g.map_container, true, 0);

     /* Disable all scrolling flags to enforce 466x466 constraint */
     lv_obj_clear_flag(g.map_container, LV_OBJ_FLAG_SCROLL_ELASTIC);
     lv_obj_clear_flag(g.map_container, LV_OBJ_FLAG_SCROLL_MOMENTUM);
     lv_obj_clear_flag(g.map_container, LV_OBJ_FLAG_SCROLL_ONE);
     lv_obj_clear_flag(g.map_container, LV_OBJ_FLAG_SCROLL_CHAIN);

     lv_obj_center(g.map_container);

     /* Initialize dummy GPS data from data structure */
     g.self_lat = DUMMY_SELF_LAT;
     g.self_lon = DUMMY_SELF_LON;
     g.target_count = 0;
     g.map_scale = 1.0f;

     /* Add dummy targets from data structure using actual GPS coordinates */
    // TODO: test
    //  for (int i = 0; i < (int)DUMMY_TARGET_COUNT; i++) {
    //      add_target_coord(DUMMY_TARGETS[i].lat, DUMMY_TARGETS[i].lon, DUMMY_TARGETS[i].color);
    //  }

     /* Initialize map scale - targets will be rendered after distance scale is set */
     update_map_scale();

     /* Add distance image - positioned at center with 10px right offset - ON TOP LAYER */
     g.distance_img = lv_img_create(g.tracking_screen);
     lv_img_set_src(g.distance_img, &diatance);
     lv_obj_set_size(g.distance_img, 157, 14); /* Match the image dimensions */
     lv_obj_center(g.distance_img);
     lv_obj_set_x(g.distance_img, lv_obj_get_x(g.distance_img) + 95); /* Offset 10px to the right */

     /* Add distance text label under the arrow */
     g.distance_text = lv_label_create(g.tracking_screen);
     lv_obj_set_style_text_color(g.distance_text, lv_color_white(), 0);
     lv_obj_set_style_text_font(g.distance_text, &lv_font_montserrat_24, 0);
     lv_obj_center(g.distance_text);
     lv_obj_set_y(g.distance_text, lv_obj_get_y(g.distance_img) + 18); /* Position under the arrow */
     lv_obj_set_x(g.distance_text, lv_obj_get_x(g.distance_img) + 95); /* Position under the arrow */

     /* Initialize distance to 200M */
     g.distance_meters = 200;
     g.distance_scale_meters = 200; /* Default scale */
     set_distance_text(g.distance_meters);

     /* Now render targets with the correct 200M scale */
     update_map_scale();
     render_target_markers();

     /* Create rotation display at lower bottom */
     g.rotation_bg = lv_obj_create(g.tracking_screen);
     lv_obj_set_size(g.rotation_bg, 100, 32); /* Slightly tighter - reduced from 120x40 to 100x32 */
     lv_obj_set_style_bg_color(g.rotation_bg, lv_color_hex(0x404040), 0); /* Slight grayish color */
     lv_obj_set_style_bg_opa(g.rotation_bg, LV_OPA_80, 0);
     lv_obj_set_style_radius(g.rotation_bg, 16, 0); /* Adjusted radius for smaller size */
     lv_obj_set_style_border_width(g.rotation_bg, 0, 0);
     lv_obj_set_style_pad_all(g.rotation_bg, 6, 0);            /* Reduced padding from 8 to 6 */
     lv_obj_align(g.rotation_bg, LV_ALIGN_BOTTOM_MID, 0, -75); /* Move 35px higher (from -20 to -55) */

     g.rotation_text = lv_label_create(g.rotation_bg);
     lv_obj_set_style_text_color(g.rotation_text, lv_color_white(), 0);
     lv_obj_set_style_text_font(g.rotation_text, &lv_font_montserrat_16, 0);
     lv_obj_center(g.rotation_text);

     /* Initialize rotation display */
     update_rotation_text(g.yaw_deg);

     /* Ensure all elements are on top layer by moving them to front */
     lv_obj_move_foreground(g.distance_img);
     lv_obj_move_foreground(g.distance_text);
     lv_obj_move_foreground(g.rotation_bg);
 }

 static void create_settings_panel(void)
 {
     /* Backdrop */
     g.settings_backdrop = lv_obj_create(g.screen);
     lv_obj_remove_style_all(g.settings_backdrop);
     lv_obj_set_size(g.settings_backdrop, CATTLE_SCREEN_WIDTH, CATTLE_SCREEN_HEIGHT);
     lv_obj_set_style_bg_color(g.settings_backdrop, lv_color_black(), 0);
     lv_obj_set_style_bg_opa(g.settings_backdrop, LV_OPA_30, 0);
     lv_obj_add_flag(g.settings_backdrop, LV_OBJ_FLAG_HIDDEN);
     lv_obj_add_event_cb(g.settings_backdrop, on_settings_backdrop_click, LV_EVENT_CLICKED, NULL);
     /* Don't let backdrop capture keyboard events */
     lv_obj_clear_flag(g.settings_backdrop, LV_OBJ_FLAG_CLICKABLE);

     g.settings_panel = lv_obj_create(g.screen);
     lv_obj_set_size(g.settings_panel, CATTLE_SCREEN_WIDTH, SETTINGS_PANEL_HEIGHT);
     lv_obj_align(g.settings_panel, LV_ALIGN_TOP_MID, 0, -SETTINGS_PANEL_HEIGHT);
    lv_obj_set_style_radius(g.settings_panel, CIRCLE_RADIUS, 0);
    lv_obj_set_style_clip_corner(g.settings_panel, true, 0);
    lv_obj_set_style_bg_color(g.settings_panel, lv_color_black(), 0);
     lv_obj_set_style_bg_opa(g.settings_panel, LV_OPA_COVER, 0);
     lv_obj_set_style_border_width(g.settings_panel, 0, 0);
     lv_obj_set_style_pad_all(g.settings_panel, 0, 0);
     /* Don't let settings panel capture keyboard events */
     lv_obj_clear_flag(g.settings_panel, LV_OBJ_FLAG_CLICKABLE);

     /* Disable scrolling on settings panel to enforce 466x466 constraint */
     lv_obj_clear_flag(g.settings_panel, LV_OBJ_FLAG_SCROLLABLE);
     lv_obj_clear_flag(g.settings_panel, LV_OBJ_FLAG_SCROLL_ELASTIC);
     lv_obj_clear_flag(g.settings_panel, LV_OBJ_FLAG_SCROLL_MOMENTUM);
     lv_obj_clear_flag(g.settings_panel, LV_OBJ_FLAG_SCROLL_ONE);
     lv_obj_clear_flag(g.settings_panel, LV_OBJ_FLAG_SCROLL_CHAIN);

   /* GPS Status at top with icon */
   g.gps_icon_img = lv_img_create(g.settings_panel);
   lv_img_set_src(g.gps_icon_img, &gps_icon);
   lv_obj_align(g.gps_icon_img, LV_ALIGN_TOP_MID, -30, 35);  /* Moved up by 5px, adjusted for image size */

     lv_obj_t *gps_label = lv_label_create(g.settings_panel);
    lv_label_set_text(gps_label, "GPS状态");
    lv_obj_set_style_text_color(gps_label, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_text_font(gps_label, &font_puhui_18_2, 0);
    lv_obj_align(gps_label, LV_ALIGN_TOP_MID, 20, 35);  /* Moved up by 5px */

    g.settings_gps_sats_label = lv_label_create(g.settings_panel);
    lv_label_set_text(g.settings_gps_sats_label, "0 颗卫星");
    lv_obj_set_style_text_color(g.settings_gps_sats_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(g.settings_gps_sats_label, &font_puhui_18_2, 0);
    lv_obj_align(g.settings_gps_sats_label, LV_ALIGN_TOP_MID, 0, 65);  /* Moved up by 5px */

    /* Large centered clock display */
    g.settings_date_label = lv_label_create(g.settings_panel);
    lv_label_set_text(g.settings_date_label, "1970 / 01 / 01");
    lv_obj_set_style_text_color(g.settings_date_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(g.settings_date_label, &lv_font_montserrat_20, 0);  /* Use montserrat_20 instead of 22 */
    lv_obj_set_style_text_align(g.settings_date_label, LV_TEXT_ALIGN_LEFT, 0);  /* Align to left */
    lv_obj_align(g.settings_date_label, LV_ALIGN_CENTER, -95, -65);  // Move to left by 20px

    g.settings_time_label = lv_label_create(g.settings_panel);
    lv_label_set_text(g.settings_time_label, "12:00");
    lv_obj_set_style_text_color(g.settings_time_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(g.settings_time_label, &montserrat_time_82_extra_bold, 0);  /* Larger font */
    lv_obj_align(g.settings_time_label, LV_ALIGN_CENTER, -50, 0);  // Moved left by 20px

   /* Status icons on the right side - images already have circle backgrounds */
   /* 4G/WiFi network icon */
   g.network_icon_img = lv_img_create(g.settings_panel);
   lv_img_set_src(g.network_icon_img, &_4g_enable);  /* Default to 4G enabled */
   lv_obj_align(g.network_icon_img, LV_ALIGN_RIGHT_MID, -65, -60);  /* 15px right, 10px up */

   /* Battery icon */
   g.battery_icon_img = lv_img_create(g.settings_panel);
   lv_img_set_src(g.battery_icon_img, &battery_full);  /* Default to full battery */
   lv_obj_align(g.battery_icon_img, LV_ALIGN_RIGHT_MID, -65, 30);  /* 15px right, 10px up */

    /* Volume section at bottom */
    lv_obj_t *vol_icon = lv_label_create(g.settings_panel);
    lv_label_set_text(vol_icon, LV_SYMBOL_VOLUME_MAX);
    lv_obj_set_style_text_color(vol_icon, lv_color_hex(0x1E90FF), 0);
    lv_obj_set_style_text_font(vol_icon, &lv_font_montserrat_20, 0);
    lv_obj_align(vol_icon, LV_ALIGN_BOTTOM_LEFT, 85, -135);  /* Moved up by 15px */

     lv_obj_t *vol_label = lv_label_create(g.settings_panel);
    lv_label_set_text(vol_label, "音量");
    lv_obj_set_style_text_color(vol_label, lv_color_hex(0x1E90FF), 0);
    lv_obj_set_style_text_font(vol_label, &font_puhui_18_2, 0);
    lv_obj_align(vol_label, LV_ALIGN_BOTTOM_LEFT, 120, -135);  /* Moved up by 15px */

    /* Create custom styled slider matching the design */
    g.settings_volume_slider = lv_slider_create(g.settings_panel);
    lv_obj_set_width(g.settings_volume_slider, 280);  /* Shorter slider */
    lv_obj_set_height(g.settings_volume_slider, 10);
    lv_obj_align(g.settings_volume_slider, LV_ALIGN_BOTTOM_MID, 0, -105);  /* Moved up by 15px */
    lv_slider_set_range(g.settings_volume_slider, 0, 100);
    g.current_volume = ai_audio_get_volume();
    lv_slider_set_value(g.settings_volume_slider, g.current_volume, LV_ANIM_OFF);

    /* Style for the main track (background/unfilled part) */
    lv_obj_set_style_bg_opa(g.settings_volume_slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(g.settings_volume_slider, lv_color_hex(0x2D3E50), LV_PART_MAIN);
    lv_obj_set_style_radius(g.settings_volume_slider, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(g.settings_volume_slider, -2, LV_PART_MAIN); /* Makes indicator slightly larger */

    /* Style for the indicator (filled/blue part) */
    lv_obj_set_style_bg_opa(g.settings_volume_slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(g.settings_volume_slider, lv_color_hex(0x1E90FF), LV_PART_INDICATOR);
    lv_obj_set_style_radius(g.settings_volume_slider, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);

    /* Style for the knob (circular white handle) */
    lv_obj_set_style_bg_opa(g.settings_volume_slider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_bg_color(g.settings_volume_slider, lv_color_white(), LV_PART_KNOB);
    lv_obj_set_style_border_width(g.settings_volume_slider, 0, LV_PART_KNOB);
    lv_obj_set_style_radius(g.settings_volume_slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(g.settings_volume_slider, 10, LV_PART_KNOB); /* Makes knob larger */
    lv_obj_set_style_shadow_width(g.settings_volume_slider, 8, LV_PART_KNOB);
    lv_obj_set_style_shadow_color(g.settings_volume_slider, lv_color_black(), LV_PART_KNOB);
    lv_obj_set_style_shadow_opa(g.settings_volume_slider, LV_OPA_20, LV_PART_KNOB);
    
    /* Add event handler for volume changes */
    lv_obj_add_event_cb(g.settings_volume_slider, on_volume_slider_changed, LV_EVENT_VALUE_CHANGED, NULL);


    /* Draggable tab at very bottom */
     g.settings_drag_tab = lv_obj_create(g.settings_panel);
    lv_obj_set_size(g.settings_drag_tab, 60, 6);
    lv_obj_align(g.settings_drag_tab, LV_ALIGN_BOTTOM_MID, 0, -15);
    lv_obj_set_style_bg_color(g.settings_drag_tab, lv_color_hex(0x444444), 0);
    lv_obj_set_style_radius(g.settings_drag_tab, 3, 0);
     lv_obj_set_style_border_width(g.settings_drag_tab, 0, 0);
     lv_obj_add_event_cb(g.settings_drag_tab, on_settings_drag, LV_EVENT_PRESSING, NULL);
     lv_obj_add_event_cb(g.settings_drag_tab, on_settings_drag, LV_EVENT_RELEASED, NULL);
 }

 static void create_sos_screen(void)
 {
     g.sos_screen = lv_obj_create(g.screen);
     lv_obj_remove_style_all(g.sos_screen);
     lv_obj_set_size(g.sos_screen, CATTLE_SCREEN_WIDTH, CATTLE_SCREEN_HEIGHT);
     lv_obj_set_style_bg_color(g.sos_screen, lv_color_hex(0x2a1a1a), 0);
     lv_obj_set_style_bg_opa(g.sos_screen, LV_OPA_COVER, 0);
     lv_obj_clear_flag(g.sos_screen, LV_OBJ_FLAG_SCROLLABLE);

     /* Disable all scrolling flags to enforce 466x466 constraint */
     lv_obj_clear_flag(g.sos_screen, LV_OBJ_FLAG_SCROLL_ELASTIC);
     lv_obj_clear_flag(g.sos_screen, LV_OBJ_FLAG_SCROLL_MOMENTUM);
     lv_obj_clear_flag(g.sos_screen, LV_OBJ_FLAG_SCROLL_ONE);
     lv_obj_clear_flag(g.sos_screen, LV_OBJ_FLAG_SCROLL_CHAIN);

     lv_obj_add_flag(g.sos_screen, LV_OBJ_FLAG_HIDDEN);

     lv_obj_t *title = lv_label_create(g.sos_screen);
     lv_label_set_text(title, "SOS Active\n\nPress 'X' to cancel");
     lv_obj_set_style_text_color(title, lv_color_hex(0xffeaea), 0);
     lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
     lv_obj_center(title);

     /* Hold progress ring (shown during long press on main/root) */
     g.sos_hold_ring = lv_arc_create(g.screen);
     lv_obj_set_size(g.sos_hold_ring, CATTLE_SCREEN_WIDTH - 90, CATTLE_SCREEN_HEIGHT - 90);
     lv_obj_center(g.sos_hold_ring);
     lv_arc_set_rotation(g.sos_hold_ring, 270);
     lv_arc_set_bg_angles(g.sos_hold_ring, 0, 360);
     lv_arc_set_value(g.sos_hold_ring, 0);
     lv_obj_set_style_arc_color(g.sos_hold_ring, lv_color_hex(0x333a), LV_PART_MAIN);
     lv_obj_set_style_arc_width(g.sos_hold_ring, 10, LV_PART_MAIN);
     lv_obj_set_style_arc_color(g.sos_hold_ring, lv_color_hex(0xff5555), LV_PART_INDICATOR);
     lv_obj_set_style_arc_width(g.sos_hold_ring, 12, LV_PART_INDICATOR);
     lv_obj_add_flag(g.sos_hold_ring, LV_OBJ_FLAG_HIDDEN);
 }

 static void show_idle(void)
 {
     lv_obj_add_flag(g.tracking_screen, LV_OBJ_FLAG_HIDDEN);
     lv_obj_add_flag(g.sos_screen, LV_OBJ_FLAG_HIDDEN);
     lv_obj_add_flag(g.settings_panel, LV_OBJ_FLAG_HIDDEN);
     lv_obj_add_flag(g.settings_backdrop, LV_OBJ_FLAG_HIDDEN);

     /* Clear hidden flag and position for animation */
     lv_obj_clear_flag(g.idle_screen, LV_OBJ_FLAG_HIDDEN);
     lv_obj_set_x(g.idle_screen, -CATTLE_SCREEN_WIDTH); /* Start from left side */

     /* Animate idle screen sliding in from left with settings-style animation */
     lv_anim_t a;
     lv_anim_init(&a);
     lv_anim_set_var(&a, g.idle_screen);
     lv_anim_set_values(&a, -CATTLE_SCREEN_WIDTH, 0);
     lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
     lv_anim_set_time(&a, 400); /* Match settings panel timing */
     lv_anim_set_early_apply(&a, true);
     lv_anim_set_path_cb(&a, lv_anim_path_ease_out); /* Same easing as settings */
     lv_anim_start(&a);
 }

 static void __attribute__((unused)) show_tracking(void)
 {
     slide_tracking(true);
 }

 static void show_sos_alert(void)
 {
     g.sos_active = true;
     lv_obj_add_flag(g.sos_hold_ring, LV_OBJ_FLAG_HIDDEN);
     lv_obj_add_flag(g.idle_screen, LV_OBJ_FLAG_HIDDEN);
     lv_obj_add_flag(g.tracking_screen, LV_OBJ_FLAG_HIDDEN);
     lv_obj_clear_flag(g.sos_screen, LV_OBJ_FLAG_HIDDEN);
     lv_obj_add_flag(g.settings_panel, LV_OBJ_FLAG_HIDDEN);
     lv_obj_add_flag(g.settings_backdrop, LV_OBJ_FLAG_HIDDEN);
 }

 static void hide_sos_alert(void)
 {
     g.sos_active = false;
     lv_obj_add_flag(g.sos_screen, LV_OBJ_FLAG_HIDDEN);
 }

 static void slide_settings(bool open)
 {
     if (open) {
         lv_obj_clear_flag(g.settings_backdrop, LV_OBJ_FLAG_HIDDEN);
         lv_obj_clear_flag(g.settings_panel, LV_OBJ_FLAG_HIDDEN);

         /* Enhanced opening animation with easing */
         lv_anim_t a;
         lv_anim_init(&a);
         lv_anim_set_var(&a, g.settings_panel);
         lv_anim_set_values(&a, -SETTINGS_PANEL_HEIGHT, 0);
         lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
         lv_anim_set_time(&a, 400); /* Slightly longer for smoother animation */
         lv_anim_set_early_apply(&a, true);
         lv_anim_set_path_cb(&a, lv_anim_path_ease_out); /* Ease out for natural feel */
         lv_anim_start(&a);

         /* Fade in backdrop */
         lv_anim_t b;
         lv_anim_init(&b);
         lv_anim_set_var(&b, g.settings_backdrop);
         lv_anim_set_values(&b, LV_OPA_TRANSP, LV_OPA_30);
         lv_anim_set_exec_cb(&b, on_bg_opa_anim);
         lv_anim_set_time(&b, 300);
         lv_anim_set_early_apply(&b, true);
         lv_anim_start(&b);

     } else {
         /* Enhanced closing animation with easing */
         lv_anim_t a;
         lv_anim_init(&a);
         lv_anim_set_var(&a, g.settings_panel);
         lv_anim_set_values(&a, 0, -SETTINGS_PANEL_HEIGHT);
         lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
         lv_anim_set_time(&a, 350); /* Slightly longer for smoother animation */
         lv_anim_set_early_apply(&a, true);
         lv_anim_set_path_cb(&a, lv_anim_path_ease_in); /* Ease in for natural feel */
         lv_anim_set_ready_cb(&a, on_settings_close_anim_ready);
         lv_anim_start(&a);

         /* Fade out backdrop */
         lv_anim_t b;
         lv_anim_init(&b);
         lv_anim_set_var(&b, g.settings_backdrop);
         lv_anim_set_values(&b, LV_OPA_30, LV_OPA_TRANSP);
         lv_anim_set_exec_cb(&b, on_bg_opa_anim);
         lv_anim_set_time(&b, 300);
         lv_anim_set_early_apply(&b, true);
         lv_anim_start(&b);
     }
 }

 static void on_settings_drag(lv_event_t *e)
 {
     lv_indev_t *indev = lv_indev_get_act();
     lv_point_t point;
     lv_indev_get_point(indev, &point);

     /* Clamp input coordinates to 466x466 constraint */
     clamp_input_coordinates(&point);

     /* Get current panel position */
     lv_coord_t current_y = lv_obj_get_y(g.settings_panel);

     /* Calculate drag distance from initial position */
     static lv_coord_t start_y = 0;
     static bool drag_started = false;

     if (lv_event_get_code(e) == LV_EVENT_PRESSING) {
         if (!drag_started) {
             start_y = current_y;
             drag_started = true;
         }

         /* Calculate new position based on drag */
         lv_coord_t drag_delta = point.y - start_y;
         lv_coord_t new_y = start_y + drag_delta;

         /* Limit movement to reasonable bounds */
         if (new_y < -SETTINGS_PANEL_HEIGHT)
             new_y = -SETTINGS_PANEL_HEIGHT;
         if (new_y > 0)
             new_y = 0;

         /* Update panel position in real-time */
         lv_obj_set_y(g.settings_panel, new_y);

         /* If dragged down significantly, close the panel */
         if (new_y > SETTINGS_PANEL_HEIGHT / 3) {
             // printf("Drag down - closing settings\n");
             slide_settings(false);
             drag_started = false;
         }
     } else if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
         if (drag_started) {
             /* Snap back to original position if not closed */
             if (current_y > -SETTINGS_PANEL_HEIGHT / 3) {
                 // printf("Drag released - snapping back\n");
                 lv_obj_set_y(g.settings_panel, 0);
             }
             drag_started = false;
         }
     }
 }

 static void on_gesture(lv_event_t *e)
 {
     (void)e;
     lv_indev_t *indev = lv_indev_get_act();
     lv_indev_type_t indev_type = lv_indev_get_type(indev);

     if (indev_type == LV_INDEV_TYPE_POINTER) {
         lv_dir_t dir = lv_indev_get_gesture_dir(indev);

        /* Check if settings panel is visible */
        bool settings_visible = !lv_obj_has_flag(g.settings_panel, LV_OBJ_FLAG_HIDDEN);

         switch (dir) {
         case LV_DIR_TOP: // Swipe up - close settings
            if (settings_visible) {
                 // printf("Swipe up - closing settings with animation\n");
                 slide_settings(false);
             }
             break;
         case LV_DIR_BOTTOM: // Swipe down - open settings
            if (!settings_visible) {
                 // printf("Swipe down - opening settings\n");
                 slide_settings(true);
             }
             break;
         case LV_DIR_LEFT: // Swipe left - tracking screen
            /* Disable left/right swipes when settings panel is visible to avoid
             * interfering with volume slider */
            if (settings_visible) {
                // printf("Swipe left - ignored, settings panel is open\n");
                break;
            }
             if (lv_obj_has_flag(g.tracking_screen, LV_OBJ_FLAG_HIDDEN)) {
                 // printf("Swipe left - showing tracking\n");
                 slide_tracking(true);
             } else {
                 // printf("Swipe left - tracking already visible, ignoring\n");
             }
             break;
         case LV_DIR_RIGHT: // Swipe right - idle screen
            /* Disable left/right swipes when settings panel is visible to avoid
             * interfering with volume slider */
            if (settings_visible) {
                // printf("Swipe right - ignored, settings panel is open\n");
                break;
            }
             if (lv_obj_has_flag(g.idle_screen, LV_OBJ_FLAG_HIDDEN)) {
                 // printf("Swipe right - showing idle\n");
                 show_idle();
             } else {
                 // printf("Swipe right - idle already visible, ignoring\n");
             }
             break;
         default:
             break;
         }
     }
 }

 static void on_settings_backdrop_click(lv_event_t *e)
 {
     (void)e;
     slide_settings(false);
 }

 static void on_keyboard(lv_event_t *e)
 {
     uint32_t key = lv_event_get_key(e);
     // printf("Key pressed: %d (char: %c)\n", key, (char)key);

     switch (key) {
     case 'i':
     case 'I': // Idle screen
         if (lv_obj_has_flag(g.idle_screen, LV_OBJ_FLAG_HIDDEN)) {
             // printf("Key I - showing idle\n");
             show_idle();
         } else {
             // printf("Key I - idle already visible, ignoring\n");
         }
         break;
     case 't':
     case 'T': // Tracking screen
         if (lv_obj_has_flag(g.tracking_screen, LV_OBJ_FLAG_HIDDEN)) {
             // printf("Key T - showing tracking\n");
             slide_tracking(true);
         } else {
             // printf("Key T - tracking already visible, ignoring\n");
         }
         break;
     case 's':
     case 'S': // Settings
         // printf("Key S - toggling settings\n");
         if (lv_obj_has_flag(g.settings_panel, LV_OBJ_FLAG_HIDDEN)) {
             slide_settings(true);
         } else {
             slide_settings(false);
         }
         break;
     case 'x':
     case 'X': // Cancel SOS
         // printf("Key X - canceling SOS\n");
         if (g.sos_active) {
             hide_sos_alert();
         }
         break;
     case ' ': // Space for SOS
         // printf("Space - triggering SOS\n");
         if (!g.sos_active) {
             show_sos_alert();
         }
         break;
     case '0': // 50m scale
         // printf("Key 0 - animating to 50m scale\n");
         animate_distance_scale(50);
         break;
     case '1': // 100m scale
         // printf("Key 1 - animating to 100m scale\n");
         animate_distance_scale(100);
         break;
     case '2': // 200m scale
         // printf("Key 2 - animating to 200m scale\n");
         animate_distance_scale(200);
         break;
     case '3': // 500m scale
         // printf("Key 3 - animating to 500m scale\n");
         animate_distance_scale(500);
         break;
     case '4': // 1km scale
         // printf("Key 4 - animating to 1km scale\n");
         animate_distance_scale(1000);
         break;
     case '5': // 3km scale
         // printf("Key 5 - animating to 3km scale\n");
         animate_distance_scale(3000);
         break;
     case '6': // 5km scale
         // printf("Key 6 - animating to 5km scale\n");
         animate_distance_scale(5000);
         break;

    /* Eye animation controls */
    case 'n':
    case 'N': // Normal/Idle eyes
        // printf("Key N - resetting to idle eyes\n");
        set_idle_eye_state(0);  // 0 = idle state
        break;
    case 'h':
    case 'H': // Happy eyes
        // printf("Key H - triggering happy eyes\n");
        set_idle_eye_state(2);  // 2 = happy state
        break;
    case 'b':
    case 'B': // Manual blink
        // printf("Key B - triggering manual blink\n");
        set_idle_eye_state(1);  // 1 = blinking state
        break;
    case 'l':
    case 'L': // Look left
        // printf("Key L - looking left\n");
        g.target_pupil_x = -18;
        g.target_pupil_y = 0;
        break;
    case 'r':
    case 'R': // Look right
        // printf("Key R - looking right\n");
        g.target_pupil_x = 18;
        g.target_pupil_y = 0;
        break;
    case 'u':
    case 'U': // Look up
        // printf("Key U - looking up\n");
        g.target_pupil_x = 0;
        g.target_pupil_y = -18;
        break;
    case 'd':
    case 'D': // Look down
        // printf("Key D - looking down\n");
        g.target_pupil_x = 0;
        g.target_pupil_y = 18;
        break;
    case 'c':
    case 'C': // Look center
        // printf("Key C - looking center\n");
        g.target_pupil_x = 0;
        g.target_pupil_y = 0;
        break;
    case 'e':
    case 'E': // Random eye movement
        // printf("Key E - random eye movement\n");
        {
            int direction = rand() % 8;
            if (direction == 0) {
                g.target_pupil_x = -18;
                g.target_pupil_y = 0;
            } else if (direction == 1) {
                g.target_pupil_x = 18;
                g.target_pupil_y = 0;
            } else if (direction == 2) {
                g.target_pupil_x = 0;
                g.target_pupil_y = -18;
            } else if (direction == 3) {
                g.target_pupil_x = 0;
                g.target_pupil_y = 18;
            } else if (direction == 4) {
                g.target_pupil_x = -9;
                g.target_pupil_y = -9;
            } else if (direction == 5) {
                g.target_pupil_x = 9;
                g.target_pupil_y = -9;
            } else if (direction == 6) {
                g.target_pupil_x = -9;
                g.target_pupil_y = 9;
            } else {
                g.target_pupil_x = 9;
                g.target_pupil_y = 9;
            }
        }
        break;
    case 'o':
    case 'O': // Surprised expression
        // printf("Key O - surprised expression\n");
        set_idle_eye_state(3);  // 3 = surprised
        g.target_pupil_size = 110;  // Dilated pupils
        break;
    case 'z':
    case 'Z': // Sleepy expression
        // printf("Key Z - sleepy expression\n");
        set_idle_eye_state(4);  // 4 = sleepy
        g.target_pupil_size = 85;  // Smaller pupils
        break;
    case 'w':
    case 'W': // Wink
        // printf("Key W - wink\n");
        set_idle_eye_state(5);  // 5 = wink
        break;
    case 'a':
    case 'A': // Angry expression
        // printf("Key A - angry expression\n");
        set_idle_eye_state(6);  // 6 = angry
        break;
    case 'y':
    case 'Y': // Toggle red ring indicator
        // printf("Key Y - toggle red ring indicator\n");
        toggle_idle_red_ring();
        break;

    default:
         // printf("Unhandled key: %d\n", key);
         break;
     }
 }

 static void on_pressed(lv_event_t *e)
 {
     (void)e;
     if (g.sos_active)
         return;
     g.sos_pressed_start_ms = lv_tick_get();
     lv_arc_set_value(g.sos_hold_ring, 0);
     lv_obj_clear_flag(g.sos_hold_ring, LV_OBJ_FLAG_HIDDEN);
 }

 static void on_pressing(lv_event_t *e)
 {
     (void)e;
     if (g.sos_active)
         return;
     if (g.sos_pressed_start_ms == 0)
         return;
     uint32_t elapsed = lv_tick_elaps(g.sos_pressed_start_ms);
     int32_t pct = (int32_t)((elapsed * 100) / SOS_HOLD_MS);
     if (pct > 100)
         pct = 100;
     lv_arc_set_value(g.sos_hold_ring, pct);
     if (elapsed >= SOS_HOLD_MS) {
         g.sos_pressed_start_ms = 0;
         show_sos_alert();
     }
 }

 static void on_released(lv_event_t *e)
 {
     (void)e;
     if (!g.sos_active) {
         lv_obj_add_flag(g.sos_hold_ring, LV_OBJ_FLAG_HIDDEN);
     }
     g.sos_pressed_start_ms = 0;
 }

 static void __attribute__((unused)) on_sos_cancel(lv_event_t *e)
 {
     (void)e;
     hide_sos_alert();
 }

 /* Calibration function removed - no longer needed */

 static void compass_update(float yaw_deg)
 {
     /* Timer-based animation - 15 degrees per second */
     static float last_yaw = -999.0f;      /* Initialize to impossible value */
     if (fabsf(yaw_deg - last_yaw) < 1.0f) /* Threshold for timer-based updates */
         return;                           /* Skip if change is negligible */
     last_yaw = yaw_deg;

     int16_t angle_int = (int16_t)(yaw_deg * 10);

     /* Rotate compass face ring image - OPTIMIZED */
     /* Only update if angle change is significant (reduce unnecessary redraws) */
     static int16_t last_angle = -1;
     if (abs(angle_int - last_angle) >= 20) { /* Only update every 2 degrees for better performance */
         lv_obj_set_style_transform_angle(g.compass_face_ring_img, angle_int, 0);
         last_angle = angle_int;

         /* Force a refresh only when needed */
         lv_obj_invalidate(g.compass_face_ring_img);
     }
     /* Pivot point is already set to screen center in compass_build() */

     /* Update rotation text display */
     update_rotation_text(yaw_deg);

     /* Update target positions with new compass rotation */
     update_target_positions();

 #if ENABLE_CLOSE_TRACKING
     /* Update close-range navigation if active */
     if (g.close_range_mode) {
         update_close_range_arrow();
     }
 #endif

     /* Center overlay stays fixed - no rotation applied */
 }

 static void on_settings_close_anim_ready(lv_anim_t *anim)
 {
     (void)anim;
     lv_obj_add_flag(g.settings_panel, LV_OBJ_FLAG_HIDDEN);
     lv_obj_add_flag(g.settings_backdrop, LV_OBJ_FLAG_HIDDEN);

     /* Ensure focus returns to keyboard handler */
     if (g.focus_obj) {
         lv_group_focus_obj(g.focus_obj);
     }
 }

 static float wrap_deg(float d)
 {
     /* Normalize angle to 0-360 range */
     while (d < 0)
         d += 360.f;
     while (d >= 360.f)
         d -= 360.f;
     return d;
 }

 /* Improved angle wrapping that handles 180-degree glitches */
 static float wrap_angle_smooth(float current, float target)
 {
     /* Normalize both angles to 0-360 range */
     current = wrap_deg(current);
     target = wrap_deg(target);

     /* Calculate the shortest rotation path */
     float diff = target - current;

     /* Handle the 180-degree glitch case */
     if (diff > 180.0f) {
         /* Rotate counter-clockwise (shorter path) */
         return current - (360.0f - diff);
     } else if (diff < -180.0f) {
         /* Rotate clockwise (shorter path) */
         return current + (360.0f + diff);
     } else {
         /* Normal rotation */
         return target;
     }
 }

 /* Pinch gesture detection functions */
 /* Removed calculate_distance_between_points - not needed for placeholder implementation */

 // static void handle_pinch_gesture(lv_event_t *e)
 // {
 //     (void)e; /* Suppress unused parameter warning */

 //     /* Placeholder for pinch gesture - would need multi-touch support */
 //     // printf("Pinch gesture detected (placeholder)\n");
 // }

 // static void apply_zoom_to_compass(void)
 // {
 //     if (g.compass_container) {
 //         /* Apply zoom transform to compass container */
 //         lv_obj_set_style_transform_zoom(g.compass_container, (int16_t)(g.current_scale * 256), 0);

 //         /* Also apply to compass face ring image */
 //         if (g.compass_face_ring_img) {
 //             lv_obj_set_style_transform_zoom(g.compass_face_ring_img, (int16_t)(g.current_scale * 256), 0);
 //         }

 //         /* Update interval lines scale */
 //         update_interval_lines_scale();
 //     }
 // }

 // static void update_interval_lines_scale(void)
 // {
 //     /* Update interval lines to match zoom scale */
 //     for (int i = 0; i < g.interval_lines_count; i++) {
 //         if (g.interval_lines[i]) {
 //             lv_obj_set_style_transform_zoom(g.interval_lines[i], (int16_t)(g.current_scale * 256), 0);
 //         }
 //     }
 // }

 static void on_pinch_gesture(lv_event_t *e)
 {
     (void)e; /* Suppress unused parameter warning */

     /* Placeholder for pinch gesture detection */
     // printf("Pinch gesture event (placeholder)\n");
 }

 /* Smooth rotation animation functions */
 static void start_smooth_rotation(void)
 {
     /* Stop any existing animation */
     if (g.rotation_anim) {
         lv_anim_del(g.rotation_anim, NULL);
         g.rotation_anim = NULL;
     }

     /* Set rotation flag */
     g.is_rotating = true;

     /* Create smooth rotation animation */
     static lv_anim_t anim;
     g.rotation_anim = &anim;
     lv_anim_init(g.rotation_anim);
     lv_anim_set_var(g.rotation_anim, &g.current_yaw_deg);
     lv_anim_set_values(g.rotation_anim, g.current_yaw_deg, g.target_yaw_deg);
     lv_anim_set_exec_cb(g.rotation_anim, on_rotation_anim_value);
     lv_anim_set_time(g.rotation_anim, 1200); /* 1200ms smooth rotation for 45 degrees */
     lv_anim_set_ready_cb(g.rotation_anim, on_rotation_anim_ready);
     lv_anim_set_path_cb(g.rotation_anim, lv_anim_path_ease_out); /* Smooth easing */
     lv_anim_start(g.rotation_anim);
 }

 static void on_rotation_anim_value(void *var, int32_t value)
 {
     (void)var;
     /* Update current angle with animated value */
     g.current_yaw_deg = (float)value;

     /* Ensure angle is properly wrapped to avoid glitches */
     g.yaw_deg = wrap_deg(g.current_yaw_deg);

     /* Update compass with smooth rotation */
     compass_update(g.yaw_deg);
 }

 static void on_rotation_anim_ready(lv_anim_t *anim)
 {
     (void)anim;
     /* Animation complete - update final angle with proper wrapping */
     g.yaw_deg = wrap_deg(g.target_yaw_deg);
     g.is_rotating = false;
     g.rotation_anim = NULL;

     /* Final compass update */
     compass_update(g.yaw_deg);
 }

 static void __attribute__((unused)) on_tick(lv_timer_t *t)
 {
     (void)t;
     /* Timer-based animation - 45 degrees every 5 seconds with smooth rotation */
     if (!g.is_rotating) {
         /* Calculate target angle with smooth wrapping */
         float raw_target = g.yaw_deg + 45.0f;
         g.target_yaw_deg = wrap_angle_smooth(g.yaw_deg, raw_target);
         g.current_yaw_deg = g.yaw_deg;

         /* Start smooth rotation animation */
         start_smooth_rotation();
     }
 }

// /* Helper function to count UTF-8 characters */
// static int utf8_char_count(const char *text)
// {
//     int count = 0;
//     const unsigned char *str = (const unsigned char *)text;

//     while (*str) {
//         /* Skip continuation bytes (10xxxxxx) */
//         if ((*str & 0xC0) != 0x80) {
//             count++;
//         }
//         str++;
//     }
//     return count;
// }

// /* Helper function to get byte position of Nth UTF-8 character */
// static int utf8_char_to_byte_pos(const char *text, int char_limit)
// {
//     int char_count = 0;
//     int byte_pos = 0;
//     const unsigned char *str = (const unsigned char *)text;

//     while (str[byte_pos] && char_count < char_limit) {
//         /* Count characters (not continuation bytes) */
//         if ((str[byte_pos] & 0xC0) != 0x80) {
//             char_count++;
//         }
//         byte_pos++;
//     }
//     return byte_pos;
// }

/* Helper function to get the size of the next UTF-8 character */
static int utf8_next_char_size(const char *text, int pos)
{
    unsigned char c = text[pos];
    if ((c & 0x80) == 0) return 1;           // ASCII
    else if ((c & 0xE0) == 0xC0) return 2;   // 2-byte UTF-8
    else if ((c & 0xF0) == 0xE0) return 3;   // 3-byte UTF-8 (Chinese)
    else if ((c & 0xF8) == 0xF0) return 4;   // 4-byte UTF-8
    else return 1;                            // Invalid, skip one byte
}

/* Static version of update function (doesn't trigger animation) */
static void update_idle_bottom_text_static(const char *text)
{
    if (g.idle_bottom_text) {
        static char display_text[300]; // Buffer for formatted text with line breaks
        const int MAX_LINES = 3;         // Maximum number of lines
        const lv_font_t *font = &font_puhui_18_2;

        /* Different width for each line based on circular geometry */
        const int LINE_WIDTHS[3] = {280, 240, 180};

        if (text == NULL || text[0] == '\0') {
            lv_label_set_text(g.idle_bottom_text, "");
            return;
        }

        int line_count = 0;
        int out_pos = 0;
        int in_pos = 0;

        /* Process text line by line */
        while (text[in_pos] != '\0' && line_count < MAX_LINES) {
            int max_width = LINE_WIDTHS[line_count];
            int line_end_pos = in_pos;
            int best_break_pos = in_pos;

            /* Find the maximum amount of text that fits in current line */
            while (text[line_end_pos] != '\0') {
                /* Move to next character boundary (UTF-8 aware) */
                int next_pos = line_end_pos;
                if ((text[next_pos] & 0x80) == 0) {
                    next_pos += 1; // ASCII
                } else if ((text[next_pos] & 0xE0) == 0xC0) {
                    next_pos += 2; // 2-byte UTF-8
                } else if ((text[next_pos] & 0xF0) == 0xE0) {
                    next_pos += 3; // 3-byte UTF-8 (Chinese)
                } else if ((text[next_pos] & 0xF8) == 0xF0) {
                    next_pos += 4; // 4-byte UTF-8
                } else {
                    next_pos += 1;
                }

                /* Check if this much text fits */
                int len = next_pos - in_pos;
                lv_coord_t width = lv_txt_get_width(&text[in_pos], len, font, 0);

                if (width > max_width) {
                    /* Too wide - use previous position */
                    break;
                }

                /* This position fits - remember it */
                best_break_pos = next_pos;
                line_end_pos = next_pos;
            }

            /* Copy the line text */
            int line_len = best_break_pos - in_pos;
            if (line_len > 0 && out_pos + line_len < sizeof(display_text) - 10) {
                memcpy(&display_text[out_pos], &text[in_pos], line_len);
                out_pos += line_len;

                /* Move to next position */
                in_pos = best_break_pos;
                line_count++;

                /* Add newline if there's more text and we haven't reached max lines */
                if (text[in_pos] != '\0' && line_count < MAX_LINES) {
                    display_text[out_pos++] = '\n';
                }
            } else {
                /* No progress made - force at least one character */
                if (line_len == 0 && text[in_pos] != '\0') {
                    /* Force copy at least one character to avoid infinite loop */
                    int char_size = utf8_next_char_size(text, in_pos);

                    if (out_pos + char_size < sizeof(display_text) - 10) {
                        memcpy(&display_text[out_pos], &text[in_pos], char_size);
                        out_pos += char_size;
                        in_pos += char_size;

                        if (text[in_pos] != '\0' && line_count < MAX_LINES - 1) {
                            display_text[out_pos++] = '\n';
                        }
                        line_count++;
                    }
                }
                break;
            }
        }

        /* Check if there's still text remaining after 3 lines */
        if (text[in_pos] != '\0') {
            /* Text exceeds 3 lines - need to add ellipsis to the last line */
            /* Remove the last line break if it exists */
            if (out_pos > 0 && display_text[out_pos - 1] == '\n') {
                out_pos--;
            }

            /* Find the last line start */
            int last_line_start = out_pos;
            while (last_line_start > 0 && display_text[last_line_start - 1] != '\n') {
                last_line_start--;
            }

            /* Calculate how much space we have for ellipsis */
            int last_line_len = out_pos - last_line_start;
            int max_width = LINE_WIDTHS[line_count - 1]; // Width of last line we wrote

            /* Find how much of the last line fits with "..." */
            lv_coord_t ellipsis_width = lv_txt_get_width("...", 3, font, 0);

            while (last_line_len > 0) {
                lv_coord_t line_width = lv_txt_get_width(&display_text[last_line_start], last_line_len, font, 0);

                if (line_width + ellipsis_width <= max_width) {
                    break;
                }

                /* Go back one character */
                last_line_len--;
                while (last_line_len > 0 && (display_text[last_line_start + last_line_len] & 0xC0) == 0x80) {
                    last_line_len--; // Skip UTF-8 continuation bytes
                }
            }

            /* Truncate and add ellipsis */
            out_pos = last_line_start + last_line_len;
            if (out_pos + 3 < sizeof(display_text) - 1) {
                memcpy(&display_text[out_pos], "...", 3);
                out_pos += 3;
            }
        }

        display_text[out_pos] = '\0';
        lv_label_set_text(g.idle_bottom_text, display_text);

        /* Adjust vertical position based on number of lines */
        int y_offset;
        if (line_count == 1) {
            y_offset = -50;  /* Single line - position higher */
        } else if (line_count == 2) {
            y_offset = -35;  /* Two lines - middle position */
        } else {
            y_offset = -20;  /* Three lines - near bottom */
        }
        lv_obj_align(g.idle_bottom_text, LV_ALIGN_BOTTOM_MID, 0, y_offset);
    }
}

/* Typewriter animation timer callback */
// TODO: DEBUG typewriter_full_text need locking?
static void typewriter_timer_cb(lv_timer_t *timer)
{
    if (!g.typewriter_active || !g.typewriter_full_text) {
        return;
    }

    int text_len = strlen(g.typewriter_full_text);
    const int MAX_DISPLAY_CHARS = 38; // ~38 chars fit in 3 lines

    /* Count how many characters we've displayed so far */
    int displayed_chars = 0;
    int pos = g.typewriter_window_start;
    while (pos < g.typewriter_char_index && pos < text_len) {
        if ((g.typewriter_full_text[pos] & 0xC0) != 0x80) {
            displayed_chars++;
        }
        pos++;
    }

    /* Check if we're in scrolling mode (display window is full) */
    bool is_scrolling = (displayed_chars >= MAX_DISPLAY_CHARS);

    /* Check if the last displayed character was Chinese punctuation for extra pause */
    bool just_displayed_punctuation = false;
    if (g.typewriter_char_index >= 3) {
        /* Check for "，" (E3 80 81) or "。" (E3 80 82) */
        const unsigned char *text_bytes = (const unsigned char *)g.typewriter_full_text;
        int prev_pos = g.typewriter_char_index - 3;

        if (prev_pos >= 0 && prev_pos < text_len - 2) {
            /* Chinese comma "，" is 0xE3 0x80 0x81 */
            /* Chinese period "。" is 0xE3 0x80 0x82 */
            if (text_bytes[prev_pos] == 0xE3 && text_bytes[prev_pos + 1] == 0x80) {
                if (text_bytes[prev_pos + 2] == 0x81 || text_bytes[prev_pos + 2] == 0x82) {
                    just_displayed_punctuation = true;
                }
            }
        }
    }

    /* Adjust timer period based on mode and punctuation */
    if (just_displayed_punctuation && !is_scrolling) {
        lv_timer_set_period(g.typewriter_timer, 600); // Extra pause at punctuation (300ms)
    } else if (is_scrolling) {
        lv_timer_set_period(g.typewriter_timer, 200); // Slower scroll (150ms per char)
    } else {
        lv_timer_set_period(g.typewriter_timer, 100);  // Fast typewriter (50ms per char)
    }

    /* Extract window of text to display */
    static char window_text[500];
    int window_size = 0;
    int byte_pos = g.typewriter_window_start;
    int char_count = 0;

    /* Build the window text character by character up to current index */
    while (byte_pos < text_len && byte_pos <= g.typewriter_char_index && char_count < MAX_DISPLAY_CHARS) {
        int char_size = utf8_next_char_size(g.typewriter_full_text, byte_pos);

        if (window_size + char_size < sizeof(window_text) - 1) {
            memcpy(&window_text[window_size], &g.typewriter_full_text[byte_pos], char_size);
            window_size += char_size;
            char_count++;
        }

        byte_pos += char_size;
    }
    window_text[window_size] = '\0';

    /* Update display with current window */
    update_idle_bottom_text_static(window_text);

    /* Advance to next character */
    if (g.typewriter_char_index < text_len) {
        int char_size = utf8_next_char_size(g.typewriter_full_text, g.typewriter_char_index);
        g.typewriter_char_index += char_size;

        /* Recalculate displayed characters after advance */
        displayed_chars = 0;
        pos = g.typewriter_window_start;
        while (pos < g.typewriter_char_index && pos < text_len) {
            if ((g.typewriter_full_text[pos] & 0xC0) != 0x80) {
                displayed_chars++;
            }
            pos++;
        }

        /* Slide window forward if we exceed display capacity */
        if (displayed_chars > MAX_DISPLAY_CHARS) {
            /* Move window start forward by one character */
            int skip_size = utf8_next_char_size(g.typewriter_full_text, g.typewriter_window_start);
            g.typewriter_window_start += skip_size;
        }
    } else {
        /* Animation complete - pause briefly, then restart */
        static int pause_counter = 0;
        pause_counter++;

        if (pause_counter >= 20) { // Pause for ~1 second (20 * 50ms)
            /* Clear display */
            if (g.idle_bottom_text) {
                lv_label_set_text(g.idle_bottom_text, "");
            }

            /* Reset animation to start from beginning */
            g.typewriter_char_index = 0;
            g.typewriter_window_start = 0;
            pause_counter = 0;

            /* Reset timer to fast speed for typewriter effect */
            lv_timer_set_period(g.typewriter_timer, 50);
        }
    }
}

/* Function to update idle screen bottom text with typewriter animation */
 void update_idle_bottom_text(const char *text)
 {
    /* Stop any existing animation */
    if (g.typewriter_timer) {
        lv_timer_del(g.typewriter_timer);
        g.typewriter_timer = NULL;
    }

    if (g.typewriter_full_text) {
        tal_psram_free(g.typewriter_full_text);
        g.typewriter_full_text = NULL;
    }

    if (text == NULL || text[0] == '\0') {
        g.typewriter_active = false;
     if (g.idle_bottom_text) {
            lv_label_set_text(g.idle_bottom_text, "");
        }
        return;
    }

    /* Store the full text */
    g.typewriter_full_text = tal_psram_malloc(strlen(text) + 1);
    if (g.typewriter_full_text == NULL) {
        g.typewriter_active = false;
        return; // Memory allocation failed
    }
    memset(g.typewriter_full_text, 0, strlen(text) + 1);
    strncpy(g.typewriter_full_text, text, strlen(text) + 1);
    g.typewriter_char_index = 0;
    g.typewriter_window_start = 0;
    g.typewriter_active = true;

    /* Create timer for animation - 50ms per character (~20 chars/second) */
    g.typewriter_timer = lv_timer_create(typewriter_timer_cb, 50, NULL);
}

/* Public eye animation API */
void set_idle_eye_state(int state)
{
    set_eye_state(state);
 }

 /* Public GPS API Functions */
 void gps_add_target(float lat, float lon, uint32_t color)
 {
    tuya_lvgl_mutex_lock();
     add_target_coord(lat, lon, color);
     tuya_lvgl_mutex_unlock();
 }


/**
 * @brief Update compass heading from external sensor data
 * @param heading_degrees Compass heading in degrees (0-360)
 * 
 * This function updates the compass rotation based on real sensor data
 * (e.g., BMM150 magnetometer). It uses smooth rotation animation for
 * natural transitions.
 */
void tracker_update_compass_heading(float heading_degrees)
{
    /* Normalize heading to 0-360 range */
    heading_degrees = wrap_deg(heading_degrees);
    
    /* Calculate current angle (use g.yaw_deg if not rotating) */
    float current_angle = g.is_rotating ? g.current_yaw_deg : g.yaw_deg;
    
    /* Calculate angle difference to determine if update is needed */
    float diff = fabsf(heading_degrees - current_angle);
    if (diff > 180.0f) {
        diff = 360.0f - diff; /* Handle wrap-around */
    }
    
    /* Only update if change is significant (> 2 degrees) to avoid jitter
     * This threshold filters out sensor noise and prevents excessive animations
     * at 10Hz BMM150 update rate */
    if (diff < 2.0f) {
        return;
    }
    
    /* Calculate smooth target angle to avoid 360-0 glitch */
    g.target_yaw_deg = wrap_angle_smooth(current_angle, heading_degrees);
    g.current_yaw_deg = current_angle;
    
    /* Debug: Log significant compass updates */
    static uint32_t last_log_time = 0;
    uint32_t now = tal_system_get_millisecond();
    if (now - last_log_time > 1000) { /* Log once per second max */
        PR_DEBUG("[COMPASS] Heading update: %.1f° → %.1f° (diff: %.1f°)", 
                 current_angle, heading_degrees, diff);
        last_log_time = now;
    }
    
    /* Start smooth rotation animation */
    start_smooth_rotation();
}

 void gps_remove_target(int index)
 {
     tuya_lvgl_mutex_lock();
     remove_target_coord(index);
     tuya_lvgl_mutex_unlock();
 }

 void gps_clear_all_targets(void)
 {
     tuya_lvgl_mutex_lock();
     clear_all_targets();
     tuya_lvgl_mutex_unlock();
 }

 void gps_set_tracker_position(float lat, float lon)
 {
    tuya_lvgl_mutex_lock();
     g.self_lat = lat;
     g.self_lon = lon;

     /* Dynamically update all target positions and distances */
     update_target_positions_for_new_origin();
     tuya_lvgl_mutex_unlock();
 }

 int gps_get_target_count(void)
 {
     return g.target_count;
 }

 int gps_get_target_distance(int index)
 {
     if (index < 0 || index >= g.target_count)
         return -1;
     return g.targets[index].distance_meters;
 }

 void gps_update_target_markers(void)
 {
     render_target_markers();
 }

 /* Dummy data access functions */
 float gps_get_dummy_self_lat(void)
 {
     return DUMMY_SELF_LAT;
 }

 float gps_get_dummy_self_lon(void)
 {
     return DUMMY_SELF_LON;
 }

 int gps_get_dummy_target_count(void)
 {
     return DUMMY_TARGET_COUNT;
 }

 const void *gps_get_dummy_target(int index)
 {
     if (index < 0 || index >= (int)DUMMY_TARGET_COUNT)
         return NULL;
     return &DUMMY_TARGETS[index];
 }

 #if ENABLE_CLOSE_TRACKING
 /**********************
  * CLOSE-RANGE NAVIGATION MODE
  **********************/

 static void create_close_range_ui(void)
 {
     /* Create close-range navigation container */
     g.close_nav_container = lv_obj_create(g.tracking_screen);
     lv_obj_set_size(g.close_nav_container, CATTLE_SCREEN_WIDTH, CATTLE_SCREEN_HEIGHT);
     lv_obj_set_style_bg_opa(g.close_nav_container, LV_OPA_TRANSP, 0);
     lv_obj_set_style_border_width(g.close_nav_container, 0, 0); /* Remove border */
     lv_obj_clear_flag(g.close_nav_container, LV_OBJ_FLAG_CLICKABLE);
     lv_obj_clear_flag(g.close_nav_container, LV_OBJ_FLAG_SCROLLABLE);

     /* Set transform pivot to center for proper scaling */
     lv_obj_set_style_transform_pivot_x(g.close_nav_container, CATTLE_SCREEN_WIDTH / 2, 0);
     lv_obj_set_style_transform_pivot_y(g.close_nav_container, CATTLE_SCREEN_HEIGHT / 2, 0);

     lv_obj_center(g.close_nav_container);
     lv_obj_add_flag(g.close_nav_container, LV_OBJ_FLAG_HIDDEN); /* Initially hidden */

     /* Create black circle background */
     lv_obj_t *black_circle = lv_obj_create(g.close_nav_container);
     lv_obj_set_size(black_circle, 466, 466);
     lv_obj_set_style_bg_color(black_circle, lv_color_black(), 0);
     lv_obj_set_style_radius(black_circle, 233, 0);                    /* 466/2 = 233 for perfect circle */
     lv_obj_set_style_border_width(black_circle, 0, 0);                /* Remove border */
     lv_obj_set_style_border_color(black_circle, lv_color_black(), 0); /* Set border color to black */
     lv_obj_center(black_circle);

     /* Create the navigation ring */
     g.close_nav_ring_img = lv_img_create(g.close_nav_container);
     lv_img_set_src(g.close_nav_ring_img, &closing_nav_ring);
     lv_obj_center(g.close_nav_ring_img);

     /* Create the navigation arrow */
     g.close_nav_arrow_img = lv_img_create(g.close_nav_container);
     lv_img_set_src(g.close_nav_arrow_img, &closing_nav_arrow);
     lv_obj_center(g.close_nav_arrow_img);

     /* Set transform pivot to screen center for proper rotation */
     lv_obj_set_style_transform_pivot_x(g.close_nav_arrow_img, CATTLE_SCREEN_WIDTH / 2, 0);
     lv_obj_set_style_transform_pivot_y(g.close_nav_arrow_img, CATTLE_SCREEN_HEIGHT / 2, 0);

     /* Create distance text */
     g.close_nav_distance_text = lv_label_create(g.close_nav_container);
     lv_obj_set_style_text_color(g.close_nav_distance_text, lv_color_white(), 0);
     lv_obj_set_style_text_font(g.close_nav_distance_text, &lv_font_montserrat_24, 0);
     lv_obj_align(g.close_nav_distance_text, LV_ALIGN_TOP_MID, 0, 20);

     /* Create compass text */
     g.close_nav_compass_text = lv_label_create(g.close_nav_container);
     lv_obj_set_style_text_color(g.close_nav_compass_text, lv_color_white(), 0);
     lv_obj_set_style_text_font(g.close_nav_compass_text, &lv_font_montserrat_16, 0);
     lv_obj_align(g.close_nav_compass_text, LV_ALIGN_BOTTOM_MID, 0, -20);

     /* Create found state elements (initially hidden) */
     g.found_circle = lv_obj_create(g.close_nav_container);
     lv_obj_set_size(g.found_circle, 200, 200);
     lv_obj_set_style_bg_color(g.found_circle, lv_color_hex(0x00FF00), 0);
     lv_obj_set_style_radius(g.found_circle, 100, 0);
     lv_obj_center(g.found_circle);
     lv_obj_add_flag(g.found_circle, LV_OBJ_FLAG_HIDDEN);

     g.found_text = lv_label_create(g.found_circle);
     lv_label_set_text(g.found_text, "HERE");
     lv_obj_set_style_text_color(g.found_text, lv_color_white(), 0);
     lv_obj_set_style_text_font(g.found_text, &lv_font_montserrat_20, 0);
     lv_obj_align(g.found_text, LV_ALIGN_TOP_MID, 0, 20);

     g.found_cow_icon = lv_img_create(g.found_circle);
     lv_img_set_src(g.found_cow_icon, &closing_nav_cow_icon);
     lv_obj_center(g.found_cow_icon);
 }

 static void show_close_range_mode(void)
 {
     if (!g.close_nav_container) {
         create_close_range_ui();
     }

     /* Clear all dummy targets when entering close-range mode */
     clear_all_targets();

     /* Set transform pivot to center of screen for proper scaling */
     lv_obj_set_style_transform_pivot_x(g.close_nav_container, CATTLE_SCREEN_WIDTH / 2, 0);
     lv_obj_set_style_transform_pivot_y(g.close_nav_container, CATTLE_SCREEN_HEIGHT / 2, 0);

     /* Start with small scale for zoom-in animation */
     lv_obj_set_style_transform_zoom(g.close_nav_container, 128, 0); /* 128 = 0.5x scale (small) */
     lv_obj_center(g.close_nav_container);

     /* Show close-range navigation */
     lv_obj_clear_flag(g.close_nav_container, LV_OBJ_FLAG_HIDDEN);

     /* Hide compass elements */
     lv_obj_add_flag(g.compass_container, LV_OBJ_FLAG_HIDDEN);
     lv_obj_add_flag(g.distance_img, LV_OBJ_FLAG_HIDDEN);
     lv_obj_add_flag(g.distance_text, LV_OBJ_FLAG_HIDDEN);
     lv_obj_add_flag(g.rotation_bg, LV_OBJ_FLAG_HIDDEN);

     /* Start zoom-in animation */
     if (g.close_nav_zoom_anim) {
         lv_anim_del(g.close_nav_zoom_anim, NULL);
     }
     g.close_nav_zooming_out = false; /* This is a zoom-in animation */
     static lv_anim_t zoom_anim;
     g.close_nav_zoom_anim = &zoom_anim;
     lv_anim_init(g.close_nav_zoom_anim);
     lv_anim_set_var(g.close_nav_zoom_anim, g.close_nav_container);
     lv_anim_set_values(g.close_nav_zoom_anim, 128, 256); /* From 0.5x to 1.0x scale */
     lv_anim_set_time(g.close_nav_zoom_anim, 250);        /* 250ms animation - faster */
     lv_anim_set_exec_cb(g.close_nav_zoom_anim, on_close_nav_zoom_anim);
     lv_anim_set_ready_cb(g.close_nav_zoom_anim, on_close_nav_zoom_ready);
     lv_anim_set_path_cb(g.close_nav_zoom_anim, lv_anim_path_ease_out);
     lv_anim_start(g.close_nav_zoom_anim);

     /* Update close-range UI */
     update_close_range_arrow();
 }

 static void hide_close_range_mode(void)
 {
     if (g.close_nav_container) {
         /* Ensure transform pivot is set to center for proper scaling */
         lv_obj_set_style_transform_pivot_x(g.close_nav_container, CATTLE_SCREEN_WIDTH / 2, 0);
         lv_obj_set_style_transform_pivot_y(g.close_nav_container, CATTLE_SCREEN_HEIGHT / 2, 0);

         /* Start zoom-out animation */
         if (g.close_nav_zoom_anim) {
             lv_anim_del(g.close_nav_zoom_anim, NULL);
         }
         g.close_nav_zooming_out = true; /* This is a zoom-out animation */
         static lv_anim_t zoom_out_anim;
         g.close_nav_zoom_anim = &zoom_out_anim;
         lv_anim_init(g.close_nav_zoom_anim);
         lv_anim_set_var(g.close_nav_zoom_anim, g.close_nav_container);
         lv_anim_set_values(g.close_nav_zoom_anim, 256, 128); /* From 1.0x to 0.5x scale */
         lv_anim_set_time(g.close_nav_zoom_anim, 250);        /* 100ms animation - half the time */
         lv_anim_set_exec_cb(g.close_nav_zoom_anim, on_close_nav_zoom_anim);
         lv_anim_set_ready_cb(g.close_nav_zoom_anim, on_close_nav_zoom_ready);
         lv_anim_set_path_cb(g.close_nav_zoom_anim, lv_anim_path_ease_in);
         lv_anim_start(g.close_nav_zoom_anim);

         /* Don't hide immediately - let the animation complete first */
         /* The hiding will be handled in the animation ready callback */
     }

     /* Restore dummy targets when exiting close-range mode */
     for (int i = 0; i < (int)DUMMY_TARGET_COUNT; i++) {
         add_target_coord(DUMMY_TARGETS[i].lat, DUMMY_TARGETS[i].lon, DUMMY_TARGETS[i].color);
     }
     update_map_scale();
     render_target_markers();

     /* Show compass elements */
     lv_obj_clear_flag(g.compass_container, LV_OBJ_FLAG_HIDDEN);
     lv_obj_clear_flag(g.distance_img, LV_OBJ_FLAG_HIDDEN);
     lv_obj_clear_flag(g.distance_text, LV_OBJ_FLAG_HIDDEN);
     lv_obj_clear_flag(g.rotation_bg, LV_OBJ_FLAG_HIDDEN);
 }

 static void update_close_range_arrow(void)
 {
     if (!g.close_nav_arrow_img || !g.close_range_mode)
         return;

     /* Use main compass angle for rotation */
     float compass_angle = g.yaw_deg;

     /* Use a fixed distance for close-range mode */
     float distance = 50.0f; /* Fixed 50m distance for close-range display */

     /* Update distance text */
     char distance_str[32];
     snprintf(distance_str, sizeof(distance_str), "Distance: %.0fM", distance);
     lv_label_set_text(g.close_nav_distance_text, distance_str);

     /* Update compass text */
     char compass_str[32];
     int degrees = (int)roundf(compass_angle);
     const char *direction;
     if (degrees >= 337.5f || degrees < 22.5f) {
         direction = "N";
     } else if (degrees >= 22.5f && degrees < 67.5f) {
         direction = "NE";
     } else if (degrees >= 67.5f && degrees < 112.5f) {
         direction = "E";
     } else if (degrees >= 112.5f && degrees < 157.5f) {
         direction = "SE";
     } else if (degrees >= 157.5f && degrees < 202.5f) {
         direction = "S";
     } else if (degrees >= 202.5f && degrees < 247.5f) {
         direction = "SW";
     } else if (degrees >= 247.5f && degrees < 292.5f) {
         direction = "W";
     } else {
         direction = "NW";
     }
     snprintf(compass_str, sizeof(compass_str), "%s-%d°", direction, degrees);
     lv_label_set_text(g.close_nav_compass_text, compass_str);

     /* Set pivot to center of the arrow image for proper rotation */
     lv_coord_t arrow_w = lv_obj_get_width(g.close_nav_arrow_img);
     lv_coord_t arrow_h = lv_obj_get_height(g.close_nav_arrow_img);
     lv_obj_set_style_transform_pivot_x(g.close_nav_arrow_img, arrow_w / 2, 0);
     lv_obj_set_style_transform_pivot_y(g.close_nav_arrow_img, arrow_h / 2, 0);

     /* Align the arrow image center to the screen center */
     lv_obj_align(g.close_nav_arrow_img, LV_ALIGN_CENTER, 0, 0);

     /* Rotate only the navigation arrow based on compass angle */
     lv_obj_set_style_transform_angle(g.close_nav_arrow_img, compass_angle * 10, 0);

     /* Dots removed for clean close-range navigation */
 }

 static void on_close_nav_zoom_anim(void *var, int32_t value)
 {
     lv_obj_t *container = (lv_obj_t *)var;
     lv_obj_set_style_transform_zoom(container, value, 0);
 }

 static void on_close_nav_zoom_ready(lv_anim_t *anim)
 {
     (void)anim;
     /* Animation complete - check if this was a zoom-out animation */
     if (g.close_nav_zooming_out) {
         /* This is a zoom-out animation - hide the container */
         if (g.close_nav_container) {
             lv_obj_add_flag(g.close_nav_container, LV_OBJ_FLAG_HIDDEN);
         }
     } else {
         /* This is a zoom-in animation - ensure final scale is set */
         lv_obj_set_style_transform_zoom(g.close_nav_container, 256, 0); /* 1.0x scale */
     }
 }
 #endif
