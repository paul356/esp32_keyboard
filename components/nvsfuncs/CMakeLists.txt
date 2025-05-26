idf_component_register(SRCS "nvs_io.c" "arr_conv.c"
                    INCLUDE_DIRS "include" "${CMAKE_SOURCE_DIR}/main"
                    REQUIRES nvs_flash quantum)
