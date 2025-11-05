/*
 * ESP32 MacroPad Project - Unicode Helper
 * Copyright (C) [2025] [Enrico Mori]
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef UNICODE_HELPER_H
#define UNICODE_HELPER_H

#include <Arduino.h>
#include <BleComboKeyboard.h>

// Target platform for Unicode input
enum UnicodePlatform {
    PLATFORM_WINDOWS,   // Uses Alt + numpad codes
    PLATFORM_LINUX,     // Uses Ctrl+Shift+U + hex
    PLATFORM_MACOS      // Uses Unicode Hex Input method
};

/**
 * Helper class for sending Unicode characters and emoji via BLE HID keyboard
 * Bypasses keyboard layout issues by using platform-specific Unicode input methods
 */
class UnicodeHelper {
private:
    UnicodePlatform platform;

    /**
     * Decode UTF-8 string to Unicode codepoint
     * Returns the Unicode codepoint and advances the index
     */
    static uint32_t decodeUTF8(const String& str, int& index);

    /**
     * Send a digit on the numeric keypad (for Windows Alt codes)
     */
    static void sendNumpadDigit(uint8_t digit);

    /**
     * Send a hex digit (for Linux Ctrl+Shift+U method)
     */
    static void sendHexDigit(char hexChar);

public:
    /**
     * Constructor
     * @param targetPlatform The target OS platform (default: Windows)
     */
    UnicodeHelper(UnicodePlatform targetPlatform = PLATFORM_WINDOWS);

    /**
     * Set the target platform
     */
    void setPlatform(UnicodePlatform targetPlatform);

    /**
     * Get current platform
     */
    UnicodePlatform getPlatform() const;

    /**
     * Send a single Unicode character by its codepoint
     * @param codepoint Unicode codepoint (e.g., 0x1F600 for 😀)
     * @return true if successful
     */
    bool sendUnicodeChar(uint32_t codepoint);

    /**
     * Send a UTF-8 encoded string with full Unicode support
     * Automatically handles multi-byte characters and emoji
     * @param text UTF-8 encoded string
     * @return true if successful
     */
    bool sendUnicodeString(const String& text);

    /**
     * Check if a character is ASCII (0-127)
     */
    static bool isASCII(uint32_t codepoint);

    /**
     * Get platform name as string
     */
    String getPlatformName() const;
};

#endif // UNICODE_HELPER_H
