set(COMMON_PATH ${CMAKE_CURRENT_LIST_DIR})

file(GLOB_RECURSE FONT_SRC "${COMMON_PATH}/display/font/*.c")
file(GLOB_RECURSE EMOJI_SRC "${COMMON_PATH}/display/emoji/*.c")

set(BOARD_SRC
    "${FONT_SRC}"
    "${EMOJI_SRC}"
    "${COMMON_PATH}/audio/tdd_audio_no_codec.c"
    "${COMMON_PATH}/audio/tdd_audio_8311_codec.c"
    "${COMMON_PATH}/display/oled_display.c"
    "${COMMON_PATH}/display/esp_io_expander.c"
    "${COMMON_PATH}/display/esp_io_expander_tca9554.c"
    "${COMMON_PATH}/display/lcd_display.c"
    "${COMMON_PATH}/display/sh8601.c"
    "${COMMON_PATH}/display/esp_lcd_sh8601.c"
    )

set(BOARD_INC
    "${COMMON_PATH}/audio"
    "${COMMON_PATH}/display"
    "${COMMON_PATH}/display/font"
    )
