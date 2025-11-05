##
# @file CMakeLists.txt
# @brief 
#/

# APP_PATH
set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})

# APP_NAME
get_filename_component(APP_NAME ${APP_PATH} NAME)

# APP_SRC
file(GLOB_RECURSE APP_SRC "${APP_PATH}/src/*.c")

# APP_INC
set(APP_INC 
        ${APP_PATH}/include 
        ${APP_PATH}/include/func)

list(APPEND APP_INC "${APP_PATH}/assets")

# APP_OPTIONS
set(APP_OPTIONS "")  # -W

# APP UI
if (CONFIG_ENABLE_APP_UI STREQUAL "y")
    file(GLOB_RECURSE APP_UI_SRC 
        "${APP_PATH}/app_ui/*.c" )

    set(APP_UI_INC ${APP_PATH}/app_ui)
    list(APPEND APP_SRC ${APP_UI_SRC})
    list(APPEND APP_INC ${APP_UI_INC})
endif()

# DP
add_subdirectory(${APP_PATH}/app_dp)
list(APPEND APP_SRC ${DP_SRCS})
list(APPEND APP_INC ${DP_INCS})

########################################
# Target Configure
########################################
add_library(${EXAMPLE_LIB})
message(STATUS "EXAMPLE_LIB:${APP_PATH}")

target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${APP_SRC}
    )

target_include_directories(${EXAMPLE_LIB}
    PRIVATE
        ${APP_INC}
    )

target_compile_options(${EXAMPLE_LIB}
    PRIVATE
        ${APP_OPTIONS}
    )

########################################
# Add subdirectory
########################################
add_subdirectory(${APP_PATH}/ai_components/ai_audio)
target_include_directories(${EXAMPLE_LIB} PRIVATE ${APP_PATH}/ai_components/ai_audio)
