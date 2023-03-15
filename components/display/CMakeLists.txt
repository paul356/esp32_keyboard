idf_component_register(SRCS "status_display.c" "bluetooth_icon.c" "wifi_icon.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_lcd lvgl esp_lvgl_port keymap)
