#ifndef KEY_DEFINITIONS_H
#define KEY_DEFINITIONS_H

#include <stdint.h>

const char* GetKeyCodeName(uint16_t keycode);
uint16_t GetKeyCodeNum();
int GetKeyCodeWithName(const char* name);

#endif
