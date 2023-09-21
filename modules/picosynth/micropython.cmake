set(MOD_NAME picosynth)
string(TOUPPER ${MOD_NAME} MOD_NAME_UPPER)
add_library(usermod_${MOD_NAME} INTERFACE)

get_filename_component(PICOVISION_PATH ${CMAKE_CURRENT_LIST_DIR}/../.. ABSOLUTE)

target_sources(usermod_${MOD_NAME} INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/${MOD_NAME}.c
    ${CMAKE_CURRENT_LIST_DIR}/${MOD_NAME}.cpp
    ${PIMORONI_PICO_PATH}/libraries/pico_synth/pico_synth.cpp
    ${PICOVISION_PATH}/libraries/pico_synth_i2s/pico_synth_i2s.cpp
)

pico_generate_pio_header(usermod_${MOD_NAME} ${PICOVISION_PATH}/libraries/pico_synth_i2s/pico_synth_i2s.pio)


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
