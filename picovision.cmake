
include(drivers/aps6404/aps6404)
include(drivers/dv_display/dv_display)

set(LIB_NAME picovision)
add_library(${LIB_NAME} INTERFACE)

target_sources(${LIB_NAME} INTERFACE
    ${PIMORONI_PICO_PATH}/libraries/pico_graphics/pico_graphics.cpp
    ${CMAKE_CURRENT_LIST_DIR}/libraries/pico_graphics/pico_graphics_pen_dv_rgb888.cpp
    ${CMAKE_CURRENT_LIST_DIR}/libraries/pico_graphics/pico_graphics_pen_dv_rgb555.cpp
    ${CMAKE_CURRENT_LIST_DIR}/libraries/pico_graphics/pico_graphics_pen_dv_p5.cpp
    ${PIMORONI_PICO_PATH}/libraries/pico_graphics/types.cpp
)

target_include_directories(${LIB_NAME} INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/libraries/pico_graphics     # for pico_graphics_dv.hpp
    ${PIMORONI_PICO_PATH}/libraries/pico_graphics  # for pico_graphics.hpp
    ${PIMORONI_PICO_PATH}/libraries/pngdec
)

target_link_libraries(${LIB_NAME} INTERFACE aps6404 dv_display pico_stdlib)