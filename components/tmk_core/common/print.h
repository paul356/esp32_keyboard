/* Copyright 2012 Jun Wako <wakojun@gmail.com> */
/* Very basic print functions, intended to be used with usb_debug_only.c
 * http://www.pjrc.com/teensy/
 * Copyright (c) 2008 PJRC.COM, LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef PRINT_H__
#define PRINT_H__ 1

#include <stdint.h>
#include <stdbool.h>
#include "util.h"

#if defined(PROTOCOL_CHIBIOS) || defined(PROTOCOL_ARM_ATSAM)
#    define PSTR(x) x
#elif defined(__ESP32S3__)
#define PSTR(x) x
#endif

#ifndef NO_PRINT

#    if defined(__AVR__) /* __AVR__ */

#        include "avr/xprintf.h"

#        ifdef USER_PRINT /* USER_PRINT */

// Remove normal print defines
#            define print(s)
#            define println(s)
#            undef xprintf
#            define xprintf(fmt, ...)

// Create user print defines
#            define uprint(s) xputs(PSTR(s))
#            define uprintln(s) xputs(PSTR(s "\r\n"))
#            define uprintf(fmt, ...) __xprintf(PSTR(fmt), ##__VA_ARGS__)

#        else /* NORMAL PRINT */

// Create user & normal print defines
#            define print(s) xputs(PSTR(s))
#            define println(s) xputs(PSTR(s "\r\n"))
#            define uprint(s) print(s)
#            define uprintln(s) println(s)
#            define uprintf(fmt, ...) xprintf(fmt, ##__VA_ARGS__)

#        endif /* USER_PRINT / NORMAL PRINT */

#        ifdef __cplusplus
extern "C"
#        endif

    /* function pointer of sendchar to be used by print utility */
    void print_set_sendchar(int8_t (*print_sendchar_func)(uint8_t));

#    elif defined(PROTOCOL_CHIBIOS) /* PROTOCOL_CHIBIOS */

#        include "printf.h"  // lib/printf/printf.h

#        ifdef USER_PRINT /* USER_PRINT */

// Remove normal print defines
#            define print(s)
#            define println(s)
#            define xprintf(fmt, ...)

// Create user print defines
#            define uprint(s) printf(s)
#            define uprintln(s) printf(s "\r\n")
#            define uprintf printf

#        else /* NORMAL PRINT */
// Create user & normal print defines
#            define print(s) printf(s)
#            define println(s) printf(s "\r\n")
#            define xprintf printf
#            define uprint(s) printf(s)
#            define uprintln(s) printf(s "\r\n")
#            define uprintf printf

#        endif /* USER_PRINT / NORMAL PRINT */

#elif defined(__ESP32S3__)
#include "esp32s3/print.h"
#ifdef USER_PRINT
#            define print(s)
#            define println(s)
#            define xprintf(fmt, ...)

// Create user print defines
#            define uprint(s) tinyusb_cdc_print(s)
#            define uprintln(s) tinyusb_cdc_print(s "\r\n")
#            define uprintf tinyusb_cdc_vprintf
#else
#            define print(s) tinyusb_cdc_print(s)
#            define println(s) tinyusb_cdc_print(s "\r\n")
#            define xprintf tinyusb_cdc_vprintf
#            define uprint(s) tinyusb_cdc_print(s)
#            define uprintln(s) tinyusb_cdc_print(s "\r\n")
#            define uprintf tinyusb_cdc_vprintf
#endif

#    elif defined(PROTOCOL_ARM_ATSAM) /* PROTOCOL_ARM_ATSAM */

#        include "arm_atsam/printf.h"

#        ifdef USER_PRINT /* USER_PRINT */

// Remove normal print defines
#            define print(s)
#            define println(s)
#            define xprintf(fmt, ...)

// Create user print defines
#            define uprintf(fmt, ...) __xprintf(fmt, ##__VA_ARGS__)
#            define uprint(s) xprintf(s)
#            define uprintln(s) xprintf(s "\r\n")

#        else /* NORMAL PRINT */

// Create user & normal print defines
#            define xprintf(fmt, ...) __xprintf(fmt, ##__VA_ARGS__)
#            define print(s) xprintf(s)
#            define println(s) xprintf(s "\r\n")
#            define uprint(s) print(s)
#            define uprintln(s) println(s)
#            define uprintf(fmt, ...) xprintf(fmt, ##__VA_ARGS__)

#        endif /* USER_PRINT / NORMAL PRINT */

#    endif /* __AVR__ / PROTOCOL_CHIBIOS / PROTOCOL_ARM_ATSAM */

