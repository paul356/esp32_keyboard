idf_component_register(SRCS "idle_detection.c"
                            "display_power_mgmt.c"
                    INCLUDE_DIRS "include"
                    PRIV_INCLUDE_DIRS "prv_include"
                    REQUIRES "esp_timer" "display" "miscs"
                    PRIV_REQUIRES "freertos")
