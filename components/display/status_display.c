/*
 *  This program is a keyboard firmware for ESP family boards inspired by
 *  MK32 and qmk_firmware.
 *
 *  Copyright (C) 2024 github.com/paul356
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "status_display.h"
#include "keyboard_gui.h"
#include "esp_log.h"

static const char *TAG = "status_display";

esp_err_t init_display(void)
{
    ESP_LOGI(TAG, "Initializing display");
    
    esp_err_t ret = keyboard_gui_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize keyboard GUI: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Display initialization complete");
    return ESP_OK;
}

void update_display(uint16_t last_key)
{
    keyboard_gui_update_stats(last_key);
}

void display_handle_encoder_rotation(int direction)
{
    keyboard_gui_handle_encoder(direction);
}

void display_handle_enter(void)
{
    keyboard_gui_handle_enter();
}

void display_handle_esc(void)
{
    keyboard_gui_handle_esc();
}

void display_update_task(void)
{
    keyboard_gui_update();
}

esp_err_t display_set_brightness(uint8_t brightness)
{
    return keyboard_gui_set_brightness(brightness);
}

esp_err_t display_on_off(bool on)
{
    return keyboard_gui_display_on_off(on);
}
