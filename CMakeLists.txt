cmake_minimum_required(VERSION 3.9)

project(PicoVision)

install(FILES
    ${CMAKE_CURRENT_LIST_DIR}/examples/main.py
    ${CMAKE_CURRENT_LIST_DIR}/examples/rainbow_wheel.py
    ${CMAKE_CURRENT_LIST_DIR}/examples/starfield_rainbow.py
    ${CMAKE_CURRENT_LIST_DIR}/examples/starfield.py
    ${CMAKE_CURRENT_LIST_DIR}/examples/vector_clock_smooth.py
    ${CMAKE_CURRENT_LIST_DIR}/examples/vector_clock.py
    ${CMAKE_CURRENT_LIST_DIR}/examples/toaster.png
    DESTINATION .
)

set(examples
    alert
    bouncing_logo
    floppy_birb
    pride
    sneks_and_ladders
)

foreach(example IN LISTS examples)
install(DIRECTORY
    ${CMAKE_CURRENT_LIST_DIR}/examples/${example}/
    DESTINATION ${example}
)
endforeach()

# setup release packages
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
set(CPACK_GENERATOR "ZIP")
include(CPack)