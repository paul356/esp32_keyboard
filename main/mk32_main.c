/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 * Copyright 2018 Gal Zaidenstein.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "driver/touch_pad.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "esp_pm.h"
#include "tusb.h"
#include "matrix_config.h"
#include "function_control.h"
#include "keyboard_gui.h"
#include "hid_desc.h"

//MK32 functions
#include "matrix.h"
#include "layout_store.h"
#include "esp32s3/keyboard_report.h"
#include "action_layer.h"
#include "wait.h"
#include "host.h"
#include "miscs.h"
#include "memory_debug.h"
#include "drv_loop.h"
#include "led_ctrl.h"
#include "idle_detection.h"

extern esp_err_t start_file_server();
extern void wifi_init_softap(void);
extern void rtc_matrix_deinit(void);

#define KEY_REPORT_TAG "KEY_REPORT"
#define SYSTEM_REPORT_TAG "KEY_REPORT"
#define TRUNC_SIZE 20
#define USEC_TO_SEC 1000000
#define SEC_TO_MIN 60
#define TAG "main"
#define KEYBOARD_TASK_PERIOD 5000 // 5ms

bool DEEP_SLEEP = true; // flag to check if we need to go to deep sleep

/*If no key press has been recieved in SLEEP_MINS amount of minutes, put device into deep sleep
 *  wake up on touch on GPIO pin 2
 *  */
#ifdef SLEEP_MINS
static void deep_sleep(void *pvParameters) {
    uint64_t initial_time = esp_timer_get_time(); // notice that timer returns time passed in microseconds!
    uint64_t current_time_passed = 0;
    while (1) {

        current_time_passed = (esp_timer_get_time() - initial_time);

        if (DEEP_SLEEP == false) {
            current_time_passed = 0;
            initial_time = esp_timer_get_time();
            DEEP_SLEEP = true;
        }

        if (((double)current_time_passed/USEC_TO_SEC) >= (double)  (SEC_TO_MIN * SLEEP_MINS)) {
            if (DEEP_SLEEP == true) {
                ESP_LOGE(SYSTEM_REPORT_TAG, "going to sleep!");
#ifdef OLED_ENABLE
                vTaskDelay(20 / portTICK_PERIOD_MS);
                vTaskSuspend(xOledTask);
                deinit_oled();
#endif
                // wake up esp32 using rtc gpio
                rtc_matrix_setup();
                esp_sleep_enable_touchpad_wakeup();
                esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
                esp_deep_sleep_start();

            }
            if (DEEP_SLEEP == false) {
                current_time_passed = 0;
                initial_time = esp_timer_get_time();
                DEEP_SLEEP = true;
            }
        }

    }
}
#endif

static int32_t encoder_last_pos;
static bool pos_inited = false;
//How to handle key reports
static void detect_user_actions(void)
{
    keyboard_task();

    // Set the initial encoder position
    if (!pos_inited) {
        encoder_last_pos = miscs_encoder_get_position();
        pos_inited = true;
    }

    int32_t curr_pos = miscs_encoder_get_position();
    if (curr_pos != encoder_last_pos) {
        encoder_last_pos = curr_pos;
        miscs_encoder_direction_t encoder_direct = miscs_encoder_get_direction();
        if (encoder_direct == MISCS_ENCODER_CW) {
            keyboard_gui_post_input_event_isr(INPUT_EVENT_ENCODER_CW, 0);
        } else if (encoder_direct == MISCS_ENCODER_CCW) {
            keyboard_gui_post_input_event_isr(INPUT_EVENT_ENCODER_CCW, 0);
        }

        idle_detection_reset();
    }
}

// for test only
void test_miscs(void)
{
    bool usb_powered = miscs_is_usb_powered();
    ESP_LOGI(TAG, "USB Powered: %s", usb_powered ? "Yes" : "No");
    bool charging = miscs_is_battery_charging();
    ESP_LOGI(TAG, "Battery Charging: %s", charging ? "Yes" : "No");
    uint8_t battery_percentage = 0;
    esp_err_t ret = miscs_get_battery_percentage(&battery_percentage, false);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Battery Percentage: %u%%", battery_percentage);
    } else {
        ESP_LOGE(TAG, "Failed to get battery percentage: %s", esp_err_to_name(ret));
    }
    uint32_t encoder_pos = miscs_encoder_get_position();
    ESP_LOGI(TAG, "Encoder Position: %ld", encoder_pos);
    miscs_encoder_direction_t encoder_direct = miscs_encoder_get_direction();
    switch (encoder_direct) {
        case MISCS_ENCODER_CW:
            ESP_LOGI(TAG, "Encoder Direction: Clockwise");
            break;
        case MISCS_ENCODER_CCW:
            ESP_LOGI(TAG, "Encoder Direction: Counter-Clockwise");
            break;
        case MISCS_ENCODER_STOPPED:
            ESP_LOGI(TAG, "Encoder Direction: Stopped");
            break;
    }
}

void app_main()
{
    esp_err_t ret;

    // Log initial memory state
    log_memory_usage("INITIAL STATE");

    // Configure Dynamic Frequency Scaling for power optimization
    // This allows the CPU to scale between 40MHz (idle) and 160MHz (active)
    // with automatic light sleep during idle periods
    esp_pm_config_t pm_config = {
        .max_freq_mhz = 160,  // Full speed when needed
        .min_freq_mhz = 40,   // Scale down during idle (good balance for keyboard responsiveness)
        .light_sleep_enable = true  // Enable automatic light sleep
    };
    ret = esp_pm_configure(&pm_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Dynamic Frequency Scaling enabled: 40-160 MHz with light sleep");
    } else {
        ESP_LOGW(TAG, "Failed to configure power management: %s", esp_err_to_name(ret));
    }
    log_memory_usage("After PM configuration");

    ESP_ERROR_CHECK(drv_loop_init());

    // Initialize NVS.
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    log_memory_usage("After NVS flash init");

    (void)register_keyboard_reporter();
    log_memory_usage("After keyboard reporter registration");

    enable_usb_hid();
    log_memory_usage("After USB HID enable");

    // Initialize miscs
    ret = miscs_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to initialize miscs: %s", esp_err_to_name(ret));
    }
    log_memory_usage("After miscs_init");

    ret = led_ctrl_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to initialize led ctrl: %s", esp_err_to_name(ret));
    }
    log_memory_usage("After led_ctrl_init");

    ESP_ERROR_CHECK(keyboard_gui_init());
    log_memory_usage("After keyboard_gui_init");

    matrix_setup();
    log_memory_usage("After matrix_setup");

    matrix_init();
    log_memory_usage("After matrix_init");

    default_layer_set(0x1);
    log_memory_usage("After default_layer_set");

    // Load layouts from nvs (if found)
    nvs_load_layouts();
    log_memory_usage("After nvs_load_layouts");

    ESP_ERROR_CHECK(restore_saved_state());
    log_memory_usage("After restore_saved_state");

    if (is_wifi_enabled())
    {
        ESP_ERROR_CHECK(start_file_server());
        log_memory_usage("After start_file_server");
    }

    // Initialize idle detection system
    idle_detection_init();

    log_memory_usage("Keyboard initialization complete");

    uint32_t count = 0;
    while (1) {
        detect_user_actions();

        if (((count++) % 1000) == 0) {
            // Process power management based on idle state
            pwr_mgmt_process();
            uint8_t percentage;
            miscs_get_battery_percentage(&percentage, true);
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
