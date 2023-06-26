set(MOD_NAME picographics)
string(TOUPPER ${MOD_NAME} MOD_NAME_UPPER)
add_library(usermod_${MOD_NAME} INTERFACE)

target_sources(usermod_${MOD_NAME} INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/${MOD_NAME}.c
    ${CMAKE_CURRENT_LIST_DIR}/${MOD_NAME}.cpp
    ${PIMORONI_PICO_PATH}/drivers/dv_display/dv_display.cpp
    ${PIMORONI_PICO_PATH}/drivers/dv_display/swd_load.cpp
    ${PIMORONI_PICO_PATH}/drivers/aps6404/aps6404.cpp
    ${PIMORONI_PICO_PATH}/libraries/pico_graphics/pico_graphics.cpp
    ${PIMORONI_PICO_PATH}/libraries/pico_graphics/pico_graphics_pen_dv_rgb888.cpp
    ${PIMORONI_PICO_PATH}/libraries/pico_graphics/pico_graphics_pen_dv_rgb555.cpp
    ${PIMORONI_PICO_PATH}/libraries/pico_graphics/pico_graphics_pen_dv_p5.cpp
    ${PIMORONI_PICO_PATH}/libraries/pico_graphics/types.cpp
)

pico_generate_pio_header(usermod_${MOD_NAME} ${PIMORONI_PICO_PATH}/drivers/dv_display/swd.pio)
pico_generate_pio_header(usermod_${MOD_NAME} ${PIMORONI_PICO_PATH}/drivers/aps6404/aps6404.pio)


target_include_directories(usermod_${MOD_NAME} INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
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
