/*
 * ESP32 MacroPad Project
 * Copyright (C) [2025] [Enrico Mori]
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <vector>
#include <functional>

// Structure for each log entry
struct LogEntry {
    String message;
    bool newLine;
};

constexpr size_t BUFFER_SIZE = 64;

class Logger
{
public:
    static Logger &getInstance(); // Singleton for using the class anywhere

    void log(const String &message, bool newLine = true);          // Method to send a log message
    void addOutput(std::function<void(const String &)> output);      // Add additional log outputs
    void setWebServerActive(bool active);                          // Activate/deactivate web server output
    void setSerialEnabled(bool enabled);                           // Activate/deactivate serial output
    void processBuffer();                                          // Process the buffered log messages

private:
    Logger() {} // Private constructor to prevent multiple instances

    // Members used in logging
    std::vector<std::function<void(const String &)>> outputs; // List of registered output functions
    bool webServerActive = false;                             // Flag indicating if the web server is active
    bool serialEnabled = true;                                // Flag indicating if the serial output is active
    LogEntry logBuffer[BUFFER_SIZE];
    size_t bufferWriteIndex = 0;
    size_t bufferReadIndex = 0;
    size_t bufferCount = 0;
    unsigned long lastProcessTime = 0;                        // For delay control in processing
};

#endif
