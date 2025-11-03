/*
 * ESP32 MacroPad Project
 *
 * Shared swipe/shake classifier used by MPU6050 and ADXL345 recognizers.
 */

#include "SimpleGestureDetector.h"

#include <algorithm>
#include <cmath>

#include <Arduino.h>
#include <Logger.h>

namespace
{
    constexpr float kDefaultSwipeThreshold = 0.6f;
    constexpr float kDefaultShakeMinPeak = 0.7f;
    constexpr float kDefaultShakeRange = 1.8f;

    struct AxisStats
    {
        float min;
        float max;

        AxisStats() : min(0.0f), max(0.0f) {}
        AxisStats(float firstValue) : min(firstValue), max(firstValue) {}

        void update(float value)
        {
            if (value < min)
            {
                min = value;
            }
            if (value > max)
            {
                max = value;
            }
        }

        float range() const { return max - min; }
    };

    struct OrientationInfo
    {
        bool gravity[3];

        OrientationInfo()
        {
            gravity[0] = gravity[1] = gravity[2] = false;
        }
    };

    OrientationInfo detectOrientation(const Sample &firstSample)
    {
        OrientationInfo info;
        float absValues[3] = {
            fabsf(firstSample.x),
            fabsf(firstSample.y),
            fabsf(firstSample.z)};

        const float maxValue = std::max({absValues[0], absValues[1], absValues[2]});
        if (maxValue > 0.5f)
        {
            const int axis = (maxValue == absValues[0]) ? 0 : (maxValue == absValues[1] ? 1 : 2);
            info.gravity[axis] = true;
        }
        return info;
    }

    const char *axisName(int axis)
    {
        switch (axis)
        {
        case 0:
            return "X";
        case 1:
            return "Y";
        case 2:
            return "Z";
        default:
            return "?";
        }
    }

    float clampMinMax(float value, float minValue, float maxValue)
    {
        return std::max(minValue, std::min(maxValue, value));
    }
} // namespace

