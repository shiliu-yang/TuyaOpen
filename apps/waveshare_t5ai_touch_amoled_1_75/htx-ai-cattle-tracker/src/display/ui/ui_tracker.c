/**
 * @file ui_tracker.c
 * @brief Circular 466x466 tracker UI with multiple screens and dummy interfaces
 *
 * Screens:
 * - Idle: centered text inside circular area
 * - Tracking: simple compass with calibration flow and dummy GPS/IMU
 * - Mic: static "Listening" screen with exit
 * - Settings overlay (slide from top): GPS num, time/date, volume slider
 * - SOS: long-press 3s animation to enter SOS, cancel with X
 */

#include "tuya_cloud_types.h"

#if defined(ENABLE_GUI_TRACKER) && (ENABLE_GUI_TRACKER == 1)

#include "ui_display.h"

#include "lvgl.h"

/***********************************************************
************************macro define************************
***********************************************************/

#define CIRCULAR_SIZE            466
#define SOS_HOLD_MS              3000
#define COMPASS_TIMER_MS         100

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef enum {
    SCREEN_IDLE = 0,
    SCREEN_TRACKING,
    SCREEN_MIC,
    SCREEN_SOS,
} TRACKER_SCREEN_E;

typedef struct {
    /* root */
    lv_obj_t *root;
    lv_obj_t *circular;

    /* nav */
    lv_obj_t *footer;
    lv_obj_t *btn_idle;
    lv_obj_t *btn_track;
    lv_obj_t *btn_mic;
    lv_obj_t *btn_sos;

    /* overlay */
    lv_obj_t *settings_overlay;
    lv_slider_t *volume_slider;
    lv_obj_t *gps_label;
    lv_obj_t *time_label;

    /* idle */
    lv_obj_t *idle_cont;
    lv_obj_t *idle_label;

    /* tracking */
    lv_obj_t *track_cont;
    lv_obj_t *compass_cont;
    lv_obj_t *compass_needle;
    lv_obj_t *compass_center;
    lv_obj_t *calib_label;
    lv_obj_t *calib_btn;
    lv_timer_t *compass_timer;

    /* mic */
    lv_obj_t *mic_cont;
    lv_obj_t *mic_label;
    lv_obj_t *mic_back_btn;

    /* sos */
    lv_obj_t *sos_cont;
    lv_obj_t *sos_label;
    lv_obj_t *sos_cancel_btn;
    lv_timer_t *sos_timer;
    uint32_t sos_press_start_ms;

    /* state */
    TRACKER_SCREEN_E current;
    bool settings_visible;
    bool is_calibrated;
    int16_t dummy_heading_deg;
} TRACKER_UI_T;

typedef struct {
    lv_font_t *text;
    lv_font_t *icon;
    const lv_font_t *emoji; /* unused */
    UI_EMOJI_LIST_T *emoji_list; /* unused */
} TRACKER_FONT_T;

typedef struct {
    TRACKER_UI_T ui;
    TRACKER_FONT_T font;
} TRACKER_APP_T;

static TRACKER_APP_T sg_tracker = {0};

/***********************************************************
********************dummy interfaces************************
***********************************************************/
static int __dummy_get_gps_sat_count(void) { return 7; }
static void __dummy_get_time_str(char *buf, size_t len) {
    lv_snprintf(buf, len, "2025-09-25 12:34");
}
static void __dummy_set_volume(int value) { (void)value; }
static bool __dummy_is_asr_listening(void) { return true; }
static void __dummy_get_target_ll(double *lat, double *lon) { *lat = 0.0; *lon = 0.0; }
static void __dummy_get_self_ll(double *lat, double *lon) { *lat = 0.0; *lon = 0.0; }

/***********************************************************
***********************helpers******************************
***********************************************************/
static void __route_to(TRACKER_SCREEN_E screen);
static void __ensure_only(lv_obj_t *cont);
static void __update_settings_overlay(void);

