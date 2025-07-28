idf_component_register(SRCS "key_definitions.c" "keymap.c" "macros.c" "function_key.c" "keymap_json.c" "layout_store.c"
                    INCLUDE_DIRS "include"
                    REQUIRES quantum wifi json control led_ctrl)
