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

#include "lcd_hardware_st7789.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lvgl_port.h"

static const char *TAG = "lcd_hw_st7789";

static lcd_hardware_t s_lcd_hardware = {0};

static bool lcd_flush_ready_callback(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_display_t *disp = (lv_display_t *)user_ctx;
    lv_display_flush_ready(disp);
    return false;
}

static void lcd_lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(disp);
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map);
}

esp_err_t lcd_hardware_init(void)
{
    if (s_lcd_hardware.initialized) {
        ESP_LOGW(TAG, "LCD hardware already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initialize SPI bus");
    spi_bus_config_t buscfg = {
        .sclk_io_num = LCD_PIN_CLK,
        .mosi_io_num = LCD_PIN_MOSI,
        .miso_io_num = GPIO_NUM_NC,  // MISO not used
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = LCD_WIDTH * LCD_HEIGHT * LCD_BIT_PER_PIXEL / 8,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO), TAG, "SPI bus init failed");

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = LCD_PIN_DC,
        .cs_gpio_num = LCD_PIN_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .on_color_trans_done = lcd_flush_ready_callback,
        .user_ctx = NULL,  // Will be set later when LVGL display is created
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &io_config, &s_lcd_hardware.io_handle), 
                        TAG, "New panel IO failed");

    ESP_LOGI(TAG, "Install ST7789 panel driver");
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = LCD_BIT_PER_PIXEL,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7789(s_lcd_hardware.io_handle, &panel_config, &s_lcd_hardware.panel_handle), 
                        TAG, "New panel failed");

    ESP_LOGI(TAG, "Reset the display");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_lcd_hardware.panel_handle), TAG, "Panel reset failed");

    ESP_LOGI(TAG, "Initialize the display");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_lcd_hardware.panel_handle), TAG, "Panel init failed");

    ESP_LOGI(TAG, "Invert display colors (if needed)");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(s_lcd_hardware.panel_handle, true), TAG, "Panel invert failed");

    ESP_LOGI(TAG, "Turn on the screen");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_lcd_hardware.panel_handle, true), TAG, "Panel display on failed");

    // Initialize backlight
    ESP_LOGI(TAG, "Initialize backlight");
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << LCD_PIN_BL
    };
    ESP_RETURN_ON_ERROR(gpio_config(&bk_gpio_config), TAG, "Backlight GPIO config failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(LCD_PIN_BL, 1), TAG, "Backlight on failed");

    ESP_LOGI(TAG, "Initialize LVGL display");
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "LVGL port init failed");

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = s_lcd_hardware.io_handle,
        .panel_handle = s_lcd_hardware.panel_handle,
        .buffer_size = LCD_WIDTH * LCD_HEIGHT / 4,  // 1/4 screen buffer
        .double_buffer = true,
        .hres = LCD_WIDTH,
        .vres = LCD_HEIGHT,
        .monochrome = false,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = true,
        }
    };
    s_lcd_hardware.lvgl_display = lvgl_port_add_disp(&disp_cfg);
    ESP_RETURN_ON_FALSE(s_lcd_hardware.lvgl_display, ESP_FAIL, TAG, "Add LVGL display failed");

    // Update the user context for the flush callback
    esp_lcd_panel_io_spi_config_t *spi_config = (esp_lcd_panel_io_spi_config_t *)&io_config;
    spi_config->user_ctx = s_lcd_hardware.lvgl_display;

    // Set panel handle as user data for LVGL display
    lv_display_set_user_data(s_lcd_hardware.lvgl_display, s_lcd_hardware.panel_handle);

    s_lcd_hardware.initialized = true;
    ESP_LOGI(TAG, "LCD hardware initialization completed");

    return ESP_OK;
}

lcd_hardware_t* lcd_hardware_get_instance(void)
{
    return &s_lcd_hardware;
}

esp_err_t lcd_hardware_set_backlight(uint8_t brightness)
{
    if (!s_lcd_hardware.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Simple on/off control - could be enhanced with PWM for brightness control
    bool level = brightness > 50 ? 1 : 0;
    return gpio_set_level(LCD_PIN_BL, level);
}

esp_err_t lcd_hardware_display_on_off(bool on)
{
    if (!s_lcd_hardware.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    return esp_lcd_panel_disp_on_off(s_lcd_hardware.panel_handle, on);
}

esp_err_t lcd_hardware_deinit(void)
{
    if (!s_lcd_hardware.initialized) {
        return ESP_OK;
    }

    if (s_lcd_hardware.lvgl_display) {
        lvgl_port_remove_disp(s_lcd_hardware.lvgl_display);
        s_lcd_hardware.lvgl_display = NULL;
    }

    if (s_lcd_hardware.panel_handle) {
        esp_lcd_panel_del(s_lcd_hardware.panel_handle);
        s_lcd_hardware.panel_handle = NULL;
    }

    if (s_lcd_hardware.io_handle) {
        esp_lcd_panel_io_del(s_lcd_hardware.io_handle);
        s_lcd_hardware.io_handle = NULL;
    }

    spi_bus_free(LCD_SPI_HOST);
    lvgl_port_deinit();

    gpio_set_level(LCD_PIN_BL, 0);  // Turn off backlight

    s_lcd_hardware.initialized = false;
    ESP_LOGI(TAG, "LCD hardware deinitialized");

    return ESP_OK;
}
