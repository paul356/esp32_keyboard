set(SRC_FILES
    keymap_server.c)
set(INC_DIRS
    ./
    ${CMAKE_SOURCE_DIR}/main
    )
idf_component_register(SRCS ${SRC_FILES}
                       INCLUDE_DIRS ${INC_DIRS}
                       REQUIRES esp_http_server json app_update
    nvsfuncs keymap)
