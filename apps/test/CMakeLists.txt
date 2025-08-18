##
# @file CMakeLists.txt
# @brief 
#/

# APP_PATH
set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})

# APP_NAME
get_filename_component(APP_NAME ${APP_PATH} NAME)

# APP_SRCS
# aux_source_directory(${APP_PATH}/src APP_SRCS)
file(GLOB_RECURSE SRC_FILES "${APP_PATH}/src/*.c")
file(GLOB_RECURSE SRC_AT_MODEM_FILES "${APP_PATH}/src/at_modem/*.c")
file(GLOB_RECURSE SRC_AT_MODULE_FILES "${APP_PATH}/src/at_modem/module/*.c")

set(APP_SRCS ${SRC_FILES} ${SRC_AT_MODEM_FILES} ${SRC_AT_MODULE_FILES})

# APP_INC
set(APP_INC
    ${APP_PATH}/include
    ${APP_PATH}/include/at_modem
    ${APP_PATH}/include/at_modem/module

)

########################################
# Target Configure
########################################
add_library(${EXAMPLE_LIB})

target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${APP_SRCS}
    )

target_include_directories(${EXAMPLE_LIB}
    PRIVATE
        ${APP_INC}
    )
