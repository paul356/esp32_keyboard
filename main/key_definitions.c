#include <stddef.h>
#include "key_definitions.h"

const char* key_code_name[] = {
	"",
	"",
	"",
	"___",
	"A",
	"B",
	"C",
	"D",
	"E",
	"F",
	"G",
	"H",
	"I",
	"J",
	"K",
	"L",
	"M", /* 0x10 */
	"N",
	"O",
	"P",
	"Q",
	"R",
	"S",
	"T",
	"U",
	"V",
	"W",
	"X",
	"Y",
	"Z",
	"1",
	"2",
	"3", /* 0x20 */
	"4",
	"5",
	"6",
	"7",
	"8",
	"9",
	"0",
	"Ent",
	"Esc",
	"Backspace",
	"TAB",
	"Space",
	"-",
	"=",
	"[",
	"]", /* 0x30 */
	"\\", /* \ (and |) */
	"#(EU)", /* Non-US # and ~ (Typically near the Enter key) */
	";", /* ; (and :) */
	"'", /* ' and " */
	"`", /* Grave accent and tilde */
	",", /* ", and < */
	".", /* . and > */
	"/", /* / and ? */
	"Caps",
	"F1",
	"F2",
	"F3",
	"F4",
	"F5",
	"F6",
	"F7", /* 0x40 */
	"F8",
	"F9",
	"F10",
	"F11",
	"F12",
	"PrtSc",
	"ScrLk",
	"Pause",
	"Ins",
	"Home",
	"PgUp",
	"Del",
	"End",
	"PgDn",
	"<-",
	"->", /* 0x50 */
	"Dn",
	"Up",
	"Num",
	"KP_/",
	"KP_*",
	"KP_-",
	"KP_+",
	"KP_Ent",
	"KP_1",
	"KP_2",
	"KP_3",
	"KP_4",
	"KP_5",
	"KP_6",
	"KP_7",
	"KP_8", /* 0x60 */
	"KP_9",
	"KP_0",
	"KP_.",
	"\(EU)", /* Non-US \ and | (Typically near the Left-Shift key) */
	"App",
	"", /* from Power to EXSEL are not recognized by Windows and Mac*/
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"", /* 0x70 */
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"", /* 0x80 */
	"",
	"", /* locking Caps Lock */
	"", /* locking Num Lock */
	"", /* locking Scroll Lock */
	"",
	"", /* equal sign on AS/400 */
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"", /* 0x90 */
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"", /* 0xA0 */
	"",
	"",
	"",
	"", /* 0xA4 */
    "","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","",
	/* Modifiers */
	"LCtrl",
	"LShift",
	"LAlt",
	"LWin",
	"RCtrl",
	"RShift",
	"RAlt",
	"RWin",

    /* NOTE: 0xE8-FF are used for internal special purpose */
    "","","","","","","","","","","","","","","","","","","","","","","","",

    /* 0x100 to 0x102, move default layer */
    "Top",
    "Lower",
    "Raise",

    /* 0x103 to 0x122 */
    "","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","",

    /* 0x123 to 0x134 */
    "HQwerty",
    "HNum",
    "HPlugin",
    "","","","","","","","","","","","","","","",
};

const char* GetKeyCodeName(uint16_t keycode)
{
    if (keycode >= sizeof(key_code_name) / sizeof(key_code_name[0])) {
        return NULL;
    } else {
        return key_code_name[keycode];
    }
}

uint16_t GetKeyCodeNum() {
    return sizeof(key_code_name) / sizeof(key_code_name[0]);
}
