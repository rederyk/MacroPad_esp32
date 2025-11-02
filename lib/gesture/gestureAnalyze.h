/*
 * ESP32 MacroPad Project
 * Simplified gesture analysis wrapper.
 */

#ifndef GESTUREANALYZE_H
#define GESTUREANALYZE_H

#include <Arduino.h>
#include <memory>

#include "gestureRead.h"
#include "IGestureRecognizer.h"

class GestureAnalyze
{
public:
    explicit GestureAnalyze(GestureRead &gestureReader);

    void clearSamples();
    SampleBuffer &getRawSample();

    bool initRecognizer(const String &sensorType, const String &gestureMode);
    GestureRecognitionResult recognizeWithRecognizer();

    bool hasRecognizer() const { return _recognizer != nullptr; }
    String getRecognizerModeName() const;

    void setConfidenceThreshold(float threshold);
    float getConfidenceThreshold() const { return _confidenceThreshold; }

private:
    GestureRead &_gestureReader;
    float _confidenceThreshold;
    std::unique_ptr<IGestureRecognizer> _recognizer;
    String _currentSensorType;
};

extern GestureAnalyze gestureAnalyzer;

#endif // GESTUREANALYZE_H
