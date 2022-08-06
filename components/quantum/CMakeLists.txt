set(SRC_FILES
    matrix.c
    matrix_common.c
    quantum.c
    keymap_common.c
    debounce/sym_defer_g.c
    keycode_config.c)
set(INC_DIRS
    ./
    audio/
    process_keycode/
    ${CMAKE_SOURCE_DIR}/main)

idf_component_register(SRCS ${SRC_FILES}
                       INCLUDE_DIRS ${INC_DIRS}
                       REQUIRES "tmk_core")
