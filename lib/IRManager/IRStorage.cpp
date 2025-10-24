#include "IRStorage.h"
#include <ArduinoJson.h>
#include <IRremoteESP8266.h>
#include <LittleFS.h>
#include "IRutils.h"

IRStorage::IRStorage() : _fsInitialized(false)
{
    _jsonDoc.clear();
    _jsonDoc["devices"] = JsonObject();
}

IRStorage::~IRStorage()
{
    end();
}

bool IRStorage::begin()
{
    // Try to initialize LittleFS
    _fsInitialized = LittleFS.begin();

    // Note: Cannot use Logger here as it might cause circular dependency
    // The calling code in main.cpp will log the success/failure

    if (_fsInitialized)
    {
        loadIRData();
    }
    return _fsInitialized;
}

void IRStorage::end()
{
    if (_fsInitialized)
    {
       // saveIRData();
        LittleFS.end();
        _fsInitialized = false;
    }
}

bool IRStorage::loadIRData()
{
    if (!_fsInitialized)
        return false;

    File file = LittleFS.open("/ir_data.json", "r");
    if (!file)
    {
        _jsonDoc.clear();
        _jsonDoc["devices"] = JsonObject();
        return false;
    }

    DeserializationError error = deserializeJson(_jsonDoc, file);
    file.close();

    if (error || _jsonDoc.overflowed())
    {
        _jsonDoc.clear();
        _jsonDoc["devices"] = JsonObject();
        return false;
    }

    if (!_jsonDoc.containsKey("devices"))
    {
        _jsonDoc["devices"] = JsonObject();
    }
    return true;
}

bool IRStorage::saveIRData()
{
    if (!_fsInitialized || _jsonDoc.overflowed())
    {
        return false;
    }

    File file = LittleFS.open("/ir_data.json", "w");
    if (!file)
    {
        return false;
    }

    size_t bytesWritten = serializeJson(_jsonDoc, file);
    file.close();

    return bytesWritten > 0;
}

bool IRStorage::addDevice(const String &deviceName)
{
    if (!_jsonDoc["devices"].containsKey(deviceName))
    {
        _jsonDoc["devices"][deviceName] = JsonObject();
        return true;
    }
    return false;
}

bool IRStorage::addIRCommand(const String &deviceName, const String &commandName,
                             decode_type_t protocol, uint64_t value, uint16_t bits)
{
    if (_jsonDoc.overflowed())
    {
       // Logger::getInstance().log("JSON Document overflowed.");
        return false;
    }

    if (!_jsonDoc["devices"].containsKey(deviceName))
    {
        _jsonDoc["devices"][deviceName] = JsonObject();
    }

    JsonObject cmd = _jsonDoc["devices"][deviceName].createNestedObject(commandName);
    if (cmd.isNull())
    {
       // Logger::getInstance().log("Failed to create nested command object.");
        return false;
    }

    cmd["protocol"] = getProtocolName(protocol);
    cmd["value"] = String(value, HEX);
    cmd["bits"] = bits;

    ProtocolSupport support = getProtocolSupport(protocol);
    if (support == ProtocolSupport::AddressAndCommand || support == ProtocolSupport::AddressOnly)
    {
        cmd["address"] = (value >> 16) & 0xFFFF;
    }
    if (support == ProtocolSupport::AddressAndCommand || support == ProtocolSupport::CommandOnly)
    {
        cmd["command"] = value & 0xFFFF;
    }

    return true;
}

bool IRStorage::addRawIRCommand(const String &deviceName, const String &commandName,
                                const uint16_t *rawData, size_t length)
{
    if (_jsonDoc.overflowed())
    {
        return false;
    }

    const size_t maxRawToCopy = 128;
    if (length > maxRawToCopy)
    {
        return false;
    }

    if (!_jsonDoc["devices"].containsKey(deviceName))
    {
        if (!addDevice(deviceName))
        {
            return false;
        }
    }

    JsonObject cmd = _jsonDoc["devices"][deviceName].createNestedObject(commandName);
    if (cmd.isNull())
    {
        return false;
    }

    cmd["protocol"] = "RAW";
    JsonArray raw = cmd.createNestedArray("raw");
    for (size_t i = 0; i < length; i++)
    {
        raw.add(rawData[i]);
    }

    return !_jsonDoc.overflowed();
}

ProtocolSupport IRStorage::getProtocolSupport(decode_type_t protocol)
{
    switch (protocol)
    {
    case NEC:
    case NEC_LIKE:
    case SAMSUNG:
    case LG:
    case SONY:
    case PANASONIC:
    case SANYO:
    case SHARP:
    case JVC:
    case RC5:
    case RC6:
    case DENON:
        return ProtocolSupport::AddressAndCommand;
    case WHYNTER:
    case LEGOPF:
    case MAGIQUEST:
    case BOSE:
        return ProtocolSupport::CommandOnly;
    default:
        return ProtocolSupport::None;
    }
}

String IRStorage::getProtocolName(decode_type_t protocol)
{
    return typeToString(protocol, false);
}

String IRStorage::getJsonString()
{
    String jsonString;
    serializeJson(_jsonDoc, jsonString);
    return jsonString;
}

const JsonDocument& IRStorage::getJsonObject() const
{
    return _jsonDoc;
}

JsonObject IRStorage::getDeviceCommands(const String &deviceName) {
    return _jsonDoc["devices"][deviceName].as<JsonObject>();
}

JsonObject IRStorage::getCommand(const String &deviceName, const String &commandName) {
    return _jsonDoc["devices"][deviceName][commandName].as<JsonObject>();
}

