#include "esp_timer.h"

void timer_init(void) {}

uint16_t timer_read(void) {
    uint64_t curr_time = (esp_timer_get_time() >> 10) & 0xffff; // for speed
    return (uint16_t)curr_time;
}

uint32_t timer_read32(void) {
    uint64_t curr_time = (esp_timer_get_time() >> 10) & 0xffffffff; // for speed
    return (uint32_t)curr_time;
}

uint16_t timer_elapsed(uint16_t last) {
    uint16_t curr_time = timer_read();
    uint16_t time_diff = curr_time - last;

    return (curr_time < last) ? ~(uint16_t)0 : time_diff;
}

uint32_t timer_elapsed32(uint32_t last) {
    uint32_t curr_time = timer_read32();
    uint32_t time_diff = curr_time - last;

    return (curr_time < last) ? ~(uint32_t)0 : time_diff;
}
