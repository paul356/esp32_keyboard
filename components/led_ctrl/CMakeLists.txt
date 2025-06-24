idf_component_register(SRCS "led_ctrl.c"
                    INCLUDE_DIRS "include"
                    REQUIRES "drv_loop" "miscs" "esp_timer"
                    PRIV_REQUIRES "freertos")
