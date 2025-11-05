/*
 * ESP32 MacroPad Project
 * Simplified gesture analysis wrapper.
 */

#ifndef GESTUREANALYZE_H
#define GESTUREANALYZE_H

#include <Arduino.h>
#include "gestureRead.h"
#include "SimpleGestureDetector.h"

class GestureAnalyze
{
public:
    explicit GestureAnalyze(GestureRead &gestureReader);

    void clearSamples();
    SampleBuffer &getRawSample();

    GestureRecognitionResult recognize(SampleBuffer* buffer);

    void setSensorType(const String& sensorType);

    void setConfidenceThreshold(float threshold);
    float getConfidenceThreshold() const { return _confidenceThreshold; }

private:
    GestureRead &_gestureReader;
    float _confidenceThreshold;
    String _currentSensorType;
};

extern GestureAnalyze gestureAnalyzer;

#endif // GESTUREANALYZE_H
