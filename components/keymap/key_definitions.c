#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "esp_err.h"
#include "esp_log.h"
#include "quantum_keycodes.h"
#include "action_code.h"
#include "key_definitions.h"
#include "macros.h"

#define MAX_TOKEN_LEN 12
#define LAYER_MASK 0xf
#define BASIC_CODE_MASK 0xff
#define MOD_MASK 0x1f
#define MOD_BIT_SEP '|'
#define MACRO_NAME_PREFIX "MACRO"

#define TAG "KEY_DEF"

typedef struct {
    uint16_t min_code;
    uint16_t max_code;
    const char* desc;
    int num_args;
    argument_type_e arg_types[MAX_ARG_NUM];
} quantum_funct_desc_t;

typedef struct {
    uint16_t mod_bit;
    const char* desc;
} mod_bit_desc_t;

typedef mod_bit_desc_t macro_desc_t;

const char* basic_key_codes[] = {
    NULL,
    "____",
    NULL,
    NULL,
    "a",
    "b",
    "c",
    "d",
    "e",
    "f",
    "g",
    "h",
    "i",
    "j",
    "k",
    "l",
    "m", /* 0x10 */
    "n",
    "o",
    "p",
    "q",
    "r",
    "s",
    "t",
    "u",
    "v",
    "w",
    "x",
    "y",
    "z",
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
    "<--",
    "TAB",
    "Space",
    "-",
    "=",
    "[",
    "]", /* 0x30 */
    "\\\\", /* \ (and |) */
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
    "Left",
    "Right", /* 0x50 */
    "Down",
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
    NULL, /* from Power to EXSEL are not recognized by Windows and Mac*/
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL, /* 0x70 */
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL, /* 0x80 */
    NULL,
    NULL, /* locking Caps Lock */
    NULL, /* locking Num Lock */
    NULL, /* locking Scroll Lock */
    NULL,
    NULL, /* equal sign on AS/400 */
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL, /* 0x90 */
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL, /* 0xA0 */
    NULL,
    NULL,
    NULL,
    NULL, /* 0xA4 */
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    /* Modifiers */
    "LCtrl", /* 0xE0 */
    "LShift",
    "LAlt",
    "LWin",
    "RCtrl",
    "RShift",
    "RAlt",
    "RWin",
};

const quantum_funct_desc_t quantum_functs[] = {
    {QK_BASIC,        QK_BASIC_MAX,        "BS",      1,     {BASIC_CODE}},
    {QK_MODS,         QK_MODS_MAX,         "MD",      2,     {MOD_BITS, BASIC_CODE}},
    {QK_LAYER_TAP,    QK_LAYER_TAP_MAX,    "LT",      2,     {LAYER_NUM, BASIC_CODE}},
    {QK_TO,           QK_TO_MAX,           "TO",      1,     {LAYER_NUM}},
    {QK_MOMENTARY,    QK_MOMENTARY_MAX,    "MO",      1,     {LAYER_NUM}},
    {QK_DEF_LAYER,    QK_DEF_LAYER_MAX,    "DF",      1,     {LAYER_NUM}},
    {QK_TOGGLE_LAYER, QK_TOGGLE_LAYER_MAX, "TG",      1,     {LAYER_NUM}},
    {QK_MOD_TAP,      QK_MOD_TAP_MAX,      "MT",      2,     {MOD_BITS, BASIC_CODE}},
    {MACRO_CODE_MIN,  MACRO_CODE_MAX,      "MA",      1,     {MACRO_CODE}}
};

const mod_bit_desc_t mod_bit_name[] = {
    {MOD_LCTL, "LCTL"},
    {MOD_LSFT, "LSFT"},
    {MOD_LALT, "LALT"},
    {MOD_LGUI, "LGUI"},
    {MOD_RCTL, "RCTL"},
    {MOD_RSFT, "RSFT"},
    {MOD_RALT, "RALT"},
    {MOD_RGUI, "RGUI"}
};

