#include "host.h"
#include "keyboard_report.h"
#include "keyboard_config.h"
#include "hal_ble.h"
#include "keycode_conv.h"

static void send_keyboard_to_queue(report_keyboard_t*);
static uint8_t keyboard_leds_status(void);
static host_driver_t keyboard_driver = {.send_keyboard = send_keyboard_to_queue, .keyboard_leds = keyboard_leds_status};

int register_keyboard_reporter(void)
{
    host_set_driver(&keyboard_driver);
    return 0;
}

static uint8_t keyboard_leds_status(void)
{
    return 0;
}

static void send_keyboard_to_queue(report_keyboard_t *report)
{
    uint8_t report_state[REPORT_LEN] = {0};

    //Do not send anything if queues are uninitialized
    if (mouse_q == NULL || keyboard_q == NULL || joystick_q == NULL) {
        ESP_LOGE("report", "queues not initialized");
        return;
    }

#ifdef NKRO_ENABLE
    //Check if the report was modified, if so send it
    report_state[0] = report->nkro.mods;
    report_state[1] = 0;

    int index = 0;
    for (uint16_t i = 0; index < REPORT_COUNT_BYTES && i < KEYBOARD_REPORT_BITS; i++) {
        if (report->nkro.bits[i]) {
            for (uint16_t j = 0; j < 8 && ((1 << j) & report->nkro.bits[i]); j++) {
                report_state[index + MOD_LED_BYTES] = (i << 3) + j;
                index++;
            }
        }
    }
#else
    report_state[0] = report->mods;
    report_state[1] = report->reserved;

    int index = 0;
    for (uint16_t i = 0; i < REPORT_COUNT_BYTES && i < KEYBOARD_REPORT_KEYS; i++) {
        if (report->keys[i]) {
            report_state[index + MOD_LED_BYTES] = report->keys[i];
            index++;
        }
    }
#endif

    if (BLE_EN == 1) {
        xQueueSend(keyboard_q, report_state, (TickType_t) 0);
    }
}
