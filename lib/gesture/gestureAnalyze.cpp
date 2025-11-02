/*
 * ESP32 MacroPad Project
 * Simplified gesture analysis wrapper.
 */

#include "gestureAnalyze.h"

#include "MPU6050GestureRecognizer.h"
#include "ADXL345GestureRecognizer.h"
#include "Logger.h"

GestureAnalyze::GestureAnalyze(GestureRead &gestureReader)
    : _gestureReader(gestureReader),
      _confidenceThreshold(0.5f),
      _recognizer(nullptr),
      _currentSensorType()
{
}

void GestureAnalyze::clearSamples()
{
    _gestureReader.clearMemory();
}

SampleBuffer &GestureAnalyze::getRawSample()
{
    return _gestureReader.getCollectedSamples();
}

bool GestureAnalyze::initRecognizer(const String &sensorType, const String &gestureMode)
{
    _currentSensorType = sensorType;

    String mode = gestureMode;
    mode.toLowerCase();
    String normalizedSensor = sensorType;
    normalizedSensor.toLowerCase();

    if (mode == "auto")
    {
        if (normalizedSensor == "mpu6050")
        {
            mode = "mpu6050";
        }
        else if (normalizedSensor == "adxl345")
        {
            mode = "adxl345";
        }
        else
        {
            Logger::getInstance().log("[GestureAnalyze] Unknown sensor type: " + sensorType);
            return false;
        }
    }

    if (mode == "mpu6050" || mode == "shape" || mode == "orientation")
    {
        _recognizer.reset(new MPU6050GestureRecognizer());
        Logger::getInstance().log("[GestureAnalyze] Using MPU6050 recognizer (shape+orientation)");
    }
    else if (mode == "adxl345")
    {
        _recognizer.reset(new ADXL345GestureRecognizer());
        Logger::getInstance().log("[GestureAnalyze] Using ADXL345 recognizer (shape-only)");
    }
    else
    {
        Logger::getInstance().log("[GestureAnalyze] Gesture mode not supported without training: " + mode);
        _recognizer.reset();
        return false;
    }

    if (!_recognizer->init(normalizedSensor))
    {
        Logger::getInstance().log("[GestureAnalyze] Failed to initialize recognizer");
        _recognizer.reset();
        return false;
    }

    _recognizer->setConfidenceThreshold(_confidenceThreshold);
    Logger::getInstance().log("[GestureAnalyze] Recognizer initialized: " + _recognizer->getModeName());
    return true;
}

GestureRecognitionResult GestureAnalyze::recognizeWithRecognizer()
{
    if (!_recognizer)
    {
        Logger::getInstance().log("[GestureAnalyze] No recognizer initialized");
        return GestureRecognitionResult();
    }

    SampleBuffer &buffer = getRawSample();
    if (buffer.sampleCount == 0 || buffer.samples == nullptr)
    {
        Logger::getInstance().log("[GestureAnalyze] No samples to analyze");
        return GestureRecognitionResult();
    }

    GestureRecognitionResult result = _recognizer->recognize(&buffer);
    if (result.gestureID >= 0 && result.confidence >= _confidenceThreshold)
    {
        Logger::getInstance().log(String("[GestureAnalyze] Recognized: ") + result.gestureName +
                                  " (ID: " + String(result.gestureID) +
                                  ", conf: " + String(result.confidence, 2) + ")");
    }
    else
    {
        Logger::getInstance().log("[GestureAnalyze] No gesture recognized (low confidence)");
    }
    return result;
}

String GestureAnalyze::getRecognizerModeName() const
{
    if (!_recognizer)
    {
        return "None";
    }
    return _recognizer->getModeName();
}

void GestureAnalyze::setConfidenceThreshold(float threshold)
{
    _confidenceThreshold = constrain(threshold, 0.0f, 1.0f);
    if (_recognizer)
    {
        _recognizer->setConfidenceThreshold(_confidenceThreshold);
    }
}
