idf_component_register(SRCS "key_definitions.c" "keymap.c"
                    INCLUDE_DIRS "include" "${CMAKE_SOURCE_DIR}/main"
                    REQUIRES quantum)
