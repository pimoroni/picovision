set(MOD_NAME picographics)
string(TOUPPER ${MOD_NAME} MOD_NAME_UPPER)
add_library(usermod_${MOD_NAME} INTERFACE)

get_filename_component(PICOVISION_PATH ${CMAKE_CURRENT_LIST_DIR}/../.. ABSOLUTE)

target_sources(usermod_${MOD_NAME} INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/${MOD_NAME}.c
    ${CMAKE_CURRENT_LIST_DIR}/${MOD_NAME}.cpp
    ${PICOVISION_PATH}/drivers/dv_display/dv_display.cpp
    ${PICOVISION_PATH}/drivers/dv_display/swd_load.cpp
    ${PICOVISION_PATH}/drivers/aps6404/aps6404.cpp
    ${PIMORONI_PICO_PATH}/libraries/pico_graphics/pico_graphics.cpp
    ${PICOVISION_PATH}/libraries/pico_graphics/pico_graphics_pen_dv_rgb888.cpp
    ${PICOVISION_PATH}/libraries/pico_graphics/pico_graphics_pen_dv_rgb555.cpp
    ${PICOVISION_PATH}/libraries/pico_graphics/pico_graphics_pen_dv_p5.cpp
    ${PIMORONI_PICO_PATH}/libraries/pico_graphics/types.cpp
)

pico_generate_pio_header(usermod_${MOD_NAME} ${PICOVISION_PATH}/drivers/dv_display/swd.pio)
pico_generate_pio_header(usermod_${MOD_NAME} ${PICOVISION_PATH}/drivers/aps6404/aps6404.pio)


target_include_directories(usermod_${MOD_NAME} INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${PICOVISION_PATH}
    ${PICOVISION_PATH}/drivers/aps6404
    ${PICOVISION_PATH}/drivers/dv_display
    ${PICOVISION_PATH}/libraries/pico_graphics     # for pico_graphics_dv.hpp
    ${PIMORONI_PICO_PATH}/libraries/pico_graphics  # for pico_graphics.hpp
    ${PIMORONI_PICO_PATH}/libraries/pngdec
)

target_compile_definitions(usermod_${MOD_NAME} INTERFACE
    -DMODULE_${MOD_NAME_UPPER}_ENABLED=1
)

target_link_libraries(usermod INTERFACE usermod_${MOD_NAME})

set_source_files_properties(
    ${CMAKE_CURRENT_LIST_DIR}/${MOD_NAME}.c
    PROPERTIES COMPILE_FLAGS
    "-Wno-discarded-qualifiers"
)
