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

constexpr size_t BUFFER_SIZE = 100;

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
