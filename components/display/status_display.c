#include "status_display.h"
#include "config.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "driver/i2c.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "key_definitions.h"
#include "function_control.h"

#define DISPLAY_PIN_I2C_SDA GPIO_NUM_35
#define DISPLAY_PIN_I2C_SCL GPIO_NUM_36
#define LCD_PIXEL_CLOCK_HZ  40000
#define DISPLAY_I2C_HOST    0
#define DISPLAY_SLAVE_ADDR  0x3c
#define DISPLAY_CMD_BITS    8
#define DISPLAY_PARAM_BITS  8
#define DISPLAY_DC_BIT_OFFSET 6
#define DISPLAY_LCD_H_RES   128
#define DISPLAY_LCD_V_RES   64

#define TAG "[DISPLAY]"

enum DISPLAY_OBJ_E {
    ICON_BLE,
    ICON_USB,
    ICON_WIFI,
    LINE_FRAME,
    LINE_HORI_BAR,
    LABEL_KEY,
    DISPLAY_OBJ_MAX
};

static esp_lcd_panel_io_handle_t s_disp_panel_io_handle;
static esp_lcd_panel_handle_t s_disp_panel_handle;
static lv_disp_t* s_disp;
static lv_obj_t* s_objects[DISPLAY_OBJ_MAX];

static esp_err_t init_i2c_bus(void)
{
    esp_err_t ret;
    ESP_LOGI(TAG, "Initialize I2C bus");

    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = DISPLAY_PIN_I2C_SDA,
        .scl_io_num = DISPLAY_PIN_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = LCD_PIXEL_CLOCK_HZ,
    };
    ret = i2c_param_config(DISPLAY_I2C_HOST, &i2c_conf);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = i2c_driver_install(DISPLAY_I2C_HOST, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK) {
        return ret;
    }

    return ESP_OK;
}

static esp_err_t create_esp_lcd_panel(void)
{
    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr = DISPLAY_SLAVE_ADDR,
        .control_phase_bytes = 1,
        .lcd_cmd_bits = DISPLAY_CMD_BITS,
        .lcd_param_bits = DISPLAY_PARAM_BITS,
        .dc_bit_offset = DISPLAY_DC_BIT_OFFSET,
    };
    esp_err_t ret = esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)DISPLAY_I2C_HOST, &io_config, &io_handle);
    if (ret != ESP_OK) {
        return ret;
    }

    ESP_LOGI(TAG, "Install SSD1306 panel driver");
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = -1,
        .color_space = ESP_LCD_COLOR_SPACE_MONOCHROME,
        .bits_per_pixel = 1,
    };
    ret = esp_lcd_new_panel_ssd1306(io_handle, &panel_config, &panel_handle);
    if (ret != ESP_OK) {
        esp_lcd_panel_io_del(io_handle);
        return ret;
    }

    ret = esp_lcd_panel_reset(panel_handle);
    if (ret != ESP_OK) {
        esp_lcd_panel_del(panel_handle);
        esp_lcd_panel_io_del(io_handle);
        return ret;
    }
    ret = esp_lcd_panel_init(panel_handle);
    if (ret != ESP_OK) {
        esp_lcd_panel_del(panel_handle);
        esp_lcd_panel_io_del(io_handle);
        return ret;
    }
    ret = esp_lcd_panel_disp_on_off(panel_handle, true);
    if (ret != ESP_OK) {
        esp_lcd_panel_del(panel_handle);
        esp_lcd_panel_io_del(io_handle);
        return ret;
    }
    
    /* mirror the screen for ssd1312 */
    esp_lcd_panel_mirror(panel_handle, true, false);

    s_disp_panel_io_handle = io_handle;
    s_disp_panel_handle = panel_handle;
    return ESP_OK;    
}

static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_disp_t * disp = (lv_disp_t *)user_ctx;
    lvgl_port_flush_ready(disp);
    return false;
}

