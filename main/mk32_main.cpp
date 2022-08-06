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
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "driver/touch_pad.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "esp_pm.h"
#include "tinyusb.h"

extern "C" {
//HID Ble functions
//#include "HID_kbdmousejoystick.h"
#include "hal_ble.h"

//MK32 functions
#include "matrix.h"
#include "keyboard_config.h"
#include "nvs_funcs.h"
#include "nvs_keymaps.h"
#include "keycode_conv.h"

extern esp_err_t start_file_server();
extern void wifi_init_softap(void);
}

#define KEY_REPORT_TAG "KEY_REPORT"
#define SYSTEM_REPORT_TAG "KEY_REPORT"
#define TRUNC_SIZE 20
#define USEC_TO_SEC 1000000
#define SEC_TO_MIN 60

static config_data_t config;

bool DEEP_SLEEP = true; // flag to check if we need to go to deep sleep

TaskHandle_t xKeyreportTask;

//How to handle key reports
static void key_reports(void *pvParameters)
{
    uint8_t report_state[REPORT_LEN];

    while (1) {
        keyboard_task();

        //Do not send anything if queues are uninitialized
        if (mouse_q == NULL || keyboard_q == NULL || joystick_q == NULL) {
            ESP_LOGE(KEY_REPORT_TAG, "queues not initialized");
            continue;
        }

        //Check if the report was modified, if so send it
        report_state[0] = 0;
        report_state[1] = 0;

        if(BLE_EN == 1){
            xQueueSend(keyboard_q, report_state, (TickType_t) 0);
        }
        if(input_str_q != NULL){
            xQueueSend(input_str_q, report_state, (TickType_t) 0);
        }
    }
}

static void send_keys(void *pParam)
{
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = configTICK_RATE_HZ * 15;

    xLastWakeTime = xTaskGetTickCount();
    while (1) {
        uint8_t keycode[6] = {0x4, 0, 0, 0, 0, 0};

        (void)xTaskDelayUntil(&xLastWakeTime, xFrequency);

        // check interface readiness
        if (tud_hid_ready())
            tud_hid_keyboard_report(1, 0, keycode);
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

static void enable_usb_hid(void)
{
    tinyusb_config_t tusb_cfg = {
        .descriptor = NULL,
        .string_descriptor = NULL,
        .external_phy = false
    };

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));    
}

extern "C" void app_main()
{
    esp_err_t ret;
    esp_log_level_set("*", ESP_LOG_INFO);

    //Reset the rtc GPIOS
    matrix_init();

    //Underclocking for better current draw (not really effective)
    //    esp_pm_config_esp32_t pm_config;
    //    pm_config.max_freq_mhz = 10;
    //    pm_config.min_freq_mhz = 10;
    //    esp_pm_configure(&pm_config);
    matrix_setup();

    // Initialize NVS.
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK (nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_softap();

    //Loading layouts from nvs (if found)
    nvs_load_layouts();
    //activate keyboard BT stack
    halBLEInit(1, 1, 1, 0);
    ESP_LOGI("BLE", "initialized");

    // Start the keyboard Tasks
    // Create the key scanning task on core 1 (otherwise it will crash)
    BLE_EN = 1;
    xTaskCreatePinnedToCore(key_reports, "key report task", 8192,
                            xKeyreportTask, configMAX_PRIORITIES, NULL, 1);
    ESP_LOGI("Keyboard task", "initialized");

#ifdef SLEEP_MINS
    xTaskCreatePinnedToCore(deep_sleep, "deep sleep task", 4096, NULL,
                            configMAX_PRIORITIES, NULL, 1);
    ESP_LOGI("Sleep", "initialized");
#endif

    enable_usb_hid();
    //xTaskCreatePinnedToCore(send_keys, "period send key", 1024, NULL, configMAX_PRIORITIES, NULL, 1);

    ESP_ERROR_CHECK(start_file_server());

    ESP_LOGI("MAIN", "MAIN finished...");
}
