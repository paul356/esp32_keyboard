idf_component_register(SRCS "mk32_main.c"
                    INCLUDE_DIRS "."
                    REQUIRES quantum tmk_core ble web hid_desc tinyusb control
                    matrix_config display miscs debug)
