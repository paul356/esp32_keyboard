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
#if CONFIG_TINYUSB_CDC_ENABLED
#include "tusb_cdc_acm.h"
#endif
#include "debug.h"

extern "C" {
//HID Ble functions
//#include "HID_kbdmousejoystick.h"
#include "hal_ble.h"

//MK32 functions
#include "matrix.h"
#include "keyboard_config.h"
#include "nvs_funcs.h"
#include "nvs_keymaps.h"
#include "esp32s3/keyboard_report.h"
#include "action_layer.h"
#include "wait.h"

extern esp_err_t start_file_server();
extern void wifi_init_softap(void);
extern void rtc_matrix_deinit(void);
}

#define KEY_REPORT_TAG "KEY_REPORT"
#define SYSTEM_REPORT_TAG "KEY_REPORT"
#define TRUNC_SIZE 20
#define USEC_TO_SEC 1000000
#define SEC_TO_MIN 60
#define TAG "main"

bool DEEP_SLEEP = true; // flag to check if we need to go to deep sleep

TaskHandle_t xKeyreportTask;

//How to handle key reports
static void key_reports(void *pvParameters)
{
    while (1) {
        keyboard_task();
        wait_ms(5);
    }
}

static void send_keys(void *pParam)
{
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = configTICK_RATE_HZ;

    xLastWakeTime = xTaskGetTickCount();
    bool state = true;
    while (1) {
        uint8_t keycode[] = {0, 0, 0, 0, 0, 0, 0, 0};

        (void)xTaskDelayUntil(&xLastWakeTime, xFrequency);

        // check interface readiness
        //if (tud_hid_ready())
        //    tud_hid_keyboard_report(1, 0, keycode);

        ESP_LOGI (TAG, "send_keys func runs %u", xLastWakeTime);

        if (state) {
            keycode[3] = 4;
        } else {
            keycode[3] = 0;
        }
        state = !state;
        xQueueSend(keyboard_q, keycode, 0);
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

#if CONFIG_TINYUSB_CDC_ENABLED
void tinyusb_cdc_rx_callback(int itf, cdcacm_event_t *event)
{
    /* initialization */
    size_t rx_size = 0;
    uint8_t buf[65];

    tinyusb_cdcacm_itf_t cdc_acm_itf = TINYUSB_CDC_ACM_0;
    /* read */
    esp_err_t ret = tinyusb_cdcacm_read(cdc_acm_itf, buf, CONFIG_TINYUSB_CDC_RX_BUFSIZE, &rx_size);
    if (ret == ESP_OK) {
        buf[rx_size] = '\0';
        ESP_LOGI(TAG, "Got data (%d bytes): %s", rx_size, buf);
    } else {
        ESP_LOGE(TAG, "Read error");
    }

    /* write back */
    tinyusb_cdcacm_write_queue(cdc_acm_itf, buf, rx_size);
    tinyusb_cdcacm_write_flush(cdc_acm_itf, 0);
}

void tinyusb_cdc_line_state_changed_callback(int itf, cdcacm_event_t *event)
{
    int dtr = event->line_state_changed_data.dtr;
    int rst = event->line_state_changed_data.rts;
    ESP_LOGI(TAG, "Line state changed! dtr:%d, rst:%d", dtr, rst);
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

#if CONFIG_TINYUSB_CDC_ENABLED
    tinyusb_config_cdcacm_t amc_cfg = {
        .usb_dev = TINYUSB_USBDEV_0,
        .cdc_port = TINYUSB_CDC_ACM_0,
        .rx_unread_buf_sz = 64,
        .callback_rx = &tinyusb_cdc_rx_callback, // the first way to register a callback
        .callback_rx_wanted_char = NULL,
        .callback_line_state_changed = &tinyusb_cdc_line_state_changed_callback,
        .callback_line_coding_changed = NULL
    };

    ESP_ERROR_CHECK(tusb_cdc_acm_init(&amc_cfg));
#endif

    debug_enable = false;
    debug_matrix = false;
    debug_keyboard = false;
}

extern "C" void app_main()
{
    esp_err_t ret;
    esp_log_level_set("*", ESP_LOG_INFO);
    //Underclocking for better current draw (not really effective)
    //    esp_pm_config_esp32_t pm_config;
    //    pm_config.max_freq_mhz = 10;
    //    pm_config.min_freq_mhz = 10;
    //    esp_pm_configure(&pm_config);
    
    // set pin to gpio mode in case
    matrix_setup();

    matrix_init();
    default_layer_set(0x1 << 0);

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

    (void)register_keyboard_reporter();
    enable_usb_hid();
    //xTaskCreatePinnedToCore(send_keys, "period send key", 1024, NULL, configMAX_PRIORITIES, NULL, 1);

    ESP_ERROR_CHECK(start_file_server());

    ESP_LOGI("MAIN", "MAIN finished...");
}
