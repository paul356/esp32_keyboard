# A Smart Keyboard Based on ESP32-S3 with an Android Config APP

### Introduction
ESP32 series chips are popular IoT chips manufactured by [Espressif Inc.](https://www.espressif.com). These chips feature many easy-to-use interfaces like WiFi, Bluetooth, USB, and UART, making them suitable candidates for keyboards. This project is inspired by [MK32](https://github.com/Galzai/MK32) and [qmk_firmware](https://github.com/qmk/qmk_firmware). It started as a fork of MK32, then ported `tmk_core` and `quantum` components from QMK and rewrote many parts.

The keyboard is a 60-key design built around an ESP32-S3 module, featuring a built-in LCD screen, RGB LEDs, and a companion Android App for dynamic keymap and macro configuration.

![img](https://paul356.github.io/images/keyboard_outlook.jpg)

Merge requests are welcome. If you have advice or a feature request, please open a ticket.

### Features

#### Hardware
- 60-key layout with 3-layer mapping, covering all 104 standard keys; full NKRO support
- Kailh Box switches and SDA-style keycaps
- LCD screen for displaying keyboard status and accessing settings
- ESP32-S3 module with BLE, WiFi, and USB connectivity
- RGB LED effects; 3.7 V NTC battery connector

#### Software
- Android App for viewing and dynamically modifying keymap layouts per layer
- Android App macro editor: define up to 32 macros with special key syntax
- BLE multi-device support with quick input-target switching (iOS not supported)
- OTA firmware update via WiFi and Web interface
- Web server for keyboard status and keymap management
- Function keys: `info`, `intro`, `hotspot`, `toggle_nkro`, `clear_bonds`, `toggle_broadcast`
- Open-source firmware with deep customization support

### LCD Interface

The keyboard has an LCD in the upper-right corner. By default it shows the **input info screen** (key count, caps-lock, WiFi/BLE status, power). Rotating the knob activates the settings menu:

```
├── Input Info
│   ├── Input Info Screen (default)
│   └── Reset Key Counter
├── BLE Settings
│   ├── Enable / Disable BLE
│   ├── BLE Pairing Screen
│   └── Switch Input Target
├── WiFi Settings
│   ├── Enable / Disable WiFi
│   └── WiFi Configuration
└── LED Settings
    ├── Enable / Disable LED
    └── LED Mode Selection
```

Navigate the menu by rotating the knob (left/right) to move between items, pressing **Enter** to select, and pressing **Esc** to go back. The keyboard captures all key input while the menu is active; press **Esc** repeatedly or wait 5 seconds to return to the input info screen.

### Android App

The Android App connects to the keyboard over BLE (the keyboard must be **paired but not connected** in Android Bluetooth settings before launching the App).

- **Keymap tab** — Displays all layers. Long-press **Sync** to scan for keyboards; short-press **Sync** to fetch or push the keymap. The **+** button saves/restores keymap snapshots. Tap a key to open the keycode editor dialog.
- **Macros tab** — Define up to 32 macros. Macros are plain text with optional escape sequences (see [Keycode Types / MA](#keycode-types) below).

### Keycode Types

| Type | Description |
|------|-------------|
| **BS** | Basic keycode — one of the standard 104-key keycodes |
| **MD** | Modifier + basic key combination (e.g. Shift+A) |
| **LT** | Layer-tap: hold to activate a layer, tap to send a basic keycode |
| **TO** | Permanently activate a layer |
| **MO** | Momentarily activate a layer while held (default for Super/Fn keys) |
| **DF** | Set the default (always-active) layer |
| **TG** | Toggle a layer on/off |
| **MT** | Modifier-tap: hold for a modifier key (Ctrl/Shift/Alt/GUI), tap for a basic keycode |
| **MA** | Macro key — triggers one of 32 user-defined macros |
| **FT** | Function key — triggers a built-in keyboard function |

#### Macro Escape Syntax

Macros can contain special characters. Users can put ALT, SHFIT, CTRL, ENTER, ESC, .etc in the macros through escape syntax.

| Sequence | Meaning |
|----------|---------|
| `\d` | Delete key |
| `\b` | Backspace key |
| `\t` | Tab key |
| `\e` | Esc key |
| `\)` | Literal `)` inside a modifier group |
| `\lshift(...)` | Hold Left Shift while sending `...` |
| `\rshift(...)` | Hold Right Shift while sending `...` |
| `\lctrl(...)` | Hold Left Ctrl while sending `...` |
| `\rctrl(...)` | Hold Right Ctrl while sending `...` |
| `\lalt(...)` | Hold Left Alt while sending `...` |
| `\ralt(...)` | Hold Right Alt while sending `...` |
| `\lgui(...)` | Hold Left GUI while sending `...` |
| `\rgui(...)` | Hold Right GUI while sending `...` |

Sequences can be nested, e.g. `\lctrl(\lalt(\d))` for Ctrl+Alt+Delete.

### Build Process
It builds with esp-idf 5.4. In order to support multiple device connection I have modified the esp-idf. The PR is sent but didn't move forward. The changed esp-idf can be downloaded from this [branch]](https://github.com/paul356/esp-idf/tree/v5.4-with-multi-hidd-conn). If you have esp-idf set up run following commands to build the firmware. Note `get_idf` is the bash alias for entering esp-idf environment.

```
git clone https://github.com/paul356/esp32_keyboard.git
get_idf
idf.py set-target esp32s3
idf.py build
idf.py flash # Please hold BOOT low to put ESP32S3 in download mode
```

In `build` directory you will find the bin file `esp32_keyboard.bin`. Flash this firmware to your ESP32S3 development board. Be aware I repurpose the TX0/RX0 ports of ESP32S3, UART0 won't output debug logs by default.

### License
tmk_core and quantum are licensed under GPL-2.0 or latter. The new code is licensed under GPL-3.0 and latter. Please refer to each file for adopted licenses.
