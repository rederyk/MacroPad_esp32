// IRSensor.h
#ifndef IRSENSOR_H
#define IRSENSOR_H

#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <ArduinoJson.h>
#include "Logger.h"

// Forward declaration delle funzioni da IRutils
uint16_t *resultToRawArray(const decode_results * const decode);
uint16_t getCorrectedRawLength(const decode_results * const results);

// kRawTick dalla libreria IRremoteESP8266
#ifndef kRawTick
extern const uint16_t kRawTick;
#endif

#ifndef JSON_IR_DOC_SIZE
#define JSON_IR_DOC_SIZE 4096
#endif

class IRSensor
{
public:
    explicit IRSensor(int pin);
    ~IRSensor();

    bool begin();
    bool checkAndDecodeSignal();
    unsigned long getLastDecodedValue() const;
  //  String getIRSignalAsJson() const;
    void end();             // Disabilita il sensore
    bool isEnabled() const; // Verifica se il sensore è attivo
    void clearBuffer();     // Pulisce buffer IR e _results
    const decode_results& getRawSignalObject() const;
    decode_results interceptCommand(int numRepetitions, bool protocolRequired=true);
    String getProtocolName(decode_type_t protocol);
    uint16_t* getRawDataArray(uint16_t &length) const;
    uint16_t* getRawDataSimple(uint16_t &length) const;


 //   ProtocolSupport getProtocolSupport(decode_type_t protocol);


private:
    uint64_t _lastDecodedValue;

    int _pin;
    IRrecv *_irrecv;
    decode_results _results;
    String _lastSignalJson;
    unsigned long _lastDecodeTime;

    static const unsigned long IR_DEBOUNCE_MS = 200;

   // void populateIRSignalJson(const decode_results &res, JsonDocument &doc);
  //  bool isValidSignal(const decode_results &res);
};

#endif // IRSENSOR_H
