add_library(pico_synth_i2s INTERFACE)

pico_generate_pio_header(pico_synth_i2s ${CMAKE_CURRENT_LIST_DIR}/pico_synth_i2s.pio)


target_sources(pico_synth_i2s INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/pico_synth_i2s.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../pico_synth/pico_synth.cpp
)

target_include_directories(pico_synth_i2s INTERFACE ${CMAKE_CURRENT_LIST_DIR})

# Pull in pico libraries that we need
target_link_libraries(pico_synth_i2s INTERFACE pico_stdlib hardware_pio hardware_dma)