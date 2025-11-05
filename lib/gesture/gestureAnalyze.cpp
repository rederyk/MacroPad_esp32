/*
 * ESP32 MacroPad Project
 * Simplified gesture analysis wrapper.
 */

#include "gestureAnalyze.h"
#include "Logger.h"

GestureAnalyze::GestureAnalyze(GestureRead &gestureReader)
    : _gestureReader(gestureReader),
      _confidenceThreshold(0.5f),
      _currentSensorType("")
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

GestureRecognitionResult GestureAnalyze::recognize(SampleBuffer* buffer)
{
    if (!buffer || buffer->sampleCount == 0 || buffer->samples == nullptr)
    {
        Logger::getInstance().log("[GestureAnalyze] No samples to analyze");
        return GestureRecognitionResult();
    }

    SimpleGestureConfig config;
    String normalizedSensor = _currentSensorType;
    normalizedSensor.toLowerCase();

    if (normalizedSensor == "mpu6050")
    {
        config.sensorTag = "MPU6050";
        config.sensorMode = SENSOR_MODE_MPU6050;
        config.useGyro = true;
        config.gyroSwipeThreshold = 100.0f;
        config.gyroShakeThreshold = 150.0f;
        config.accelSwipeThreshold = 0.6f;
        config.accelShakeThreshold = 1.2f;
    }
    else if (normalizedSensor == "adxl345")
    {
        config.sensorTag = "ADXL345";
        config.sensorMode = SENSOR_MODE_ADXL345;
        config.useGyro = false;
        config.accelSwipeThreshold = 0.6f;
        config.accelShakeThreshold = 1.2f;
    }
    else
    {
        Logger::getInstance().log("[GestureAnalyze] Unknown sensor type: " + _currentSensorType);
        return GestureRecognitionResult();
    }

    GestureRecognitionResult result = detectSimpleGesture(buffer, config);
    if (result.gestureID >= 0 && result.confidence < _confidenceThreshold)
    {
        Logger::getInstance().log("Gesture discarded (confidence " +
                                  String(result.confidence, 2) + ")");
        return GestureRecognitionResult();
    }

    return result;
}

void GestureAnalyze::setSensorType(const String& sensorType)
{
    _currentSensorType = sensorType;
}

void GestureAnalyze::setConfidenceThreshold(float threshold)
{
    _confidenceThreshold = constrain(threshold, 0.0f, 1.0f);
}
