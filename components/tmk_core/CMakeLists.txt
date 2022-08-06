set(SRC_FILES
    common/keyboard.c
    common/action.c
    common/action_layer.c
    common/action_macro.c
    common/action_tapping.c
    common/action_util.c
    common/report.c
    common/host.c
    common/debug.c
    common/magic.c
    )

idf_component_register(SRCS ${SRC_FILES}
                       INCLUDE_DIRS "common/"
                       REQUIRES quantum)
