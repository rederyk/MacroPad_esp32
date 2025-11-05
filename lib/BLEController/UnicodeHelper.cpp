/*
 * ESP32 MacroPad Project - Unicode Helper Implementation
 * Copyright (C) [2025] [Enrico Mori]
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "UnicodeHelper.h"
#include "Logger.h"
#include <BleCombo.h>
#include <BleComboKeyboard.h>

// Numpad key codes (for Windows Alt codes)
#define KEY_KP_0 0x62
#define KEY_KP_1 0x59
#define KEY_KP_2 0x5A
#define KEY_KP_3 0x5B
#define KEY_KP_4 0x5C
#define KEY_KP_5 0x5D
#define KEY_KP_6 0x5E
#define KEY_KP_7 0x5F
#define KEY_KP_8 0x60
#define KEY_KP_9 0x61

UnicodeHelper::UnicodeHelper(UnicodePlatform targetPlatform)
    : platform(targetPlatform) {
}

void UnicodeHelper::setPlatform(UnicodePlatform targetPlatform) {
    platform = targetPlatform;
}

UnicodePlatform UnicodeHelper::getPlatform() const {
    return platform;
}

String UnicodeHelper::getPlatformName() const {
    switch(platform) {
        case PLATFORM_WINDOWS: return "Windows";
        case PLATFORM_LINUX: return "Linux";
        case PLATFORM_MACOS: return "macOS";
        default: return "Unknown";
    }
}

bool UnicodeHelper::isASCII(uint32_t codepoint) {
    return codepoint < 128;
}

uint32_t UnicodeHelper::decodeUTF8(const String& str, int& index) {
    uint32_t codepoint = 0;
    uint8_t byte1 = (uint8_t)str[index];

    if (byte1 < 0x80) {
        // 1-byte character (ASCII): 0xxxxxxx
        codepoint = byte1;
        index++;
    }
    else if ((byte1 & 0xE0) == 0xC0) {
        // 2-byte character: 110xxxxx 10xxxxxx
        if (index + 1 < str.length()) {
            uint8_t byte2 = (uint8_t)str[index + 1];
            codepoint = ((byte1 & 0x1F) << 6) | (byte2 & 0x3F);
            index += 2;
        }
    }
    else if ((byte1 & 0xF0) == 0xE0) {
        // 3-byte character: 1110xxxx 10xxxxxx 10xxxxxx
        if (index + 2 < str.length()) {
            uint8_t byte2 = (uint8_t)str[index + 1];
            uint8_t byte3 = (uint8_t)str[index + 2];
            codepoint = ((byte1 & 0x0F) << 12) | ((byte2 & 0x3F) << 6) | (byte3 & 0x3F);
            index += 3;
        }
    }
    else if ((byte1 & 0xF8) == 0xF0) {
        // 4-byte character (emoji): 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        if (index + 3 < str.length()) {
            uint8_t byte2 = (uint8_t)str[index + 1];
            uint8_t byte3 = (uint8_t)str[index + 2];
            uint8_t byte4 = (uint8_t)str[index + 3];
            codepoint = ((byte1 & 0x07) << 18) | ((byte2 & 0x3F) << 12) |
                       ((byte3 & 0x3F) << 6) | (byte4 & 0x3F);
            index += 4;
        }
    }

    return codepoint;
}

void UnicodeHelper::sendNumpadDigit(uint8_t digit) {
    if (digit > 9) return;

    uint8_t keyCodes[] = {
        KEY_KP_0, KEY_KP_1, KEY_KP_2, KEY_KP_3, KEY_KP_4,
        KEY_KP_5, KEY_KP_6, KEY_KP_7, KEY_KP_8, KEY_KP_9
    };

    Keyboard.press(keyCodes[digit]);
    delay(10);
    Keyboard.release(keyCodes[digit]);
    delay(10);
}

void UnicodeHelper::sendHexDigit(char hexChar) {
    // Convert hex character to uppercase
    if (hexChar >= 'a' && hexChar <= 'f') {
        hexChar = hexChar - 'a' + 'A';
    }

    Keyboard.press(hexChar);
    delay(10);
    Keyboard.release(hexChar);
    delay(10);
}

bool UnicodeHelper::sendUnicodeChar(uint32_t codepoint) {
    if (!Keyboard.isConnected()) {
        Logger::getInstance().log("BLE not connected - cannot send Unicode");
        return false;
    }

    // For simple ASCII characters, use direct keyboard input
    if (isASCII(codepoint)) {
        Keyboard.write((char)codepoint);
        return true;
    }

    Logger::getInstance().log("Sending Unicode U+" + String(codepoint, HEX));

    switch(platform) {
        case PLATFORM_WINDOWS: {
            // Windows: Alt + decimal code on numpad
            // For characters > 255, use Alt + leading zero + decimal
            Keyboard.press(KEY_LEFT_ALT);
            delay(50);

            if (codepoint > 255) {
                // Send leading zero for extended characters
                sendNumpadDigit(0);
            }

            // Convert to decimal and send each digit
            String decimal = String(codepoint);
            for (int i = 0; i < decimal.length(); i++) {
                sendNumpadDigit(decimal[i] - '0');
            }

            delay(50);
            Keyboard.release(KEY_LEFT_ALT);
            delay(50);
            break;
        }

        case PLATFORM_LINUX: {
            // Linux: Ctrl+Shift+U, then hex code, then Space or Enter
            // IMPORTANT: Linux method has issues with certain Unicode ranges
            // including emoji. These appear as literal hex codes.

            Keyboard.press(KEY_LEFT_CTRL);
            Keyboard.press(KEY_LEFT_SHIFT);
            delay(20);
            Keyboard.press('u');
            delay(20);
            Keyboard.release('u');
            Keyboard.release(KEY_LEFT_SHIFT);
            Keyboard.release(KEY_LEFT_CTRL);
            delay(50);

            // Send hex digits
            String hex = String(codepoint, HEX);
            for (int i = 0; i < hex.length(); i++) {
                sendHexDigit(hex[i]);
            }

            // Press Space to confirm
            Keyboard.press(' ');
            delay(20);
            Keyboard.release(' ');
            delay(100); // Longer delay for Linux to process
            break;
        }

        case PLATFORM_MACOS: {
            // macOS: Option key held + hex code (requires Unicode Hex Input enabled)
            // Note: This requires the user to enable "Unicode Hex Input" in macOS
            Keyboard.press(KEY_LEFT_ALT); // Option key
            delay(50);

            String hex = String(codepoint, HEX);
            // Pad to 4 digits for macOS
            while (hex.length() < 4) {
                hex = "0" + hex;
            }

            for (int i = 0; i < hex.length(); i++) {
                sendHexDigit(hex[i]);
            }

            delay(50);
            Keyboard.release(KEY_LEFT_ALT);
            delay(50);
            break;
        }

        default:
            Logger::getInstance().log("Unknown platform for Unicode input");
            return false;
    }

    return true;
}

bool UnicodeHelper::sendUnicodeString(const String& text) {
    if (!Keyboard.isConnected()) {
        Logger::getInstance().log("BLE not connected - cannot send string");
        return false;
    }

    Logger::getInstance().log("Sending Unicode string: " + text);

    int index = 0;
    while (index < text.length()) {
        uint32_t codepoint = decodeUTF8(text, index);

        if (codepoint == 0) {
            Logger::getInstance().log("Invalid UTF-8 sequence");
            return false;
        }

        // Skip variation selectors on Linux (FE00-FE0F, E0100-E01EF)
        // These are problematic with Ctrl+Shift+U method
        if (platform == PLATFORM_LINUX) {
            if ((codepoint >= 0xFE00 && codepoint <= 0xFE0F) ||
                (codepoint >= 0xE0100 && codepoint <= 0xE01EF)) {
                Logger::getInstance().log("Skipping variation selector U+" + String(codepoint, HEX));
                continue;
            }
        }

        if (!sendUnicodeChar(codepoint)) {
            return false;
        }
    }

    return true;
}
