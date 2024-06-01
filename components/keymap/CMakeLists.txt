idf_component_register(SRCS "key_definitions.c" "keymap.c" "macros.c" "function_key.c"
                    INCLUDE_DIRS "include" "${CMAKE_SOURCE_DIR}/main"
                    REQUIRES quantum wifi)
