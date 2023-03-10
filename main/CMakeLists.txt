idf_component_register(SRCS "mk32_main.c" "matrix.c"
                    INCLUDE_DIRS "."
                    REQUIRES quantum tmk_core ble web tinyusb control)
