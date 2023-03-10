#ifndef KEYBOARD_CONFIG_H
#define KEYBOARD_CONFIG_H

/*
 *---------------------------- Everything below here should not be modified for standard usage----------------------
 *
 * */
#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))
#define SET_BIT(var,pos) (var |= 1LLU << pos);

#define MOD_LED_BYTES 2 //bytes for led status and modifiers
#define REPORT_LEN (MOD_LED_BYTES+6) //size of hid reports with NKRO and room for 3 key macro
#define REPORT_COUNT_BYTES (6)

#define LAYERS 3
#define MAX_LAYOUT_NAME_LENGTH 15

#endif
//