GestureRecognitionResult detectSimpleGesture(SampleBuffer *buffer, const SimpleGestureConfig &config)
{
    GestureRecognitionResult result;
    result.sensorMode = config.sensorMode;

    if (!buffer || buffer->sampleCount < 3 || buffer->samples == nullptr)
    {
        Logger::getInstance().log(String(config.sensorTag) + ": insufficient samples");
        return result;
    }

    const Sample &first = buffer->samples[0];
    const bool gyroActive = config.useGyro && first.gyroValid;
    const OrientationInfo orientation = detectOrientation(first);

    AxisStats accelStats[3] = {
        AxisStats(first.x),
        AxisStats(first.y),
        AxisStats(first.z)};
    AxisStats gyroStats[3] = {
        AxisStats(first.gyroX),
        AxisStats(first.gyroY),
        AxisStats(first.gyroZ)};

    float maxAccelMagnitude = 0.0f;
    float maxGyroMagnitude = 0.0f;

    for (uint16_t i = 0; i < buffer->sampleCount; i++)
    {
        const Sample &sample = buffer->samples[i];

        accelStats[0].update(sample.x);
        accelStats[1].update(sample.y);
        accelStats[2].update(sample.z);

        const float accelMag = sqrtf(sample.x * sample.x + sample.y * sample.y + sample.z * sample.z);
        if (accelMag > maxAccelMagnitude)
        {
            maxAccelMagnitude = accelMag;
        }

        if (gyroActive && sample.gyroValid)
        {
            gyroStats[0].update(sample.gyroX);
            gyroStats[1].update(sample.gyroY);
            gyroStats[2].update(sample.gyroZ);

            const float gyroMag = sqrtf(sample.gyroX * sample.gyroX +
                                        sample.gyroY * sample.gyroY +
                                        sample.gyroZ * sample.gyroZ);
            if (gyroMag > maxGyroMagnitude)
            {
                maxGyroMagnitude = gyroMag;
            }
        }
    }

    Logger::getInstance().log(String(config.sensorTag) +
                              ": Gravity on " +
                              (orientation.gravity[0] ? "X" : orientation.gravity[1] ? "Y" : orientation.gravity[2] ? "Z" : "NONE") +
                              " (gyro=" + String(gyroActive ? 1 : 0) + ")");

    Logger::getInstance().log(String(config.sensorTag) + ": accelRange X=" + String(accelStats[0].range(), 2) +
                              " Y=" + String(accelStats[1].range(), 2) +
                              " Z=" + String(accelStats[2].range(), 2));

    if (gyroActive)
    {
        Logger::getInstance().log(String(config.sensorTag) + ": gyroRange X=" + String(gyroStats[0].range(), 2) +
                                  " Y=" + String(gyroStats[1].range(), 2) +
                                  " Z=" + String(gyroStats[2].range(), 2) +
                                  " maxMag=" + String(maxGyroMagnitude, 2));
    }

    const float swipeThreshold = (config.swipeAccelThreshold > 0.0f) ? config.swipeAccelThreshold : kDefaultSwipeThreshold;
    const float shakeMinPositive = (config.shakeBidirectionalMax > 0.0f) ? config.shakeBidirectionalMax : kDefaultShakeMinPeak;
    const float shakeMinNegative = (config.shakeBidirectionalMin > 0.0f) ? config.shakeBidirectionalMin : kDefaultShakeMinPeak;
    const float shakeRange = (config.shakeRangeThreshold > 0.0f) ? config.shakeRangeThreshold : kDefaultShakeRange;

    float maxMovement = 0.0f;
    int movementAxis = -1;

    for (int axis = 0; axis < 3; axis++)
    {
        if (orientation.gravity[axis])
        {
            continue;
        }

        const float range = accelStats[axis].range();
        if (range > maxMovement && range > swipeThreshold)
        {
            maxMovement = range;
            movementAxis = axis;
        }
    }

    if (movementAxis < 0)
    {
        Logger::getInstance().log(String(config.sensorTag) + ": No gesture recognized");
        return result;
    }

    const float axisMin = accelStats[movementAxis].min;
    const float axisMax = accelStats[movementAxis].max;
    const float axisRange = accelStats[movementAxis].range();

    const bool isShake = (axisMin < -shakeMinNegative) &&
                         (axisMax > shakeMinPositive) &&
                         (axisRange > shakeRange);

    if (isShake)
    {
        result.gestureID = 203;
        result.gestureName = "G_SHAKE";
        result.confidence = clampMinMax(axisRange / 3.0f, 0.5f, 1.0f);

        Logger::getInstance().log(String(config.sensorTag) + ": SHAKE detected on axis=" +
                                  String(axisName(movementAxis)) +
                                  " range=" + String(axisRange, 2) +
                                  " min=" + String(axisMin, 2) +
                                  " max=" + String(axisMax, 2) +
                                  " (conf: " + String(result.confidence, 2) + ")");
        return result;
    }

    float swipeDirection = (axisMax + axisMin) * 0.5f;

    if (gyroActive)
    {
        const float gyroSums[3] = {
            gyroStats[0].min + gyroStats[0].max,
            gyroStats[1].min + gyroStats[1].max,
            gyroStats[2].min + gyroStats[2].max};

        // Choose gyro axis most correlated with detected movement
        int gyroAxis = movementAxis;
        if (movementAxis == 0)
        {
            gyroAxis = (gyroStats[1].range() > gyroStats[2].range()) ? 1 : 2;
        }
        else if (movementAxis == 1)
        {
            gyroAxis = (gyroStats[0].range() > gyroStats[2].range()) ? 0 : 2;
        }
        else
        {
            gyroAxis = (gyroStats[0].range() > gyroStats[1].range()) ? 0 : 1;
        }

        if (fabsf(gyroSums[gyroAxis]) > 0.5f)
        {
            swipeDirection = gyroSums[gyroAxis];
        }
    }

    const bool isRight = swipeDirection > 0.0f;
    result.gestureID = isRight ? 201 : 202;
    result.gestureName = isRight ? "G_SWIPE_RIGHT" : "G_SWIPE_LEFT";
    result.confidence = clampMinMax(axisRange / (swipeThreshold * 2.0f), 0.5f, 1.0f);

    Logger::getInstance().log(String(config.sensorTag) + ": " + result.gestureName +
                              " detected on axis=" + String(axisName(movementAxis)) +
                              " dir=" + String(swipeDirection, 2) +
                              " min=" + String(axisMin, 2) +
                              " max=" + String(axisMax, 2) +
                              " (conf: " + String(result.confidence, 2) + ")");
    return result;
}
