idf_component_register(SRCS "mk32_main.c"
                    INCLUDE_DIRS "."
                    REQUIRES quantum tmk_core ble web hid tinyusb control
                    port_mgmt display)