static esp_err_t init_lvgl_port(void)
{
    ESP_LOGI(TAG, "Initialize LVGL");
    lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_cfg.task_priority = configMAX_PRIORITIES - 1;
    lvgl_cfg.task_affinity = 1;
    lvgl_port_init(&lvgl_cfg);

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = s_disp_panel_io_handle,
        .panel_handle = s_disp_panel_handle,
        .buffer_size = DISPLAY_LCD_H_RES * DISPLAY_LCD_V_RES,
        .double_buffer = true,
        .hres = DISPLAY_LCD_H_RES,
        .vres = DISPLAY_LCD_V_RES,
        .monochrome = true,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        }
    };
    s_disp = lvgl_port_add_disp(&disp_cfg);
    /* Register done callback for IO */
    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = notify_lvgl_flush_ready,
    };
    esp_lcd_panel_io_register_event_callbacks(s_disp_panel_io_handle, &cbs, s_disp);

    return ESP_OK;
}

esp_err_t init_display(void)
{
    esp_err_t ret = init_i2c_bus();
    if (ret != ESP_OK) {
        return ret;
    }

    ret = create_esp_lcd_panel();
    if (ret != ESP_OK) {
        return ret;
    }

    (void)init_lvgl_port();

    return ESP_OK;
}

static void create_objects(void)
{
    static lv_point_t frame_pts[] = {{0, 0}, {0, 63}, {127, 63}, {127, 0}, {0, 0}};
    static lv_point_t hori_pts[] = {{0, 32}, {127, 32}};

    lv_obj_t* scr = lv_disp_get_scr_act(s_disp);

    if (is_usb_hid_enabled()) {
        lv_obj_t* usb_img = lv_img_create(scr);
        LV_IMG_DECLARE(usb_icon);
        lv_img_set_src(usb_img, &usb_icon);
        lv_obj_align(usb_img, LV_ALIGN_TOP_LEFT, 8, 0);
        s_objects[ICON_USB] = usb_img;
    }

    if (is_ble_hid_enabled()) {
        lv_obj_t* ble_img = lv_img_create(scr);
        LV_IMG_DECLARE(bluetooth_icon);
        lv_img_set_src(ble_img, &bluetooth_icon);
        lv_obj_align(ble_img, LV_ALIGN_TOP_LEFT, 48+8, 0);
        s_objects[ICON_BLE] = ble_img;
    }

    if (is_wifi_enabled()) {
        lv_obj_t* wifi_img = lv_img_create(scr);
        LV_IMG_DECLARE(wifi_icon);
        lv_img_set_src(wifi_img, &wifi_icon);
        lv_obj_align(wifi_img, LV_ALIGN_TOP_LEFT, 96, 0);
        s_objects[ICON_WIFI] = wifi_img;
    }

    lv_obj_t* frame_line = lv_line_create(scr);
    lv_line_set_points(frame_line, frame_pts, 5);
    lv_obj_align(frame_line, LV_ALIGN_TOP_LEFT, 0, 0);
    s_objects[LINE_FRAME] = frame_line;

    lv_obj_t* hori_line = lv_line_create(scr);
    lv_line_set_points(hori_line, hori_pts, 2);
    lv_obj_align(frame_line, LV_ALIGN_TOP_LEFT, 0, 0);
    s_objects[LINE_HORI_BAR] = hori_line;

    lv_obj_t* key_label = lv_label_create(scr);
    lv_label_set_long_mode(key_label, LV_LABEL_LONG_DOT);
    lv_label_set_text(key_label, "____");
    lv_obj_align(key_label, LV_ALIGN_TOP_LEFT, 32, 40);
    s_objects[LABEL_KEY] = key_label;
}

void update_display(uint16_t last_key)
{
    if (s_disp) {
        if (!s_objects[LINE_FRAME]) {
            create_objects();
        } else if (last_key) {
            ESP_LOGI(TAG, "update LVGL label %hu", last_key);
            lv_label_set_text(s_objects[LABEL_KEY], GetKeyCodeName(last_key));
        }
    }        
}
