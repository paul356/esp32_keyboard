idf_component_register(SRCS "idle_detection.c"
                            "display_power_mgmt.c"
                            "led_power_mgmt.c"
                            "ble_power_mgmt.c"
                    INCLUDE_DIRS "include"
                    PRIV_INCLUDE_DIRS "prv_include"
                    REQUIRES "esp_timer" "display" "miscs" "led_ctrl" "ble" "control"
                    PRIV_REQUIRES "freertos")
