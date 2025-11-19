idf_component_register(SRCS "miscs.c" "led_drv.c"
                       INCLUDE_DIRS "include"
                       REQUIRES driver esp_driver_gpio esp_adc drv_loop)
