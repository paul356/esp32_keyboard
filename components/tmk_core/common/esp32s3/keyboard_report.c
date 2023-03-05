#include "host.h"
#include "keyboard_report.h"
#include "keyboard_config.h"
#include "wait.h"
#include "tinyusb.h"
#include "esp_log.h"
#include "hal_ble.h"

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

#ifdef NKRO_ENABLE
    //Check if the report was modified, if so send it
    report_state[0] = report->nkro.mods;
    report_state[1] = 0;

    int index = 0;
    for (uint16_t i = 0; i < REPORT_COUNT_BYTES && index < KEYBOARD_REPORT_BITS; i++) {
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
    for (uint16_t i = 0; i < REPORT_COUNT_BYTES && index < KEYBOARD_REPORT_KEYS; i++) {
        if (report->keys[i]) {
            report_state[index + MOD_LED_BYTES] = report->keys[i];
            index++;
        }
    }
#endif

    // hardcoded HID interface 0, keyboard id 1
    if (tud_ready()) {
        // endpoint may be busy
        while (!tud_hid_n_ready(0)) {
            wait_ms(1);
        }
        tud_hid_n_keyboard_report(0, 1, report_state[0], &report_state[2]);
    }

    if (keyboard_q) {
        xQueueSend(keyboard_q, report_state, (TickType_t) 0);
    }
}
