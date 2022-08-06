#include "esp_system.h"

void bootloader_jump() {
    esp_restart();
}
