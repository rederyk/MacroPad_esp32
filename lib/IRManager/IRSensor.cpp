#include "IRSensor.h"
#include <ArduinoJson.h>

// Costruttore
IRSensor::IRSensor(int pin) : _pin(pin),
                              _irrecv(nullptr),
                              _lastSignalJson(""),
                              _lastDecodeTime(0),
                              _lastDecodedValue(0)

{
    if (_pin < 0)
    {
        // Logger::getInstance().log("IRSensor Error: Invalid pin specified (" + String(_pin) + ")");
    }
}

// Distruttore
IRSensor::~IRSensor()
{
    delete _irrecv;
    _irrecv = nullptr;
    // Logger::getInstance().log("IRSensor object destroyed.");
}

// Inizializzazione
bool IRSensor::begin()
{
    if (_pin < 0)
    {
        // Invalid pin - cannot initialize
        return false;
    }
    if (_irrecv == nullptr)
    {
        // Create new IRrecv instance
        _irrecv = new IRrecv(_pin);
        if (_irrecv)
        {
            _irrecv->enableIRIn();
            _lastDecodeTime = 0;
            return true;
        }
        else
        {
            // Failed to allocate memory
            return false;
        }
    }
    else
    {
        // Already initialized, just enable it again
        _irrecv->enableIRIn();
        _lastDecodeTime = 0;
        return true;
    }
}
void IRSensor::end()
{
    if (_irrecv)
    {
        _irrecv->disableIRIn();
    }
}

bool IRSensor::isEnabled() const
{
    return (_irrecv != nullptr);
}

// Pulisce buffer IR e _results per rimuovere segnali precedenti
void IRSensor::clearBuffer()
{
    if (_irrecv)
    {
        // Svuota il buffer hardware del ricevitore IR
        _irrecv->resume();

        // Pulisce eventuali segnali decodificati nel buffer
        while (_irrecv->decode(&_results))
        {
            _irrecv->resume();
        }
    }

    // Reset della struttura _results
    memset(&_results, 0, sizeof(_results));
    _lastDecodedValue = 0;
    _lastDecodeTime = 0;
}

// Controllo e decodifica segnale
bool IRSensor::checkAndDecodeSignal()
{

    if (_irrecv == nullptr)
        return false;

    if (_irrecv->decode(&_results))
    {
        unsigned long currentMillis = millis();

        // Debounce (solo per i non repeat)

        if (!_results.repeat && (currentMillis - _lastDecodeTime < IR_DEBOUNCE_MS))
        {
            _irrecv->resume();
            return false;
        }

        // Scarta segnali UNKNOWN troppo brevi
        if (_results.decode_type == decode_type_t::UNKNOWN && _results.bits < 8)
        {
            // Logger::getInstance().log("Ignoring unknown or too short signal.");
            _irrecv->resume();
            return false;
        }

        bool success = false;

        success = true;
        _lastDecodedValue = _results.value;
        _lastDecodeTime = currentMillis;

        _irrecv->resume();
        return success;
    }

    return false;
}

unsigned long IRSensor::getLastDecodedValue() const
{
    return _lastDecodedValue;
}

const decode_results &IRSensor::getRawSignalObject() const
{
    return _results;
}

String IRSensor::getProtocolName(decode_type_t protocol)
{
    return typeToString(protocol, false);
}

uint16_t* IRSensor::getRawDataArray(uint16_t &length) const
{
    length = getCorrectedRawLength(&_results);
    return resultToRawArray(&_results);
}

// Metodo semplificato che converte direttamente rawbuf in microsecondi
// senza la complessità di resultToRawArray (che può generare array troppo lunghi)
uint16_t* IRSensor::getRawDataSimple(uint16_t &length) const
{
    if (_results.rawlen <= 1) {
        length = 0;
        return nullptr;
    }

    // rawlen include il primo elemento (gap), quindi usiamo rawlen - 1
    length = _results.rawlen - 1;
    uint16_t* result = new uint16_t[length];

    if (result != nullptr) {
        // Copia e converti i dati da rawbuf (salta il primo elemento)
        for (uint16_t i = 0; i < length; i++) {
            // Moltiplica per kRawTick (tipicamente 2) per ottenere microsecondi
            result[i] = _results.rawbuf[i + 1] * kRawTick;
        }
    }

    return result;
}


// Intercetta un comando ripetuto N volte, ignorando protocolli non supportati se richiesto
decode_results IRSensor::interceptCommand(int numRepetitions, bool protocolRequired)
{
    unsigned long lastCommand = 0;
    unsigned long lastCommandTime = 0;
    unsigned int repeatCount = 0;
    decode_results lastResult = {};

    const unsigned long maxDelayBetweenRepeats = 300; // ms massimo fra ripetizioni

    while (true)
    {
        if (checkAndDecodeSignal())
        {
            const decode_results &res = getRawSignalObject();
            unsigned long currentCmd = res.value;
            bool isRepeat = res.repeat;
            unsigned long now = millis();

            if (isRepeat)
            {
                if (lastCommand != 0 && (now - lastCommandTime) < maxDelayBetweenRepeats)
                {
                    repeatCount++;
                    lastCommandTime = now;
                }
            }
            else if (currentCmd != 0 && currentCmd != 0xFFFFFFFF)
            {
                // Prima controlliamo se il protocollo è supportato
                // bool protocolSupported = (_sensor.getProtocolSupport(res.decode_type) != ProtocolSupport::None);

                String protocol = getProtocolName(res.decode_type);
                bool protocolSupported = (protocol != "UNKNOWN");
                if ((protocolRequired && !protocolSupported) || (!protocolRequired && protocolSupported))
                {
                    continue; // Scarta questo segnale
                }

                // Ora possiamo gestirlo
                if (currentCmd == lastCommand && (now - lastCommandTime) < maxDelayBetweenRepeats)
                {
                    repeatCount++;
                }
                else
                {
                    repeatCount = 1; // Reset su nuovo comando
                    lastCommand = currentCmd;
                    lastResult = res;
                }
                lastCommandTime = now;
            }

            // Verifica se abbiamo raggiunto il numero di ripetizioni richiesto
            if (repeatCount >= numRepetitions)
            {
                return lastResult;
            }
        }
        delay(5); // Piccolo delay per alleggerire la CPU
    }
}
