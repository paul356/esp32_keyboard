idf_component_register(SRCS "lcd_hardware.c"
                              "menu_state_machine.c"
                              "keyboard_gui.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_lcd esp_driver_gpio esp_driver_spi esp_lvgl_port
                       PRIV_REQUIRES driver drv_loop)
