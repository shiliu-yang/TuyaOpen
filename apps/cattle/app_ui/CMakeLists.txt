SET(SOURCES screens/ui_ai_chat.c
    screens/ui_setting.c
    screens/ui_SOS.c
    screens/ui_tracker.c
    screens/ui_tracker_close.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    ui_events.c
    images/ui_img_image_volume_png.c
    images/ui_img_image_wififill_png.c
    images/ui_img_image_battery100_png.c
    images/ui_img_image_gps_png.c
    fonts/ui_font_mont_82.c)

add_library(ui ${SOURCES})
