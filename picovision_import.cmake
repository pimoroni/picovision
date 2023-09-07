# This file can be dropped into a project to help locate the Picovision library
# It will also set up the required include and module search paths.

if (NOT PIMORONI_PICOVISION_PATH)
    set(PIMORONI_PICOVISION_PATH "../../picovision/")
endif()

if(NOT IS_ABSOLUTE ${PIMORONI_PICOVISION_PATH})
    get_filename_component(
        PIMORONI_PICOVISION_PATH
        "${CMAKE_CURRENT_BINARY_DIR}/${PIMORONI_PICOVISION_PATH}"
        ABSOLUTE)
endif()

if (NOT EXISTS ${PIMORONI_PICOVISION_PATH})
    message(FATAL_ERROR "Directory '${PIMORONI_PICOVISION_PATH}' not found")
endif ()

if (NOT EXISTS ${PIMORONI_PICOVISION_PATH}/picovision_import.cmake)
    message(FATAL_ERROR "Directory '${PIMORONI_PICOVISION_PATH}' does not appear to contain the Picovision library")
endif ()

message("PIMORONI_PICOVISION_PATH is ${PIMORONI_PICOVISION_PATH}")

set(PIMORONI_PICOVISION_PATH ${PIMORONI_PICOVISION_PATH} CACHE PATH "Path to the Picovision libraries" FORCE)

include_directories(${PIMORONI_PICOVISION_PATH})
list(APPEND CMAKE_MODULE_PATH ${PIMORONI_PICOVISION_PATH})

include(picovision)
include_directories(${PIMORONI_PICO_PATH}/libraries/pico_graphics)
