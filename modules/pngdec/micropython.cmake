add_library(usermod_pngdec INTERFACE)

target_sources(usermod_pngdec INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/pngdec.c
    ${CMAKE_CURRENT_LIST_DIR}/pngdec.cpp

    ${CMAKE_CURRENT_LIST_DIR}/lib/PNGdec.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/adler32.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/crc32.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/infback.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/inffast.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/inflate.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/inftrees.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/zutil.c
)

target_include_directories(usermod_pngdec INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_compile_definitions(usermod_pngdec INTERFACE
    MODULE_PNGDEC_ENABLED=1
)

target_link_libraries(usermod INTERFACE usermod_pngdec)