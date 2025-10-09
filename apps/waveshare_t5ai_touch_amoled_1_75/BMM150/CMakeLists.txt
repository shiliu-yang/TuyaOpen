##
# @file CMakeLists.txt
# @brief BMM150 Magnetometer Demo CMake Configuration
#/

if (CONFIG_ENABLE_GPIO STREQUAL "y")

# APP_PATH
set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})

# APP_NAME
get_filename_component(APP_NAME ${APP_PATH} NAME)

# APP_SRCS
aux_source_directory(${APP_PATH}/src APP_SRCS)


########################################
# Target Configure
########################################
add_library(${EXAMPLE_LIB})

target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${APP_SRCS}
    )
    
# APP include path
target_include_directories(${EXAMPLE_LIB}
    PUBLIC ${APP_PATH}/src
)
else()
message(FATAL_ERROR "sw i2c cannot work when CONFIG_ENABLE_GPIO is not set")
endif()
