#include "host.h"
#include "keyboard_report.h"


static void send_keyboard_to_queue(report_keyboard_t*);
static host_driver_t keyboard_driver = {.send_keyboard = send_keyboard_to_queue};

int register_keyboard_reporter(void)
{
    host_set_driver(&keyboard_driver);
    return 0;
}

static void send_keyboard_to_queue(report_keyboard_t *report)
{
    uint8_t report_state[REPORT_LEN];

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
