idf_component_register(SRCS "status_display.c"
                              "lcd_hardware_st7789.c"
                              "menu_state_machine.c"
                              "keyboard_gui.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_lcd esp_driver_gpio esp_driver_spi esp_lvgl_port
                       PRIV_REQUIRES driver)
