#ifndef IRSTORAGE_H
#define IRSTORAGE_H

#include <Arduino.h>
#include <LittleFS.h>
#include <IRremoteESP8266.h>
#include <ArduinoJson.h>

#define JSON_IR_DOC_SIZE 4096

enum class ProtocolSupport {
    None,
    CommandOnly,
    AddressOnly,
    AddressAndCommand
};

class IRStorage {
public:
    IRStorage();
    ~IRStorage();

    bool begin();
    void end();

    bool loadIRData();
    bool saveIRData();

    bool addDevice(const String& deviceName);
    bool addIRCommand(const String& deviceName, const String& commandName, 
                     decode_type_t protocol, uint64_t value, uint16_t bits);
    bool addRawIRCommand(const String& deviceName, const String& commandName,
                        const uint16_t* rawData, size_t length);

    String getProtocolName(decode_type_t protocol);
    ProtocolSupport getProtocolSupport(decode_type_t protocol);

    // Restituisce il contenuto JSON come stringa
    String getJsonString();



    JsonObject getDeviceCommands(const String &deviceName);
    JsonObject getCommand(const String &deviceName, const String &commandName);

    
    // Restituisce il JSON come oggetto JsonDocument
    const JsonDocument& getJsonObject() const;

private:
    bool _fsInitialized;
    StaticJsonDocument<JSON_IR_DOC_SIZE> _jsonDoc;

    bool _openFile(const char* mode);
    void _closeFile();
};

#endif
