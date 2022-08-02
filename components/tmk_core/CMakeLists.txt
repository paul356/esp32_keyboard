set(SRC_FILES
    common/keyboard.c
    )

idf_component_register(SRCS ${SRC_FILES}
                       INCLUDE_DIRS "common/")
