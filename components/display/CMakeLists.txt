idf_component_register(SRCS "lcd_hardware.c"
                              "menu_state_machine.c"
                              "keyboard_gui.c"
                              "keyboard_gui_widgets.c"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "private_include"
                       REQUIRES esp_lcd esp_driver_gpio esp_driver_spi esp_lvgl_port
                       PRIV_REQUIRES driver drv_loop keymap)
