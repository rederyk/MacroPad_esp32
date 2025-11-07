#include "IRSender.h"
#include <IRutils.h>

IRSender::IRSender(int pin, bool isAnode)
    : _pin(pin), _isAnode(isAnode), _enabled(false), _irsend(nullptr)
{
}

IRSender::~IRSender()
{
    end();
}

bool IRSender::begin()
{
    if (_pin < 0)
        return false;

    _irsend = new IRsend(_pin);
    _irsend->begin();
    configurePin();
    _enabled = true;
    return true;
}

void IRSender::end()
{
    if (_irsend)
    {
        delete _irsend;
        _irsend = nullptr;
    }
    _enabled = false;
}

bool IRSender::isEnabled() const
{
    return _enabled;
}

void IRSender::configurePin()
{
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, _isAnode ? LOW : HIGH);
}

bool IRSender::sendIR(decode_type_t protocol, uint64_t value, uint16_t bits)
{
    if (!_enabled || !_irsend)
        return false;

    _irsend->send(protocol, value, bits);
    return true;
}

bool IRSender::sendRaw(const uint16_t *rawData, size_t length, uint16_t frequency)
{
    if (!_enabled || !_irsend)
        return false;

    _irsend->sendRaw(rawData, length, frequency);
    return true;
}


bool IRSender::sendCommand(JsonVariantConst cmdVariant)
{
    if (!_enabled || !_irsend)
        return false;

    if (cmdVariant.isNull() || !cmdVariant.is<JsonObjectConst>())
        return false;

    JsonObjectConst cmd = cmdVariant.as<JsonObjectConst>();

    JsonVariantConst protocolField = cmd["protocol"];
    if (protocolField.isNull())
        return false;

    const char *protocolCStr = protocolField.as<const char *>();
    if (!protocolCStr)
        return false;

    String protocol = protocolCStr;
    protocol.trim();
    protocol.toUpperCase();

    if (protocol == "RAW")
    {
        JsonArrayConst raw = cmd["raw"].as<JsonArrayConst>();
        if (raw.isNull())
            return false;

        size_t len = raw.size();
        if (len == 0 || len > 128)
            return false;

        uint16_t rawData[128];
        size_t index = 0;
        for (JsonVariantConst value : raw)
        {
            int rawValue = value.as<int>();
            if (rawValue <= 0)
                return false;
            rawData[index++] = static_cast<uint16_t>(rawValue);
        }

        uint16_t frequency = 38000;
        JsonVariantConst freqField = cmd["frequency"];
        if (freqField.isNull())
        {
            freqField = cmd["freq"];
        }
        if (!freqField.isNull())
        {
            int freqValue = freqField.as<int>();
            if (freqValue > 0)
            {
                frequency = static_cast<uint16_t>(freqValue);
            }
        }

        return sendRaw(rawData, len, frequency);
    }

    JsonVariantConst bitsField = cmd["bits"];
    if (bitsField.isNull())
        return false;

    int bitsValue = bitsField.as<int>();
    if (bitsValue <= 0)
        return false;
    uint16_t bits = static_cast<uint16_t>(bitsValue);

    JsonVariantConst valueField = cmd["value"];
    if (valueField.isNull())
        return false;

    uint64_t value = 0;
    if (valueField.is<const char *>())
    {
        String valueStr = valueField.as<const char *>();
        valueStr.trim();
        if (valueStr.startsWith("0x") || valueStr.startsWith("0X"))
        {
            valueStr = valueStr.substring(2);
        }
        if (valueStr.length() == 0)
            return false;
        value = strtoull(valueStr.c_str(), nullptr, 16);
    }
    else if (valueField.is<uint64_t>())
    {
        value = valueField.as<uint64_t>();
    }
    else if (valueField.is<unsigned long>())
    {
        value = static_cast<uint64_t>(valueField.as<unsigned long>());
    }
    else if (valueField.is<int>())
    {
        value = static_cast<uint64_t>(valueField.as<int>());
    }
    else if (valueField.is<double>())
    {
        value = static_cast<uint64_t>(valueField.as<double>());
    }
    else
    {
        return false;
    }

    decode_type_t protoType = strToDecodeType(protocol.c_str());
    if (protoType == decode_type_t::UNKNOWN)
        return false;

    return sendIR(protoType, value, bits);
}