// User print disables the normal print messages in the body of QMK/TMK code and
// is meant as a lightweight alternative to NOPRINT. Use it when you only want to do
// a spot of debugging but lack flash resources for allowing all of the codebase to
// print (and store their wasteful strings).
//
// !!! DO NOT USE USER PRINT CALLS IN THE BODY OF QMK/TMK !!!
//
#    ifdef USER_PRINT

// Disable normal print
#        define print_dec(data)
#        define print_decs(data)
#        define print_hex4(data)
#        define print_hex8(data)
#        define print_hex16(data)
#        define print_hex32(data)
#        define print_bin4(data)
#        define print_bin8(data)
#        define print_bin16(data)
#        define print_bin32(data)
#        define print_bin_reverse8(data)
#        define print_bin_reverse16(data)
#        define print_bin_reverse32(data)
#        define print_val_dec(v)
#        define print_val_decs(v)
#        define print_val_hex8(v)
#        define print_val_hex16(v)
#        define print_val_hex32(v)
#        define print_val_bin8(v)
#        define print_val_bin16(v)
#        define print_val_bin32(v)
#        define print_val_bin_reverse8(v)
#        define print_val_bin_reverse16(v)
#        define print_val_bin_reverse32(v)

#    else /* NORMAL_PRINT */

// Enable normal print
/* decimal */
#        define print_dec(i) xprintf("%u", i)
#        define print_decs(i) xprintf("%d", i)
/* hex */
#        define print_hex4(i) xprintf("%X", i)
#        define print_hex8(i) xprintf("%02X", i)
#        define print_hex16(i) xprintf("%04X", i)
#        define print_hex32(i) xprintf("%08lX", i)
/* binary */
#        define print_bin4(i) xprintf("%s", bit4_rep[(i) & BIT4MASK])
#        define print_bin8(i) xprintf("%s%s", bit4_rep[((i) >> 4) & BIT4MASK], bit4_rep[(i) & BIT4MASK])
#        define print_bin16(i) xprintf("%s%s%s%s", bit4_rep[((i) >> 12) & BIT4MASK], bit4_rep[((i) >> 8) & BIT4MASK], bit4_rep[((i) >> 4) & BIT4MASK], bit4_rep[(i) & BIT4MASK])
#        define print_bin32(i) xprintf("%s%s%s%s%s%s%s%s", bit4_rep[((i) >> 28) & BIT4MASK], bit4_rep[((i) >> 24) & BIT4MASK], bit4_rep[((i) >> 20) & BIT4MASK], bit4_rep[((i) >> 16) & BIT4MASK], bit4_rep[((i) >> 12) & BIT4MASK], bit4_rep[((i) >> 8) & BIT4MASK], bit4_rep[((i) >> 4) & BIT4MASK], bit4_rep[(i) & BIT4MASK])
#        define print_bin_reverse8(i) print_bin8(bitrev(i))
#        define print_bin_reverse16(i) print_bin16(bitrev16(i))
#        define print_bin_reverse32(i) print_bin32(bitrev32(i))
/* print value utility */
#        define print_val_dec(v) xprintf(#        v ": %u\n", v)
#        define print_val_decs(v) xprintf(#        v ": %d\n", v)
#        define print_val_hex8(v) xprintf(#        v ": %X\n", v)
#        define print_val_hex16(v) xprintf(#        v ": %02X\n", v)
#        define print_val_hex32(v) xprintf(#        v ": %04lX\n", v)
#        define print_val_bin8(v) xprintf(#        v ": %s%s\n", bit4_rep[((v) >> 4) & BIT4MASK], bit4_rep[(v) & BIT4MASK])
#        define print_val_bin16(i) xprintf(#        i ": %s%s%s%s\n", bit4_rep[((i) >> 12) & BIT4MASK], bit4_rep[((i) >> 8) & BIT4MASK], bit4_rep[((i) >> 4) & BIT4MASK], bit4_rep[(i) & BIT4MASK])
#        define print_val_bin32(i) xprintf(#        i ": %s%s%s%s%s%s%s%s\n", bit4_rep[((i) >> 28) & BIT4MASK], bit4_rep[((i) >> 24) & BIT4MASK], bit4_rep[((i) >> 20) & BIT4MASK], bit4_rep[((i) >> 16) & BIT4MASK], bit4_rep[((i) >> 12) & BIT4MASK], bit4_rep[((i) >> 8) & BIT4MASK], bit4_rep[((i) >> 4) & BIT4MASK], bit4_rep[(i) & BIT4MASK])
#        define print_val_bin_reverse8(v) xprintf(#        v ": %s%s\n", bit4_rep[(bitrev(v) >> 4) & BIT4MASK], bit4_rep[bitrev(v) & BIT4MASK])
#        define print_val_bin_reverse16(v) xprintf(#        v ": %s%s%s%s\n", bit4_rep[(bitrev16(v) >> 12) & BIT4MASK], bit4_rep[(bitrev16(v) >> 8) & BIT4MASK], bit4_rep[(bitrev16(v) >> 4) & BIT4MASK], bit4_rep[bitrev16(v) & BIT4MASK])
#        define print_val_bin_reverse32(v) xprintf(#        v ": %s%s%s%s%s%s%s%s\n", bit4_rep[(bitrev32(v) >> 28) & BIT4MASK], bit4_rep[(bitrev32(v) >> 24) & BIT4MASK], bit4_rep[(bitrev32(v) >> 20) & BIT4MASK], bit4_rep[(bitrev32(v) >> 16) & BIT4MASK], bit4_rep[(bitrev32(v) >> 12) & BIT4MASK], bit4_rep[(bitrev32(v) >> 8) & BIT4MASK], bit4_rep[(bitrev32(v) >> 4) & BIT4MASK], bit4_rep[bitrev32(v) & BIT4MASK])

