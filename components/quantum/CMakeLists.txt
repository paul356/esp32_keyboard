set(SRC_FILES
    matrix.c
    matrix_common.c)
set(INC_DIRS
    ./
    audio/
    process_keycode/
    ../../main/)

idf_component_register(SRCS ${SRC_FILES}
                       INCLUDE_DIRS ${INC_DIRS}
                       REQUIRES "tmk_core")