// Note chr should not be '\0'
static esp_err_t copy_next_token(const char** src, int chr, char* dst, int len)
{
    const char* chr_pos = strchr(*src, chr);
    const char* next_pos;
    if (!chr_pos) {
        chr_pos = *src + strlen(*src);
        next_pos = NULL;
    } else {
        next_pos = chr_pos + 1;
    }

    int tok_len = (int)(chr_pos - *src);
    if (tok_len > len - 1) {
        ESP_LOGE(TAG, "string \"%s\" has too long tokens", *src);
        return ESP_FAIL;
    }

    memcpy(dst, *src, tok_len);
    dst[tok_len] = '\0';

    *src = next_pos;
    return ESP_OK;
}

const char* get_basic_key_name(uint16_t keycode)
{
    if (keycode >= sizeof(basic_key_codes) / sizeof(basic_key_codes[0])) {
        return NULL;
    } else {
        return basic_key_codes[keycode];
    }
}

uint16_t get_max_basic_key_code(void)
{
    return sizeof(basic_key_codes) / sizeof(basic_key_codes[0]);
}

static uint16_t get_basic_key_from_name(const char* name)
{
    if(name == NULL)
        return -1;

    for(uint16_t i = 0; i < get_max_basic_key_code(); ++i){
        if(basic_key_codes[i] && strcmp(name, basic_key_codes[i]) == 0)
            return i;
    }

    return -1;
}

int get_total_funct_num(void)
{
    return sizeof(quantum_functs) / sizeof(quantum_functs[0]);
}

esp_err_t get_funct_desc(int idx, funct_desc_t* desc)
{
    if (idx >= get_total_funct_num()) {
        return ESP_ERR_INVALID_ARG;
    }

    desc->desc = quantum_functs[idx].desc;
    desc->num_args = quantum_functs[idx].num_args;
    memcpy(desc->arg_types, quantum_functs[idx].arg_types, sizeof(desc->arg_types));

    return ESP_OK;
}

int get_funct_arg_num(int idx)
{
    return quantum_functs[idx].num_args;
}

static const quantum_funct_desc_t* get_funct_from_name(const char* name)
{
    for (int i = 0; i < sizeof(quantum_functs) / sizeof(quantum_functs[0]); i++) {
        if (name && strcmp(name, quantum_functs[i].desc) == 0) {
            return &quantum_functs[i];
        }
    }

    return NULL;
}

static const quantum_funct_desc_t* get_funct_from_code(uint16_t key_code)
{
    for (int i = 0; i < sizeof(quantum_functs) / sizeof(quantum_functs[0]); i++) {
        if (key_code >= quantum_functs[i].min_code && key_code <= quantum_functs[i].max_code) {
            return &quantum_functs[i];
        }
    }

    return NULL;
}

int get_mod_bit_num(void)
{
    return sizeof(mod_bit_name) / sizeof(mod_bit_name[0]);
}

const char* get_mod_bit_name(int idx)
{
    return mod_bit_name[idx].desc;
}

static esp_err_t fill_mod_bit_name(uint16_t mod_code, char* buf, int buf_len)
{
    char* output = buf;
    int buf_size = buf_len;
    
    int i;
    if (mod_code >= MOD_RCTL) {
        i = (sizeof(mod_bit_name) / sizeof(mod_bit_name[0])) >> 1;
    } else {
        i = 0;
    }

    bool head = true;
    for (; i < sizeof(mod_bit_name) / sizeof(mod_bit_name[0]); i++) {
        if ((mod_bit_name[i].mod_bit & mod_code) == mod_bit_name[i].mod_bit) {
            int required_size;
            if (head) {
                required_size = snprintf(output, buf_size, " %s", mod_bit_name[i].desc);
                head = false;
            } else {
                required_size = snprintf(output, buf_size, "%c%s", MOD_BIT_SEP, mod_bit_name[i].desc);
            }
            if (required_size + 1 > buf_size) {
                return ESP_ERR_INVALID_SIZE;
            }

            buf_size -= required_size;
            output += required_size;
        }
    }

    return ESP_OK;
}

static uint16_t get_mod_bit_from_name(const char* name)
{
    for (int i = 0; i < sizeof(mod_bit_name) / sizeof(mod_bit_name[0]); i++) {
        if (strcmp(mod_bit_name[i].desc, name) == 0) {
            return mod_bit_name[i].mod_bit;
        }
    }

    return (uint16_t)-1;
}

