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
#include "esp_system.h"
#include "esp_wifi.h"
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
#include "port_mgmt.h"
#include "function_control.h"
#include "status_display.h"
#include "keyboard_gui.h"
#include "hid_desc.h"

//MK32 functions
#include "matrix.h"
#include "keyboard_config.h"
#include "layout_store.h"
#include "esp32s3/keyboard_report.h"
#include "action_layer.h"
#include "wait.h"
#include "host.h"
#include "miscs.h"
#include "memory_debug.h"
#include "drv_loop.h"

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

static void send_keys(void *pParam)
{
    static uint8_t keys[] = {4, 5, 6, 7, 8, 9};
    static unsigned int count = 0;

    TickType_t xLastWakeTime;
    const TickType_t xFrequency = configTICK_RATE_HZ;

    xLastWakeTime = xTaskGetTickCount();
    bool state = true;
    while (1) {
        (void)xTaskDelayUntil(&xLastWakeTime, xFrequency);

        // check interface readiness
        //if (tud_hid_ready())
        //    tud_hid_keyboard_report(1, 0, keycode);

        ESP_LOGI (TAG, "send_keys func runs %lu", xLastWakeTime);

        report_keyboard_t report = {0};
        report.mods = 0;
        report.reserved = 0;
        if (state) {
            report.keys[0] = keys[count % 6];
            count++;
        } else {
            report.keys[3] = 0;
        }
        state = !state;

        host_get_driver()->send_keyboard(&report);
    }
}

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

//How to handle key reports
static void keyboard_timer_func(void *pvParameters)
{
    keyboard_task();
}

void start_keyboard_timer()
{
    esp_timer_create_args_t timer_args = {&keyboard_timer_func, NULL, ESP_TIMER_TASK, "kb_task", true};
    esp_timer_handle_t timer_handle = NULL;

    esp_err_t ret = esp_timer_create(&timer_args, &timer_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Create esp timer failed, ret=%d", ret);
        return;
    }

    ret = esp_timer_start_periodic(timer_handle, KEYBOARD_TASK_PERIOD);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Start peroidic timer failed, ret=%d", ret);
        return;
    }
}

// for test only
void test_miscs(void)
{
    bool usb_powered = miscs_is_usb_powered();
    ESP_LOGI(TAG, "USB Powered: %s", usb_powered ? "Yes" : "No");
    bool charging = miscs_is_battery_charging();
    ESP_LOGI(TAG, "Battery Charging: %s", charging ? "Yes" : "No");
    uint32_t voltage_mv = 0;
    esp_err_t ret = miscs_read_battery_voltage(&voltage_mv);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Battery Voltage: %lu mV", voltage_mv);
    } else {
        ESP_LOGE(TAG, "Failed to read battery voltage: %s", esp_err_to_name(ret));
    }
    uint32_t encoder_pos = miscs_encoder_get_position();
    ESP_LOGI(TAG, "Encoder Position: %ld", encoder_pos);
    miscs_encoder_direction_t encoder_direct = miscs_encoder_get_direction();
    switch (encoder_direct) {
        case MISCS_ENCODER_CW:
            ESP_LOGI(TAG, "Encoder Direction: Clockwise");
            display_handle_encoder_rotation(1);  // Positive direction for clockwise
            break;
        case MISCS_ENCODER_CCW:
            ESP_LOGI(TAG, "Encoder Direction: Counter-Clockwise");
            display_handle_encoder_rotation(-1); // Negative direction for counter-clockwise
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

    //Underclocking for better current draw (not really effective)
    //    esp_pm_config_esp32_t pm_config;
    //    pm_config.max_freq_mhz = 10;
    //    pm_config.min_freq_mhz = 10;
    //    esp_pm_configure(&pm_config);

    ESP_ERROR_CHECK(drv_loop_init());

    (void)register_keyboard_reporter();
    log_memory_usage("After keyboard reporter registration");

    enable_usb_hid();
    log_memory_usage("After USB HID enable");

    // Initialize miscs
    ret = miscs_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize miscs: %s", esp_err_to_name(ret));
        return;
    }
    log_memory_usage("After miscs_init");

    ESP_ERROR_CHECK(init_display());
    log_memory_usage("After init_display");

    bool keyboard_inited = false;
    while (true) {
        if (/*tud_ready() && */!keyboard_inited) {
            vTaskDelay(500 / portTICK_PERIOD_MS);

            matrix_setup();
            log_memory_usage("After matrix_setup");

            matrix_init();
            log_memory_usage("After matrix_init");

            default_layer_set(0x1);
            log_memory_usage("After default_layer_set");

            // Initialize NVS.
            ret = nvs_flash_init();
            if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
                ESP_ERROR_CHECK (nvs_flash_erase());
                ret = nvs_flash_init();
            }
            ESP_ERROR_CHECK(ret);
            log_memory_usage("After NVS flash init");

            // Load layouts from nvs (if found)
            nvs_load_layouts();
            log_memory_usage("After nvs_load_layouts");

            start_keyboard_timer();
            log_memory_usage("After start_keyboard_timer");

            ESP_ERROR_CHECK(restore_saved_state());
            log_memory_usage("After restore_saved_state");

            ESP_ERROR_CHECK(start_file_server());
            log_memory_usage("After start_file_server");

            (void)update_display(0);

            //xTaskCreatePinnedToCore(send_keys, "period send key", 4096, NULL, configMAX_PRIORITIES, NULL, 1);
            keyboard_inited = true;
            log_memory_usage("Keyboard initialization complete");
        }

        // Update GUI for menu handling and LVGL tasks
        keyboard_gui_update();

        vTaskDelay(500 / portTICK_PERIOD_MS);  // Reduced delay for better GUI responsiveness
    }
}