static void __ensure_only(lv_obj_t *cont)
{
    lv_obj_add_flag(sg_tracker.ui.idle_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sg_tracker.ui.track_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sg_tracker.ui.mic_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sg_tracker.ui.sos_cont, LV_OBJ_FLAG_HIDDEN);
    if (cont) {
        lv_obj_clear_flag(cont, LV_OBJ_FLAG_HIDDEN);
    }
}

static void __footer_set_active(TRACKER_SCREEN_E screen)
{
    lv_obj_clear_state(sg_tracker.ui.btn_idle, LV_STATE_CHECKED);
    lv_obj_clear_state(sg_tracker.ui.btn_track, LV_STATE_CHECKED);
    lv_obj_clear_state(sg_tracker.ui.btn_mic, LV_STATE_CHECKED);
    lv_obj_clear_state(sg_tracker.ui.btn_sos, LV_STATE_CHECKED);
    switch (screen) {
    case SCREEN_IDLE: lv_obj_add_state(sg_tracker.ui.btn_idle, LV_STATE_CHECKED); break;
    case SCREEN_TRACKING: lv_obj_add_state(sg_tracker.ui.btn_track, LV_STATE_CHECKED); break;
    case SCREEN_MIC: lv_obj_add_state(sg_tracker.ui.btn_mic, LV_STATE_CHECKED); break;
    case SCREEN_SOS: lv_obj_add_state(sg_tracker.ui.btn_sos, LV_STATE_CHECKED); break;
    default: break;
    }
}

static void __on_footer_btn(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    if (btn == sg_tracker.ui.btn_idle) {
        __route_to(SCREEN_IDLE);
    } else if (btn == sg_tracker.ui.btn_track) {
        __route_to(SCREEN_TRACKING);
    } else if (btn == sg_tracker.ui.btn_mic) {
        __route_to(SCREEN_MIC);
    } else if (btn == sg_tracker.ui.btn_sos) {
        __route_to(SCREEN_SOS);
    }
}

static void __compass_timer_cb(lv_timer_t *t)
{
    (void)t;
    /* simple dummy rotation */
    sg_tracker.ui.dummy_heading_deg += 3;
    if (sg_tracker.ui.dummy_heading_deg >= 360) sg_tracker.ui.dummy_heading_deg -= 360;
    lv_img_set_angle(sg_tracker.ui.compass_needle, sg_tracker.ui.dummy_heading_deg * 10); /* 0.1 deg units */
}

static void __sos_timer_cb(lv_timer_t *t)
{
    uint32_t now = lv_tick_get();
    if (now - sg_tracker.ui.sos_press_start_ms >= SOS_HOLD_MS) {
        /* Enter SOS state */
        lv_label_set_text(sg_tracker.ui.sos_label, "SOS Active");
        lv_timer_del(sg_tracker.ui.sos_timer);
        sg_tracker.ui.sos_timer = NULL;
    } else {
        /* show simple progress using label dots */
        uint32_t ms = now - sg_tracker.ui.sos_press_start_ms;
        uint8_t step = (ms * 3) / SOS_HOLD_MS + 1; /* 1..3 */
        const char *dots = (step == 1) ? "." : (step == 2) ? ".." : "...";
        lv_label_set_text_fmt(sg_tracker.ui.sos_label, "Hold to SOS %s", dots);
    }
}

static void __settings_close(lv_event_t *e)
{
    (void)e;
    if (sg_tracker.ui.settings_overlay) {
        lv_obj_add_flag(sg_tracker.ui.settings_overlay, LV_OBJ_FLAG_HIDDEN);
        sg_tracker.ui.settings_visible = false;
    }
}

static void __settings_open(void)
{
    if (!sg_tracker.ui.settings_overlay) return;
    __update_settings_overlay();
    lv_obj_clear_flag(sg_tracker.ui.settings_overlay, LV_OBJ_FLAG_HIDDEN);
    sg_tracker.ui.settings_visible = true;
}