static esp_err_t parse_mod_bit_name(const char* name, uint16_t* mod_bits)
{
    *mod_bits = 0;

    const char* head = name;
    char scratch[MAX_TOKEN_LEN];
    while (head) {
        esp_err_t err = copy_next_token(&head, MOD_BIT_SEP, scratch, sizeof(scratch));
        if (err != ESP_OK) {
            return ESP_FAIL;
        }

        uint16_t mod_bit = get_mod_bit_from_name(scratch);
        if (mod_bit == (uint16_t)-1) {
            return ESP_FAIL;
        }

        *mod_bits |= mod_bit;
    }

    return ESP_OK;
}

int get_macro_num(void)
{
    return MACRO_CODE_NUM;
}

esp_err_t get_macro_name(int idx, char* buf, int buf_len)
{
    int required_size = snprintf(buf, buf_len, MACRO_NAME_PREFIX "%d", idx);
    if (required_size + 1 > buf_len) {
        return ESP_ERR_INVALID_SIZE;
    } else {
        return ESP_OK;
    }
}

static inline int macro_idx(uint16_t keycode)
{
    return keycode - MACRO_CODE_MIN;
}

esp_err_t parse_macro_name(const char* macro_name, uint16_t* keycode)
{
    int name_len = strlen(macro_name);
    int prefix_len = strlen(MACRO_NAME_PREFIX);
    static int max_idx_len = 0;

    if (max_idx_len == 0) {
        char scratch[8];
        max_idx_len = snprintf(scratch, sizeof(scratch), MACRO_NAME_PREFIX "%d", MACRO_CODE_NUM - 1);
    }

    if (name_len <= prefix_len ||
        name_len > strlen(MACRO_NAME_PREFIX) + max_idx_len) {
        return ESP_ERR_INVALID_ARG;
    }

    if (strncmp(macro_name, MACRO_NAME_PREFIX, prefix_len) != 0) {
        return ESP_ERR_INVALID_ARG;
    }

    long idx = strtol(&macro_name[prefix_len], NULL, 10);
    if (idx < 0 || idx >= MACRO_CODE_NUM) {
        return ESP_ERR_INVALID_ARG;
    }

    *keycode = (uint16_t)(idx + MACRO_CODE_MIN);
    return ESP_OK;
}

/* full key name is in form "funct op1 ..." */
esp_err_t get_full_key_name(uint16_t keycode, char* buf, int buf_len)
{
    char* output = buf;
    int buf_size = buf_len;

    const quantum_funct_desc_t* funct = get_funct_from_code(keycode);
    if (!funct) {
        ESP_LOGE(TAG, "no funct is found for 0x%x", keycode);
        return ESP_FAIL;
    }

    const char* funct_name = funct->desc;
    int layer_index;
    uint16_t basic_code;
    uint16_t mod_code;
    esp_err_t ret;

    int require_size = snprintf(output, buf_size, "%s", funct_name);
    if (require_size + 1 > buf_size) {
        ESP_LOGE(TAG, "buf size(%d) is too small, required(%d)", buf_size, require_size);
        return ESP_ERR_INVALID_SIZE;
    }
    buf_size -= require_size;
    output += require_size;

    for (int i = 0; i < funct->num_args; i++) {
        switch (funct->arg_types[i]) {
        case LAYER_NUM:
            if (funct->num_args == 1) {
                layer_index = keycode & LAYER_MASK;
            } else if (funct->min_code == QK_LAYER_MOD) {
                layer_index = (keycode >> 4) & LAYER_MASK;
            } else {
                layer_index = (keycode >> 8) & LAYER_MASK;
            }
            require_size = snprintf(output, buf_size, " %u", layer_index);
            break;
        case BASIC_CODE:
            basic_code = keycode & BASIC_CODE_MASK;
            const char* key_name = get_basic_key_name(basic_code);
            if (!key_name) {
                return ESP_FAIL;
            }
            require_size = snprintf(output, buf_size, " %s", key_name);
            break;
        case MOD_BITS:
            if (funct->num_args == 1) {
                mod_code = keycode & MOD_MASK;
            } else if (funct->min_code == QK_LAYER_MOD) {
                // special case for LM
                mod_code = (keycode >> 4) & 0xf;
            } else {
                mod_code = (keycode >> 8) & MOD_MASK;
            }
            ret = fill_mod_bit_name(mod_code, output, buf_size);
            if (ret != ESP_OK) {
                return ret;
            }
            require_size = strlen(output);
            break;
        case MACRO_CODE:
            if (buf_size < 1)
                return ESP_ERR_INVALID_SIZE;
            output[0] = ' ';
            output += 1;
            buf_size -= 1;
            ret = get_macro_name(macro_idx(keycode), output, buf_size);
            if (ret != ESP_OK) {
                return ret;
            }
            // Plus one byte for space
            require_size = strlen(output) + 1;
            break;
        default:
            return ESP_FAIL;
        }

        if (require_size + 1 > buf_size) {
            ESP_LOGE(TAG, "left size(%d) is too small, required(%d)", buf_size, require_size);
            return ESP_ERR_INVALID_SIZE;
        }
        buf_size -= require_size;
        output += require_size;        
    }

    return ESP_OK;
}

