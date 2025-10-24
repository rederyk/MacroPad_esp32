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

bool IRSender::sendRaw(const uint16_t *rawData, size_t length)
{
    if (!_enabled || !_irsend)
        return false;

    const uint16_t kFrequency = 38000; // default IR frequency in Hz (38kHz)
    _irsend->sendRaw(rawData, length, kFrequency);
    return true;
}


bool IRSender::sendCommand(JsonObject cmd) {
    if (!cmd.containsKey("protocol"))
        return false;

    String protocol = cmd["protocol"];

    if (protocol == "RAW") {
        JsonArray raw = cmd["raw"];
        size_t len = raw.size();
        if (len == 0 || len > 128) return false;

        uint16_t rawData[128];
        for (size_t i = 0; i < len; i++)
            rawData[i] = raw[i];

        return sendRaw(rawData, len);
    }

    uint64_t value = strtoull(cmd["value"], nullptr, 16);
    uint16_t bits = cmd["bits"];

    decode_type_t protoType = strToDecodeType(protocol.c_str());
    if (protoType == decode_type_t::UNKNOWN)
        return false;

    return sendIR(protoType, value, bits);
}
