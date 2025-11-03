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
    constexpr float kOrientationDetectionFloor = 0.35f;
    constexpr float kMinValidSampleMagnitude = 0.05f;
    constexpr uint16_t kOrientationSampleWindow = 6;

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

    bool isSampleValid(const Sample &sample)
    {
        return std::isfinite(sample.x) &&
               std::isfinite(sample.y) &&
               std::isfinite(sample.z);
    }

    float accelMagnitude(const Sample &sample)
    {
        return sqrtf(sample.x * sample.x + sample.y * sample.y + sample.z * sample.z);
    }

    OrientationInfo detectOrientation(const SampleBuffer *buffer)
    {
        OrientationInfo info;

        if (!buffer || !buffer->samples || buffer->sampleCount == 0)
        {
            return info;
        }

        float totals[3] = {0.0f, 0.0f, 0.0f};
        uint16_t considered = 0;
        const uint16_t limit = std::min<uint16_t>(buffer->sampleCount, kOrientationSampleWindow);

        for (uint16_t i = 0; i < limit; ++i)
        {
            const Sample &sample = buffer->samples[i];
            if (!isSampleValid(sample))
            {
                continue;
            }

            if (accelMagnitude(sample) < kMinValidSampleMagnitude)
            {
                continue;
            }

            totals[0] += fabsf(sample.x);
            totals[1] += fabsf(sample.y);
            totals[2] += fabsf(sample.z);
            considered++;
        }

        if (considered == 0)
        {
            return info;
        }

        const float avgX = totals[0] / considered;
        const float avgY = totals[1] / considered;
        const float avgZ = totals[2] / considered;

        int axis = 0;
        float best = avgX;
        if (avgY > best)
        {
            best = avgY;
            axis = 1;
        }
        if (avgZ > best)
        {
            best = avgZ;
            axis = 2;
        }

        if (best > kOrientationDetectionFloor)
        {
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

    bool gyroActive = config.useGyro;
    if (gyroActive)
    {
        gyroActive = false;
        for (uint16_t i = 0; i < buffer->sampleCount; ++i)
        {
            if (buffer->samples[i].gyroValid)
            {
                gyroActive = true;
                break;
            }
        }
    }

    const OrientationInfo orientation = detectOrientation(buffer);

    AxisStats accelStats[3];
    AxisStats gyroStats[3];
    bool accelSeeded = false;
    bool gyroSeeded = false;

    float maxAccelMagnitude = 0.0f;
    float maxGyroMagnitude = 0.0f;

    for (uint16_t i = 0; i < buffer->sampleCount; i++)
    {
        const Sample &sample = buffer->samples[i];

        if (!isSampleValid(sample))
        {
            continue;
        }

        const float accelMag = accelMagnitude(sample);
        if (accelMag < kMinValidSampleMagnitude)
        {
            continue;
        }

        if (!accelSeeded)
        {
            accelStats[0] = AxisStats(sample.x);
            accelStats[1] = AxisStats(sample.y);
            accelStats[2] = AxisStats(sample.z);
            accelSeeded = true;
        }
        else
        {
            accelStats[0].update(sample.x);
            accelStats[1].update(sample.y);
            accelStats[2].update(sample.z);
        }

        if (accelMag > maxAccelMagnitude)
        {
            maxAccelMagnitude = accelMag;
        }

        if (gyroActive && sample.gyroValid)
        {
            if (!gyroSeeded)
            {
                gyroStats[0] = AxisStats(sample.gyroX);
                gyroStats[1] = AxisStats(sample.gyroY);
                gyroStats[2] = AxisStats(sample.gyroZ);
                gyroSeeded = true;
            }
            else
            {
                gyroStats[0].update(sample.gyroX);
                gyroStats[1].update(sample.gyroY);
                gyroStats[2].update(sample.gyroZ);
            }

            const float gyroMag = sqrtf(sample.gyroX * sample.gyroX +
                                        sample.gyroY * sample.gyroY +
                                        sample.gyroZ * sample.gyroZ);
            if (gyroMag > maxGyroMagnitude)
            {
                maxGyroMagnitude = gyroMag;
            }
        }
    }

    if (!accelSeeded)
    {
        Logger::getInstance().log(String(config.sensorTag) + ": no valid accelerometer data in buffer");
        return result;
    }

    if (gyroActive && !gyroSeeded)
    {
        gyroActive = false;
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

    const float swipeThresholdBase = (config.swipeAccelThreshold > 0.0f) ? config.swipeAccelThreshold : kDefaultSwipeThreshold;
    float effectiveSwipeThreshold = swipeThresholdBase;

    if (gyroActive)
    {
        effectiveSwipeThreshold *= (maxGyroMagnitude > 2.5f) ? 0.7f : 0.85f;
    }

    if (buffer->sampleCount >= 12 || maxAccelMagnitude > 1.5f)
    {
        effectiveSwipeThreshold *= 0.85f;
    }

    effectiveSwipeThreshold = std::max(0.35f, std::min(effectiveSwipeThreshold, swipeThresholdBase));

    const float shakeMinPositive = (config.shakeBidirectionalMax > 0.0f) ? config.shakeBidirectionalMax : kDefaultShakeMinPeak;
    const float shakeMinNegative = (config.shakeBidirectionalMin > 0.0f) ? config.shakeBidirectionalMin : kDefaultShakeMinPeak;
    const float shakeRange = (config.shakeRangeThreshold > 0.0f) ? config.shakeRangeThreshold : kDefaultShakeRange;
    const float shakeMinPositiveAdjusted = shakeMinPositive * 0.9f;
    const float shakeMinNegativeAdjusted = shakeMinNegative * 0.9f;
    const float shakeRangeAdjusted = shakeRange * 0.9f;

    float maxMovement = 0.0f;
    int movementAxis = -1;

    for (int axis = 0; axis < 3; axis++)
    {
        if (orientation.gravity[axis])
        {
            continue;
        }

        const float range = accelStats[axis].range();
        if (range > maxMovement && range > effectiveSwipeThreshold)
        {
            maxMovement = range;
            movementAxis = axis;
        }
    }

    if (movementAxis < 0 && gyroActive && maxGyroMagnitude > 2.0f)
    {
        for (int axis = 0; axis < 3; axis++)
        {
            if (orientation.gravity[axis])
            {
                continue;
            }

            const float range = accelStats[axis].range();
            if (range > maxMovement && range > (effectiveSwipeThreshold * 0.75f))
            {
                maxMovement = range;
                movementAxis = axis;
            }
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

    const bool isShake = (axisMin < -shakeMinNegativeAdjusted) &&
                         (axisMax > shakeMinPositiveAdjusted) &&
                         (axisRange > shakeRangeAdjusted);

    if (isShake)
    {
        result.gestureID = 203;
        result.gestureName = "G_SHAKE";
        result.confidence = clampMinMax(axisRange / (shakeRangeAdjusted + 0.01f), 0.5f, 1.0f);

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
    const float confidenceDenominator = std::max(effectiveSwipeThreshold * 2.0f, 0.1f);
    result.confidence = clampMinMax(axisRange / confidenceDenominator, 0.5f, 1.0f);

    Logger::getInstance().log(String(config.sensorTag) + ": " + result.gestureName +
                              " detected on axis=" + String(axisName(movementAxis)) +
                              " dir=" + String(swipeDirection, 2) +
                              " min=" + String(axisMin, 2) +
                              " max=" + String(axisMax, 2) +
                              " (conf: " + String(result.confidence, 2) + ")");
    return result;
}