/* parse full key name in steps:
 * 1. split full key name by space
 * 2. parse funct name
 * 3. parse ops
 */
esp_err_t parse_full_key_name(const char* full_name, uint16_t* keycode)
{
    char scratch[MAX_TOKEN_LEN];
    const char* head = full_name;
    esp_err_t err = copy_next_token(&head, ' ', scratch, sizeof(scratch));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "get funct tok from key name %s, err=%d", head, err);
        return err;
    }

    const quantum_funct_desc_t* funct_desc = get_funct_from_name(scratch);
    if (!funct_desc) {
        ESP_LOGE(TAG, "funct name \"%s\" is unkown", scratch);
        return ESP_FAIL;
    }

    *keycode = 0;
    // QK_MODS is special because mod bits are inferred from the operant
    if (funct_desc->min_code != QK_MODS) {
        if (funct_desc->min_code == QK_TO) {
            *keycode |= funct_desc->min_code | (ON_PRESS << 0x4);
        } else {
            *keycode |= funct_desc->min_code;
        }
    }

    int arg_idx = 0;
    while(head) {
        const char* prev_head = head;
        err = copy_next_token(&head, ' ', scratch, sizeof(scratch));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "get next token from key name %s, err=%d", prev_head, err);
            return err;
        }

        if (arg_idx >= funct_desc->num_args) {
            ESP_LOGE(TAG, "full name %s has more than %d ops", full_name, funct_desc->num_args);
            return ESP_FAIL;
        }

        uint16_t layer_num;
        uint16_t basic_code;
        uint16_t mod_bits;
        switch (funct_desc->arg_types[arg_idx]) {
        case LAYER_NUM:
            layer_num = (uint16_t)strtol(scratch, NULL, 10);
            if (funct_desc->num_args == 1) {
                *keycode |= layer_num & LAYER_MASK;
            } else if (funct_desc->min_code == QK_LAYER_MOD) {
                *keycode |= (layer_num & LAYER_MASK) << 4;
            } else {
                *keycode |= (layer_num & LAYER_MASK) << 8;
            }
            break;
        case BASIC_CODE:
            basic_code = get_basic_key_from_name(scratch);
            if (basic_code == (uint16_t)-1) {
                ESP_LOGE(TAG, "unkown basic name %s", scratch);
                return ESP_FAIL;
            }
            *keycode |= basic_code & BASIC_CODE_MASK;
            break;
        case MOD_BITS:
            err = parse_mod_bit_name(scratch, &mod_bits);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "unkown mod bits %s", scratch);
                return err;
            }
            if (funct_desc->num_args == 1) {
                *keycode |= mod_bits;
            } else if (funct_desc->min_code == QK_LAYER_MOD) {
                *keycode |= (mod_bits & 0xf) << 4;
            } else {
                *keycode |= (mod_bits & MOD_MASK) << 8;
            }
            break;
        case MACRO_CODE:
            err = parse_macro_name(scratch, keycode);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "unkown macro %s", scratch);
                return err;
            }
            break;
        default:
            ESP_LOGE(TAG, "unkown token %s or wrong position", scratch);
            return ESP_FAIL;
        }
        
        arg_idx++;
    }

    return ESP_OK;
}
