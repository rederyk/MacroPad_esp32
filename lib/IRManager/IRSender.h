#ifndef IRSENDER_H
#define IRSENDER_H

#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ArduinoJson.h>
class IRSender
{
public:
    IRSender(int pin, bool isAnode);
    ~IRSender();

    bool begin();
    void end();

    bool isEnabled() const;

    bool sendIR(decode_type_t protocol, uint64_t value, uint16_t bits);
    bool sendRaw(const uint16_t *rawData, size_t length);

    bool sendCommand(JsonVariantConst cmd); // metodo più semplice

private:
    int _pin;
    bool _isAnode;
    bool _enabled;
    IRsend *_irsend;

    void configurePin();
};
#endif