#    endif /* USER_PRINT / NORMAL_PRINT */

// User Print

/* decimal */
#    define uprint_dec(i) uprintf("%u", i)
#    define uprint_decs(i) uprintf("%d", i)
/* hex */
#    define uprint_hex4(i) uprintf("%X", i)
#    define uprint_hex8(i) uprintf("%02X", i)
#    define uprint_hex16(i) uprintf("%04X", i)
#    define uprint_hex32(i) uprintf("%08lX", i)
/* binary */
#    define uprint_bin4(i) uprintf("%04b", i)
#    define uprint_bin8(i) uprintf("%08b", i)
#    define uprint_bin16(i) uprintf("%016b", i)
#    define uprint_bin32(i) uprintf("%032lb", i)
#    define uprint_bin_reverse8(i) uprintf("%08b", bitrev(i))
#    define uprint_bin_reverse16(i) uprintf("%016b", bitrev16(i))
#    define uprint_bin_reverse32(i) uprintf("%032lb", bitrev32(i))
/* print value utility */
#    define uprint_val_dec(v) uprintf(#    v ": %u\n", v)
#    define uprint_val_decs(v) uprintf(#    v ": %d\n", v)
#    define uprint_val_hex8(v) uprintf(#    v ": %X\n", v)
#    define uprint_val_hex16(v) uprintf(#    v ": %02X\n", v)
#    define uprint_val_hex32(v) uprintf(#    v ": %04lX\n", v)
#    define uprint_val_bin8(v) uprintf(#    v ": %08b\n", v)
#    define uprint_val_bin16(v) uprintf(#    v ": %016b\n", v)
#    define uprint_val_bin32(v) uprintf(#    v ": %032lb\n", v)
#    define uprint_val_bin_reverse8(v) uprintf(#    v ": %08b\n", bitrev(v))
#    define uprint_val_bin_reverse16(v) uprintf(#    v ": %016b\n", bitrev16(v))
#    define uprint_val_bin_reverse32(v) uprintf(#    v ": %032lb\n", bitrev32(v))

#else /* NO_PRINT */

#    define xprintf(fmt, ...)
#    define print(s)
#    define println(s)
#    define print_set_sendchar(func)
#    define print_dec(data)
#    define print_decs(data)
#    define print_hex4(data)
#    define print_hex8(data)
#    define print_hex16(data)
#    define print_hex32(data)
#    define print_bin4(data)
#    define print_bin8(data)
#    define print_bin16(data)
#    define print_bin32(data)
#    define print_bin_reverse8(data)
#    define print_bin_reverse16(data)
#    define print_bin_reverse32(data)
#    define print_val_dec(v)
#    define print_val_decs(v)
#    define print_val_hex8(v)
#    define print_val_hex16(v)
#    define print_val_hex32(v)
#    define print_val_bin8(v)
#    define print_val_bin16(v)
#    define print_val_bin32(v)
#    define print_val_bin_reverse8(v)
#    define print_val_bin_reverse16(v)
#    define print_val_bin_reverse32(v)

#endif /* NO_PRINT */

/* Backward compatiblitly for old name */
#define pdec(data) print_dec(data)
#define pdec16(data) print_dec(data)
#define phex(data) print_hex8(data)
#define phex16(data) print_hex16(data)
#define pbin(data) print_bin8(data)
#define pbin16(data) print_bin16(data)
#define pbin_reverse(data) print_bin_reverse8(data)
#define pbin_reverse16(data) print_bin_reverse16(data)

#endif