static void __on_settings_volume(lv_event_t *e)
{
    int v = lv_slider_get_value((lv_obj_t *)lv_event_get_target(e));
    __dummy_set_volume(v);
}

static void __update_settings_overlay(void)
{
    char buf[48];
    lv_label_set_text_fmt(sg_tracker.ui.gps_label, "GPS: %d", __dummy_get_gps_sat_count());
    __dummy_get_time_str(buf, sizeof(buf));
    lv_label_set_text_fmt(sg_tracker.ui.time_label, "%s", buf);
}

static void __on_gesture(lv_event_t *e)
{
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_TOP) {
        __settings_open();
    }
}

static void __on_calib_btn(lv_event_t *e)
{
    (void)e;
    sg_tracker.ui.is_calibrated = true;
    lv_obj_add_flag(sg_tracker.ui.calib_btn, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(sg_tracker.ui.calib_label, "Calibrated. Compass active.");
}

static void __on_mic_back(lv_event_t *e)
{
    (void)e;
    __route_to(SCREEN_IDLE);
}

static void __on_sos_press(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        sg_tracker.ui.sos_press_start_ms = lv_tick_get();
        if (sg_tracker.ui.sos_timer) lv_timer_del(sg_tracker.ui.sos_timer);
        sg_tracker.ui.sos_timer = lv_timer_create(__sos_timer_cb, 200, NULL);
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        if (sg_tracker.ui.sos_timer) {
            lv_timer_del(sg_tracker.ui.sos_timer);
            sg_tracker.ui.sos_timer = NULL;
        }
        lv_label_set_text(sg_tracker.ui.sos_label, "Hold 3s to SOS");
    }
}

static void __on_sos_cancel(lv_event_t *e)
{
    (void)e;
    __route_to(SCREEN_IDLE);
}

