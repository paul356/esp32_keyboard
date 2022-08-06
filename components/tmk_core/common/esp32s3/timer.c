#include "esp_timer.h"

static uint64_t reset_point = 0;

void timer_init(void) {}

void timer_clear(void) {
    reset_point = esp_timer_get_time();
}

uint16_t timer_read(void) {
    uint64_t curr_time = esp_timer_get_time() >> 10; // for speed
    return (curr_time >= (1ULL << 16)) ? ~(uint16_t)0 : (uint16_t)curr_time;
}

uint32_t timer_read32(void) {
    uint64_t curr_time = esp_timer_get_time() >> 10; // for speed
    return (curr_time >= (1ULL << 32)) ? ~(uint32_t)0 : (uint32_t)curr_time;
}

uint16_t timer_elapsed(uint16_t last) {
    uint64_t curr_time = esp_timer_get_time();
    uint64_t time_diff = (curr_time - reset_point) >> 10; // right shift 10 bit for speed

    return (time_diff >= (1ULL << 16)) ? ~(uint16_t)0 : (uint16_t)time_diff;
}

uint32_t timer_elapsed32(uint32_t last) {
    uint64_t curr_time = esp_timer_get_time();
    uint64_t time_diff = (curr_time - reset_point) >> 10; // right shift 10 bit for speed

    return (time_diff >= (1ULL << 32)) ? ~(uint32_t)0 : (uint32_t)time_diff;
}
