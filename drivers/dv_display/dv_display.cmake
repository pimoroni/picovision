set(DRIVER_NAME dv_display)

# SWD loader
add_library(swd_load INTERFACE)

target_sources(swd_load INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/swd_load.cpp)

target_include_directories(swd_load INTERFACE ${CMAKE_CURRENT_LIST_DIR})

pico_generate_pio_header(swd_load ${CMAKE_CURRENT_LIST_DIR}/swd.pio)

target_link_libraries(swd_load INTERFACE pico_stdlib)

# main DV display driver
add_library(${DRIVER_NAME} INTERFACE)

target_sources(${DRIVER_NAME} INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/${DRIVER_NAME}.cpp)

target_include_directories(${DRIVER_NAME} INTERFACE ${CMAKE_CURRENT_LIST_DIR})

# Pull in pico libraries that we need
target_link_libraries(${DRIVER_NAME} INTERFACE swd_load pico_stdlib aps6404 pimoroni_i2c)
