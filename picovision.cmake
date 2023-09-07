set(LIB_NAME picovision)
add_library(${LIB_NAME} INTERFACE)

target_sources(${LIB_NAME} INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/drivers/dv_display/dv_display.cpp
    ${CMAKE_CURRENT_LIST_DIR}/drivers/dv_display/swd_load.cpp
    ${CMAKE_CURRENT_LIST_DIR}/drivers/aps6404/aps6404.cpp
    ${PIMORONI_PICO_PATH}/libraries/pico_graphics/pico_graphics.cpp
    ${CMAKE_CURRENT_LIST_DIR}/libraries/pico_graphics/pico_graphics_pen_dv_rgb888.cpp
    ${CMAKE_CURRENT_LIST_DIR}/libraries/pico_graphics/pico_graphics_pen_dv_rgb555.cpp
    ${CMAKE_CURRENT_LIST_DIR}/libraries/pico_graphics/pico_graphics_pen_dv_p5.cpp
    ${PIMORONI_PICO_PATH}/libraries/pico_graphics/types.cpp
)

pico_generate_pio_header(${LIB_NAME} ${CMAKE_CURRENT_LIST_DIR}/drivers/dv_display/swd.pio)
pico_generate_pio_header(${LIB_NAME} ${CMAKE_CURRENT_LIST_DIR}/drivers/aps6404/aps6404.pio)

target_include_directories(${LIB_NAME} INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/drivers/aps6404
    ${CMAKE_CURRENT_LIST_DIR}/drivers/dv_display
    ${CMAKE_CURRENT_LIST_DIR}/libraries/pico_graphics     # for pico_graphics_dv.hpp
    ${PIMORONI_PICO_PATH}/libraries/pico_graphics  # for pico_graphics.hpp
    ${PIMORONI_PICO_PATH}/libraries/pngdec
)

target_link_libraries(${LIB_NAME} INTERFACE pico_stdlib pimoroni_i2c hardware_i2c hardware_pio hardware_dma)