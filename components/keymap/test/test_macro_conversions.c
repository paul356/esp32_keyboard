/*
 *  Unit tests for macro conversion functions in MK32 keyboard firmware.
 *
 *  Copyright (C) 2024 panhao356@gmail.com
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "esp_err.h"
#include "macros.h"
#include "send_string_keycodes.h"
#include "keycode.h"

#define TEST_BUFFER_SIZE 1024

// Test basic user-friendly to QMK conversion
TEST_CASE("test_convert_user_friendly_to_qmk_basic", "keymap") {
    char output[TEST_BUFFER_SIZE];
    esp_err_t result;

    // Test simple text
    result = convert_user_friendly_to_qmk("hello", output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING("hello", output);

    // Test empty string
    result = convert_user_friendly_to_qmk("", output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING("", output);

    // Test NULL input
    result = convert_user_friendly_to_qmk(NULL, output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

    // Test NULL output
    result = convert_user_friendly_to_qmk("test", NULL, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

    // Test zero output size
    result = convert_user_friendly_to_qmk("test", output, 0);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

// Test special key conversions
TEST_CASE("test_convert_user_friendly_to_qmk_special_keys", "keymap") {
    char output[TEST_BUFFER_SIZE];
    esp_err_t result;

    // Test backspace
    result = convert_user_friendly_to_qmk("\\b", output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING(SS_TAP(X_BSPACE), output);

    // Test delete
    result = convert_user_friendly_to_qmk("\\d", output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING(SS_TAP(X_DELETE), output);

    // Test escape
    result = convert_user_friendly_to_qmk("\\e", output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING(SS_TAP(X_ESCAPE), output);

    // Test escaped backslash
    result = convert_user_friendly_to_qmk("\\\\", output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING("\\\\", output);

    // Test escaped parenthesis
    result = convert_user_friendly_to_qmk("\\)", output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING("\\)", output);
}

// Test modifier key conversions
TEST_CASE("test_convert_user_friendly_to_qmk_modifiers", "keymap") {
    char output[TEST_BUFFER_SIZE];
    esp_err_t result;

    // Test left shift
    result = convert_user_friendly_to_qmk("\\lshift(A)", output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING(SS_DOWN(X_LSFT) "A" SS_UP(X_LSFT), output);

    // Test left control
    result = convert_user_friendly_to_qmk("\\lctrl(c)", output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING(SS_DOWN(X_LCTRL) "c" SS_UP(X_LCTRL), output);

    // Test left alt
    result = convert_user_friendly_to_qmk("\\lalt(tab)", output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING(SS_DOWN(X_LALT) "tab" SS_UP(X_LALT), output);

    // Test left GUI (windows key)
    result = convert_user_friendly_to_qmk("\\lgui(r)", output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING(SS_DOWN(X_LGUI) "r" SS_UP(X_LGUI), output);
}

// Test right modifiers
TEST_CASE("test_convert_user_friendly_to_qmk_right_modifiers", "keymap") {
    char output[TEST_BUFFER_SIZE];
    esp_err_t result;

    // Test right shift
    result = convert_user_friendly_to_qmk("\\rshift(B)", output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING(SS_DOWN(X_RSFT) "B" SS_UP(X_RSFT), output);

    // Test right control
    result = convert_user_friendly_to_qmk("\\rctrl(v)", output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING(SS_DOWN(X_RCTRL) "v" SS_UP(X_RCTRL), output);

    // Test right alt
    result = convert_user_friendly_to_qmk("\\ralt(f4)", output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING(SS_DOWN(X_RALT) "f4" SS_UP(X_RALT), output);

    // Test right GUI
    result = convert_user_friendly_to_qmk("\\rgui(l)", output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING(SS_DOWN(X_RGUI) "l" SS_UP(X_RGUI), output);
}

// Test nested modifiers
TEST_CASE("test_convert_user_friendly_to_qmk_nested_modifiers", "keymap") {
    char output[TEST_BUFFER_SIZE];
    esp_err_t result;

    // Test nested modifiers: Ctrl+Shift+A
    result = convert_user_friendly_to_qmk("\\lctrl(\\lshift(A))", output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING(SS_DOWN(X_LCTRL) SS_DOWN(X_LSFT) "A" SS_UP(X_LSFT) SS_UP(X_LCTRL), output);

    // Test triple nested: Ctrl+Alt+Shift+T
    result = convert_user_friendly_to_qmk("\\lctrl(\\lalt(\\lshift(T)))", output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING(SS_DOWN(X_LCTRL) SS_DOWN(X_LALT) SS_DOWN(X_LSFT) "T" SS_UP(X_LSFT) SS_UP(X_LALT) SS_UP(X_LCTRL), output);
}

// Test complex mixed content
TEST_CASE("test_convert_user_friendly_to_qmk_mixed_content", "keymap") {
    char output[TEST_BUFFER_SIZE];
    esp_err_t result;

    // Test mixed text with modifiers and special keys
    result = convert_user_friendly_to_qmk("Hello \\lshift(World)\\b", output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING("Hello " SS_DOWN(X_LSFT) "World" SS_UP(X_LSFT) SS_TAP(X_BSPACE), output);

    // Test modifier with special key inside
    result = convert_user_friendly_to_qmk("\\lctrl(\\e)", output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING(SS_DOWN(X_LCTRL) SS_TAP(X_ESCAPE) SS_UP(X_LCTRL), output);
}

// Test error conditions
TEST_CASE("test_convert_user_friendly_to_qmk_errors", "keymap") {
    char output[TEST_BUFFER_SIZE];
    esp_err_t result;

    // Test unmatched opening parenthesis
    result = convert_user_friendly_to_qmk("\\lshift(test", output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

    // Test buffer too small
    result = convert_user_friendly_to_qmk("This is a very long string", output, 5);
    TEST_ASSERT_EQUAL(ESP_ERR_NO_MEM, result);

    // Test modifier nesting too deep (beyond MAX_MODIFIER_DEPTH)
    result = convert_user_friendly_to_qmk("\\lshift(\\lctrl(\\lalt(\\lgui(\\rshift(\\rctrl(\\ralt(\\rgui(\\lshift(test)))))))))", output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

// Test QMK to user-friendly conversion basic cases
TEST_CASE("test_convert_qmk_to_user_friendly_basic", "keymap") {
    char output[TEST_BUFFER_SIZE];
    esp_err_t result;

    // Test simple text
    result = convert_qmk_to_user_friendly("hello", output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING("hello", output);

    // Test empty string
    result = convert_qmk_to_user_friendly("", output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING("", output);

    // Test NULL input
    result = convert_qmk_to_user_friendly(NULL, output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

    // Test NULL output
    result = convert_qmk_to_user_friendly("test", NULL, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

// Test QMK to user-friendly special keys
TEST_CASE("test_convert_qmk_to_user_friendly_special_keys", "keymap") {
    char output[TEST_BUFFER_SIZE];
    esp_err_t result;

    // Test backspace
    result = convert_qmk_to_user_friendly(SS_TAP(X_BSPACE), output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING("\\b", output);

    // Test delete
    result = convert_qmk_to_user_friendly(SS_TAP(X_DELETE), output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING("\\d", output);

    // Test escape
    result = convert_qmk_to_user_friendly(SS_TAP(X_ESCAPE), output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING("\\e", output);

    // Test backslash
    result = convert_qmk_to_user_friendly("\\", output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING("\\", output);
}

// Test QMK to user-friendly modifiers
TEST_CASE("test_convert_qmk_to_user_friendly_modifiers", "keymap") {
    char output[TEST_BUFFER_SIZE];
    esp_err_t result;

    // Test left shift
    result = convert_qmk_to_user_friendly(SS_DOWN(X_LSFT) "A" SS_UP(X_LSFT), output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING("\\lshift(A)", output);

    // Test left control
    result = convert_qmk_to_user_friendly(SS_DOWN(X_LCTRL) "c" SS_UP(X_LCTRL), output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING("\\lctrl(c)", output);

    // Test left alt
    result = convert_qmk_to_user_friendly(SS_DOWN(X_LALT) "tab" SS_UP(X_LALT), output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING("\\lalt(tab)", output);

    // Test left GUI
    result = convert_qmk_to_user_friendly(SS_DOWN(X_LGUI) "r" SS_UP(X_LGUI), output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING("\\lgui(r)", output);
}

// Test QMK to user-friendly nested modifiers
TEST_CASE("test_convert_qmk_to_user_friendly_nested_modifiers", "keymap") {
    char output[TEST_BUFFER_SIZE];
    esp_err_t result;

    // Test nested modifiers: Ctrl+Shift+A
    result = convert_qmk_to_user_friendly(SS_DOWN(X_LCTRL) SS_DOWN(X_LSFT) "A" SS_UP(X_LSFT) SS_UP(X_LCTRL), output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING("\\lctrl(\\lshift(A))", output);
}

// Test QMK to user-friendly special parentheses handling
TEST_CASE("test_convert_qmk_to_user_friendly_parentheses", "keymap") {
    char output[TEST_BUFFER_SIZE];
    esp_err_t result;

    // Test parenthesis outside modifier (should not be escaped)
    result = convert_qmk_to_user_friendly("text)", output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING("text)", output);

    // Test parenthesis inside modifier (should be escaped)
    result = convert_qmk_to_user_friendly(SS_DOWN(X_LSFT) "text)" SS_UP(X_LSFT), output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING("\\lshift(text\\))", output);
}

// Test roundtrip conversion (user-friendly -> QMK -> user-friendly)
TEST_CASE("test_conversion_roundtrip", "keymap") {
    char qmk_output[TEST_BUFFER_SIZE];
    char user_output[TEST_BUFFER_SIZE];
    esp_err_t result;

    const char* test_cases[] = {
        "hello world",
        "\\lshift(Test)",
        "\\lctrl(\\lalt(delete))",
        "Text\\bwith\\dspecial\\ekeys",
        "\\\\and\\)escaped\\(chars",
        "\\lshift(A)normal\\rctrl(B)"
    };

    for (int i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        // Convert to QMK format
        result = convert_user_friendly_to_qmk(test_cases[i], qmk_output, sizeof(qmk_output));
        TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, result, "Forward conversion failed");

        // Convert back to user-friendly format
        result = convert_qmk_to_user_friendly(qmk_output, user_output, sizeof(user_output));
        TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, result, "Backward conversion failed");

        // Should match original
        TEST_ASSERT_EQUAL_STRING_MESSAGE(test_cases[i], user_output, "Roundtrip conversion failed");
    }
}

// Test buffer size edge cases
TEST_CASE("test_buffer_size_edge_cases", "keymap") {
    char output[10]; // Small buffer
    esp_err_t result;

    // Test input that exactly fits
    result = convert_user_friendly_to_qmk("123456789", output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING("123456789", output);

    // Test input that's too long
    result = convert_user_friendly_to_qmk("1234567890", output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_ERR_NO_MEM, result);

    // Test QMK conversion that expands beyond buffer
    result = convert_user_friendly_to_qmk("\\b", output, 3); // SS_TAP(X_BSPACE) is 4 chars including '\0'
    TEST_ASSERT_EQUAL(ESP_ERR_NO_MEM, result);
}

// Test error handling for malformed QMK input
TEST_CASE("test_convert_qmk_to_user_friendly_errors", "keymap") {
    char output[TEST_BUFFER_SIZE];
    esp_err_t result;

    // Test unmatched modifier UP code
    char malformed_qmk[] = {SS_QMK_PREFIX, SS_UP_CODE, KC_LSFT, '\0'};
    result = convert_qmk_to_user_friendly(malformed_qmk, output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);

    // Test buffer too small for user-friendly output
    result = convert_qmk_to_user_friendly(SS_DOWN(X_LSFT) "test" SS_UP(X_LSFT), output, 5);
    TEST_ASSERT_EQUAL(ESP_ERR_NO_MEM, result);

    // Test buffer too small for user-friendly output
    result = convert_qmk_to_user_friendly("test", output, 4);
    TEST_ASSERT_EQUAL(ESP_ERR_NO_MEM, result);
}

TEST_CASE("test_invalid_input_characters", "keymap") {
    char output[TEST_BUFFER_SIZE];
    esp_err_t result;

    result = convert_user_friendly_to_qmk("test\x7f\x01\x1f" "end", output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING("test...end", output);

    result = convert_user_friendly_to_qmk("\\lshift(\x7f\x01\x1ftest)", output, sizeof(output));
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL_STRING(SS_DOWN(X_LSFT) "...test" SS_UP(X_LSFT), output);
}
