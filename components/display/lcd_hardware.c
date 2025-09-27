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

#include "lcd_hardware.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "keyboard_gui.h"

#define LCD_BIT_PER_PIXEL   16

// GPIO Pin Definitions
#define LCD_PIN_MOSI        11
#define LCD_PIN_CLK         12
#define LCD_PIN_CS          10
#define LCD_PIN_DC          13
#define LCD_PIN_RST         9
#define LCD_PIN_BL          14

// SPI Configuration
#define LCD_SPI_HOST        SPI2_HOST
#define LCD_PIXEL_CLOCK_HZ  (20 * 1000 * 1000)  // 20MHz
#define DRAW_BUFFER_SIZE    (LCD_WIDTH * LCD_HEIGHT * sizeof(lv_color16_t) / 10)
#define SCREEN_OFFSET_X     18  // Offset for X coordinate, adjust as needed
#define SCREEN_OFFSET_Y     82

/**
 * @brief LCD hardware interface structure
 */
typedef struct {
    esp_lcd_panel_handle_t panel_handle;
    esp_lcd_panel_io_handle_t io_handle;
    lv_disp_t *lvgl_display;
    bool initialized;
} lcd_hardware_t;

static const char *TAG = "lcd_driver";

static lcd_hardware_t s_lcd_hardware = {0};

static bool lcd_flush_ready_callback(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_display_t *disp = (lv_display_t *)user_ctx;
    lv_display_flush_ready(disp);
    return false;
}

static void lvgl_tick_func(void *arg)
{
    (void)arg;
    lv_tick_inc(20);
}

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, unsigned char *px_map)
{
    // Implement the flush callback to send the buffer to the display
    esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(disp);
    esp_lcd_panel_swap_xy(panel_handle, true);
    esp_lcd_panel_mirror(panel_handle, true, false);

    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;

    esp_lcd_panel_invert_color(panel_handle, false);
    lv_draw_sw_rgb565_swap(px_map, (offsetx2 + 1 - offsetx1) * (offsety2 + 1 - offsety1));
    // copy a buffer's content to a specific area of the display
    offsetx1 += SCREEN_OFFSET_X;
    offsetx2 += SCREEN_OFFSET_X;
    offsety1 += SCREEN_OFFSET_Y;
    offsety2 += SCREEN_OFFSET_Y;
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map);
}

static esp_err_t init_lcd_panel(esp_lcd_panel_io_handle_t *io_handle, esp_lcd_panel_handle_t *panel_handle)
{
    ESP_LOGI(TAG, "initialize display panel");

    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << LCD_PIN_BL
    };
    ESP_RETURN_ON_ERROR(gpio_config(&bk_gpio_config), TAG, "Failed to configure backlight GPIO");

    spi_bus_config_t buscfg = {
        .miso_io_num = GPIO_NUM_NC,  // MISO not used
        .mosi_io_num = LCD_PIN_MOSI,
        .sclk_io_num = LCD_PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = DRAW_BUFFER_SIZE
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO), TAG, "Failed to initialize SPI bus");

    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = LCD_PIN_DC,
        .cs_gpio_num = LCD_PIN_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };

    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi(LCD_SPI_HOST, &io_config, io_handle), TAG, "Failed to create LCD panel IO");

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };

    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7789(*io_handle, &panel_config, panel_handle), TAG, "Failed to create LCD panel");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(*panel_handle), TAG, "Failed to reset LCD panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(*panel_handle), TAG, "Failed to initialize LCD panel");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(*panel_handle, true), TAG, "Failed to turn on LCD panel");

    esp_lcd_panel_invert_color(*panel_handle, true);
    ESP_LOGI(TAG, "Turn on LCD backlight");
    gpio_set_level(LCD_PIN_BL, 1);

    return ESP_OK;
}

static esp_err_t init_lvgl(esp_lcd_panel_io_handle_t io_handle, void *user_data, lv_disp_t **lv_disp)
{
    ESP_LOGI(TAG, "Initialize LVGL");
    lv_init();

    void *buf1 = spi_bus_dma_memory_alloc(LCD_SPI_HOST, DRAW_BUFFER_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    assert(buf1);
    void *buf2 = spi_bus_dma_memory_alloc(LCD_SPI_HOST, DRAW_BUFFER_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    assert(buf2);

    // Create display
    lv_disp_t *disp = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
    lv_display_set_buffers(disp, buf1, buf2, DRAW_BUFFER_SIZE, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_user_data(disp, user_data);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);

    lv_display_set_flush_cb(disp, lvgl_flush_cb);

    const esp_lcd_panel_io_callbacks_t trans_done_cb = {
        .on_color_trans_done = lcd_flush_ready_callback,
    };
    /* Register done callback */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_register_event_callbacks(io_handle, &trans_done_cb, disp), TAG, "Failed to register LCD IO event callbacks");

    // Create a timer to handle LVGL ticks
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &lvgl_tick_func,
        .name = "lvgl_tick"};

    esp_timer_handle_t lvgl_tick_timer;
    ESP_RETURN_ON_ERROR(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer), TAG, "Failed to create LVGL tick timer");
    ESP_RETURN_ON_ERROR(esp_timer_start_periodic(lvgl_tick_timer, 20 * 1000), TAG, "Failed to start LVGL tick timer");

    *lv_disp = disp;
    return ESP_OK;
}

esp_err_t lcd_hardware_init(void)
{
    if (s_lcd_hardware.initialized) {
        ESP_LOGW(TAG, "LCD hardware already initialized");
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(init_lcd_panel(&s_lcd_hardware.io_handle, &s_lcd_hardware.panel_handle), TAG, "LCD panel initialization failed");

    ESP_RETURN_ON_ERROR(init_lvgl(s_lcd_hardware.io_handle, s_lcd_hardware.panel_handle, &s_lcd_hardware.lvgl_display), TAG, "LVGL initialization failed");

    s_lcd_hardware.initialized = true;
    ESP_LOGI(TAG, "LCD hardware initialization completed");

    return ESP_OK;
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
        lv_display_delete(s_lcd_hardware.lvgl_display);
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

    gpio_set_level(LCD_PIN_BL, 0);  // Turn off backlight

    s_lcd_hardware.initialized = false;
    ESP_LOGI(TAG, "LCD hardware deinitialized");

    return ESP_OK;
}
