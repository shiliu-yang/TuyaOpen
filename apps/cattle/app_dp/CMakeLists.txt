# DP_PATH
set(DP_PATH ${CMAKE_CURRENT_LIST_DIR})

# Collect all source files from current directory
file(GLOB DP_SRCS
    ${DP_PATH}/*.c
)

# Set include directories
set(DP_INCS
    ${DP_PATH}
)

# Export variables to parent scope
set(DP_SRCS ${DP_SRCS} PARENT_SCOPE)
set(DP_INCS ${DP_INCS} PARENT_SCOPE)