/***********************************************************
***********************screen builders**********************
***********************************************************/
static void __build_footer(lv_obj_t *parent)
{
    sg_tracker.ui.footer = lv_obj_create(parent);
    lv_obj_set_width(sg_tracker.ui.footer, LV_PCT(100));
    lv_obj_set_height(sg_tracker.ui.footer, 40);
    lv_obj_set_style_pad_all(sg_tracker.ui.footer, 4, 0);
    lv_obj_set_style_border_width(sg_tracker.ui.footer, 0, 0);
    lv_obj_set_flex_flow(sg_tracker.ui.footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_bg_opa(sg_tracker.ui.footer, LV_OPA_20, 0);

    sg_tracker.ui.btn_idle = lv_btn_create(sg_tracker.ui.footer);
    lv_obj_add_event_cb(sg_tracker.ui.btn_idle, __on_footer_btn, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l1 = lv_label_create(sg_tracker.ui.btn_idle);
    lv_label_set_text(l1, "Idle");

    sg_tracker.ui.btn_track = lv_btn_create(sg_tracker.ui.footer);
    lv_obj_add_event_cb(sg_tracker.ui.btn_track, __on_footer_btn, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l2 = lv_label_create(sg_tracker.ui.btn_track);
    lv_label_set_text(l2, "Track");

    sg_tracker.ui.btn_mic = lv_btn_create(sg_tracker.ui.footer);
    lv_obj_add_event_cb(sg_tracker.ui.btn_mic, __on_footer_btn, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l3 = lv_label_create(sg_tracker.ui.btn_mic);
    lv_label_set_text(l3, "Mic");

    sg_tracker.ui.btn_sos = lv_btn_create(sg_tracker.ui.footer);
    lv_obj_add_event_cb(sg_tracker.ui.btn_sos, __on_footer_btn, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l4 = lv_label_create(sg_tracker.ui.btn_sos);
    lv_label_set_text(l4, "SOS");
}

static void __build_idle(lv_obj_t *parent)
{
    sg_tracker.ui.idle_cont = lv_obj_create(parent);
    lv_obj_set_size(sg_tracker.ui.idle_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_border_width(sg_tracker.ui.idle_cont, 0, 0);
    lv_obj_set_style_bg_opa(sg_tracker.ui.idle_cont, LV_OPA_TRANSP, 0);
    sg_tracker.ui.idle_label = lv_label_create(sg_tracker.ui.idle_cont);
    lv_label_set_text(sg_tracker.ui.idle_label, "Idle");
    lv_obj_center(sg_tracker.ui.idle_label);
}

static void __build_tracking(lv_obj_t *parent)
{
    sg_tracker.ui.track_cont = lv_obj_create(parent);
    lv_obj_set_size(sg_tracker.ui.track_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_border_width(sg_tracker.ui.track_cont, 0, 0);
    lv_obj_set_style_bg_opa(sg_tracker.ui.track_cont, LV_OPA_TRANSP, 0);

    sg_tracker.ui.calib_label = lv_label_create(sg_tracker.ui.track_cont);
    lv_label_set_text(sg_tracker.ui.calib_label, "Do 8-pattern calib");
    lv_obj_align(sg_tracker.ui.calib_label, LV_ALIGN_TOP_MID, 0, 8);

    sg_tracker.ui.calib_btn = lv_btn_create(sg_tracker.ui.track_cont);
    lv_obj_add_event_cb(sg_tracker.ui.calib_btn, __on_calib_btn, LV_EVENT_CLICKED, NULL);
    lv_obj_align_to(sg_tracker.ui.calib_btn, sg_tracker.ui.calib_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);
    lv_obj_t *cbt = lv_label_create(sg_tracker.ui.calib_btn);
    lv_label_set_text(cbt, "Mark Done");

    sg_tracker.ui.compass_cont = lv_obj_create(sg_tracker.ui.track_cont);
    lv_obj_set_size(sg_tracker.ui.compass_cont, 260, 260);
    lv_obj_set_style_radius(sg_tracker.ui.compass_cont, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(sg_tracker.ui.compass_cont, 2, 0);
    lv_obj_set_style_border_opa(sg_tracker.ui.compass_cont, LV_OPA_50, 0);
    lv_obj_center(sg_tracker.ui.compass_cont);

    /* needle: use image-less simple line via canvas alternative -> use arc + img placeholder */
    sg_tracker.ui.compass_needle = lv_img_create(sg_tracker.ui.compass_cont);
    /* No source -> use style to draw a line substitute by using 1x40 image transformed; fallback: use label '^' */
    static const char *needle_txt = "^";
    lv_obj_t *needle_label = lv_label_create(sg_tracker.ui.compass_needle);
    lv_label_set_text(needle_label, needle_txt);
    lv_obj_center(needle_label);

    sg_tracker.ui.compass_center = lv_obj_create(sg_tracker.ui.compass_cont);
    lv_obj_set_size(sg_tracker.ui.compass_center, 8, 8);
    lv_obj_set_style_radius(sg_tracker.ui.compass_center, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(sg_tracker.ui.compass_center, lv_color_black(), 0);
    lv_obj_center(sg_tracker.ui.compass_center);

    sg_tracker.ui.compass_timer = lv_timer_create(__compass_timer_cb, COMPASS_TIMER_MS, NULL);
}

static void __build_mic(lv_obj_t *parent)
{
    sg_tracker.ui.mic_cont = lv_obj_create(parent);
    lv_obj_set_size(sg_tracker.ui.mic_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(sg_tracker.ui.mic_cont, LV_OPA_TRANSP, 0);

    sg_tracker.ui.mic_label = lv_label_create(sg_tracker.ui.mic_cont);
    lv_label_set_text(sg_tracker.ui.mic_label, __dummy_is_asr_listening() ? "Listening" : "Idle");
    lv_obj_center(sg_tracker.ui.mic_label);

    sg_tracker.ui.mic_back_btn = lv_btn_create(sg_tracker.ui.mic_cont);
    lv_obj_add_event_cb(sg_tracker.ui.mic_back_btn, __on_mic_back, LV_EVENT_CLICKED, NULL);
    lv_obj_align(sg_tracker.ui.mic_back_btn, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_t *lbl = lv_label_create(sg_tracker.ui.mic_back_btn);
    lv_label_set_text(lbl, "Back");
}

static void __build_sos(lv_obj_t *parent)
{
    sg_tracker.ui.sos_cont = lv_obj_create(parent);
    lv_obj_set_size(sg_tracker.ui.sos_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(sg_tracker.ui.sos_cont, LV_OPA_TRANSP, 0);

    sg_tracker.ui.sos_label = lv_label_create(sg_tracker.ui.sos_cont);
    lv_label_set_text(sg_tracker.ui.sos_label, "Hold 3s to SOS");
    lv_obj_center(sg_tracker.ui.sos_label);

    lv_obj_t *sos_btn = lv_btn_create(sg_tracker.ui.sos_cont);
    lv_obj_set_size(sos_btn, 120, 120);
    lv_obj_set_style_radius(sos_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_align(sos_btn, LV_ALIGN_CENTER, 0, 40);
    lv_obj_add_event_cb(sos_btn, __on_sos_press, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(sos_btn, __on_sos_press, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(sos_btn, __on_sos_press, LV_EVENT_PRESS_LOST, NULL);
    lv_obj_t *sl = lv_label_create(sos_btn);
    lv_label_set_text(sl, "SOS");
    lv_obj_center(sl);

    sg_tracker.ui.sos_cancel_btn = lv_btn_create(sg_tracker.ui.sos_cont);
    lv_obj_add_event_cb(sg_tracker.ui.sos_cancel_btn, __on_sos_cancel, LV_EVENT_CLICKED, NULL);
    lv_obj_align(sg_tracker.ui.sos_cancel_btn, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_t *xl = lv_label_create(sg_tracker.ui.sos_cancel_btn);
    lv_label_set_text(xl, "X");
}

static void __build_settings_overlay(lv_obj_t *parent)
{
    sg_tracker.ui.settings_overlay = lv_obj_create(parent);
    lv_obj_set_size(sg_tracker.ui.settings_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(sg_tracker.ui.settings_overlay, LV_OPA_80, 0);
    lv_obj_set_style_bg_color(sg_tracker.ui.settings_overlay, lv_color_hex(0x000000), 0);
    lv_obj_add_flag(sg_tracker.ui.settings_overlay, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *panel = lv_obj_create(sg_tracker.ui.settings_overlay);
    lv_obj_set_size(panel, LV_PCT(90), LV_SIZE_CONTENT);
    lv_obj_set_style_radius(panel, 16, 0);
    lv_obj_center(panel);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(panel, 12, 0);

    sg_tracker.ui.gps_label = lv_label_create(panel);
    lv_label_set_text(sg_tracker.ui.gps_label, "GPS: -");

    sg_tracker.ui.time_label = lv_label_create(panel);
    lv_label_set_text(sg_tracker.ui.time_label, "--");

    lv_obj_t *vol = lv_slider_create(panel);
    lv_slider_set_range(vol, 0, 100);
    lv_slider_set_value(vol, 50, LV_ANIM_OFF);
    sg_tracker.ui.volume_slider = (lv_slider_t *)vol;
    lv_obj_add_event_cb(vol, __on_settings_volume, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *close_btn = lv_btn_create(panel);
    lv_obj_add_event_cb(close_btn, __settings_close, LV_EVENT_CLICKED, NULL);
    lv_obj_t *cl = lv_label_create(close_btn);
    lv_label_set_text(cl, "Close");
}

static void __route_to(TRACKER_SCREEN_E screen)
{
    sg_tracker.ui.current = screen;
    __footer_set_active(screen);
    switch (screen) {
    case SCREEN_IDLE:
        __ensure_only(sg_tracker.ui.idle_cont);
        break;
    case SCREEN_TRACKING:
        __ensure_only(sg_tracker.ui.track_cont);
        if (!sg_tracker.ui.is_calibrated) {
            lv_label_set_text(sg_tracker.ui.calib_label, "Do 8-pattern calib");
            lv_obj_clear_flag(sg_tracker.ui.calib_btn, LV_OBJ_FLAG_HIDDEN);
        }
        break;
    case SCREEN_MIC:
        __ensure_only(sg_tracker.ui.mic_cont);
        lv_label_set_text(sg_tracker.ui.mic_label, __dummy_is_asr_listening() ? "Listening" : "Idle");
        break;
    case SCREEN_SOS:
        __ensure_only(sg_tracker.ui.sos_cont);
        break;
    default: break;
    }
}

/***********************************************************
***********************public api***************************
***********************************************************/
int ui_init(UI_FONT_T *ui_font)
{
    /* cache fonts (text only used) */
    if (ui_font) {
        sg_tracker.font.text = ui_font->text;
        sg_tracker.font.icon = ui_font->icon;
    }

    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, sg_tracker.font.text, 0);
    lv_obj_set_style_text_color(screen, lv_color_white(), 0);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);

    /* root */
    sg_tracker.ui.root = lv_obj_create(screen);
    lv_obj_set_size(sg_tracker.ui.root, CIRCULAR_SIZE, CIRCULAR_SIZE);
    lv_obj_set_style_radius(sg_tracker.ui.root, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_clip_corner(sg_tracker.ui.root, true, 0);
    lv_obj_center(sg_tracker.ui.root);
    lv_obj_set_style_bg_color(sg_tracker.ui.root, lv_color_hex(0x101010), 0);
    lv_obj_set_style_border_width(sg_tracker.ui.root, 0, 0);
    lv_obj_add_event_cb(sg_tracker.ui.root, __on_gesture, LV_EVENT_GESTURE, NULL);

    /* circular content */
    sg_tracker.ui.circular = lv_obj_create(sg_tracker.ui.root);
    lv_obj_set_size(sg_tracker.ui.circular, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(sg_tracker.ui.circular, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sg_tracker.ui.circular, 0, 0);

    __build_idle(sg_tracker.ui.circular);
    __build_tracking(sg_tracker.ui.circular);
    __build_mic(sg_tracker.ui.circular);
    __build_sos(sg_tracker.ui.circular);
    __build_settings_overlay(sg_tracker.ui.root);
    __build_footer(sg_tracker.ui.root);

    sg_tracker.ui.is_calibrated = false;
    sg_tracker.ui.dummy_heading_deg = 0;
    __route_to(SCREEN_IDLE);

    return 0;
}

void ui_set_user_msg(const char *text)
{
    (void)text;
}

void ui_set_assistant_msg(const char *text)
{
    (void)text;
}

void ui_set_system_msg(const char *text)
{
    (void)text;
}

void ui_set_emotion(const char *emotion)
{
    (void)emotion;
}

void ui_set_status(const char *status)
{
    (void)status;
}

void ui_set_notification(const char *notification)
{
    (void)notification;
}

void ui_set_network(char *wifi_icon)
{
    (void)wifi_icon;
}

void ui_set_chat_mode(const char *chat_mode)
{
    (void)chat_mode;
}

void ui_set_status_bar_pad(int32_t value)
{
    (void)value;
}

#if defined(ENABLE_GUI_STREAM_AI_TEXT) && (ENABLE_GUI_STREAM_AI_TEXT == 1)
void ui_set_assistant_msg_stream_start(void) {}
void ui_set_assistant_msg_stream_data(const char *text) { (void)text; }
void ui_set_assistant_msg_stream_end(void) {}
void ui_set_assistant_msg_stream_interrupt(void) {}
#endif

#endif /* ENABLE_GUI_TRACKER */


