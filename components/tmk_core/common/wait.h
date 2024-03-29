#ifndef WAIT_H
#define WAIT_H

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__AVR__)
#    include <util/delay.h>
#    define wait_ms(ms) _delay_ms(ms)
#    define wait_us(us) _delay_us(us)
#elif defined PROTOCOL_CHIBIOS
#    include "ch.h"
#    define wait_ms(ms)                     \
        do {                                \
            if (ms != 0) {                  \
                chThdSleepMilliseconds(ms); \
            } else {                        \
                chThdSleepMicroseconds(1);  \
            }                               \
        } while (0)
#    define wait_us(us)                     \
        do {                                \
            if (us != 0) {                  \
                chThdSleepMicroseconds(us); \
            } else {                        \
                chThdSleepMicroseconds(1);  \
            }                               \
        } while (0)
#elif defined PROTOCOL_ARM_ATSAM
#    include "clks.h"
#    define wait_ms(ms) CLK_delay_ms(ms)
#    define wait_us(us) CLK_delay_us(us)
#elif defined __ESP32S3__
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#define NOP() asm volatile ("nop")
static inline void delayMicroseconds(uint32_t us)
{
    uint64_t m = (uint64_t)esp_timer_get_time();
    if(us){
        uint64_t e = (m + us);
        if(m > e){ //overflow
            while((uint64_t)esp_timer_get_time() > e){
                NOP();
            }
        }
        while((uint64_t)esp_timer_get_time() < e){
            NOP();
        }
    }
}           
#define wait_ms(ms) vTaskDelay((ms) / portTICK_PERIOD_MS)
#define wait_us(us) delayMicroseconds(us)
#else  // Unit tests
void wait_ms(uint32_t ms);
#    define wait_us(us) wait_ms(us / 1000)
#endif

#ifdef __cplusplus
}
#endif

#endif
