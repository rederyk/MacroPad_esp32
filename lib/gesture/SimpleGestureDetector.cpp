/*
 * ESP32 MacroPad Project
 *
 * Simple and efficient gesture detection.
 */

#include "SimpleGestureDetector.h"
#include <Arduino.h>
#include <Logger.h>
#include <cmath>

namespace
{
    constexpr float kMinValidSampleMagnitude = 0.05f;  // Minimum accel magnitude to consider valid
    constexpr float kGyroToDegPerSec = 57.2957795f;    // Convert rad/s to deg/s

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

    // Find axis with maximum range and return stats
    struct AxisAnalysis
    {
        int axis;           // 0=X, 1=Y, 2=Z
        float min;
        float max;
        float range;
        bool crossedZero;   // For shake detection
    };

    AxisAnalysis analyzeAccelAxis(const SampleBuffer *buffer)
    {
        AxisAnalysis result = {-1, 0.0f, 0.0f, 0.0f, false};

        if (!buffer || buffer->sampleCount < 3)
            return result;

        // Track min/max for each axis
        float minVals[3] = {1e6f, 1e6f, 1e6f};
        float maxVals[3] = {-1e6f, -1e6f, -1e6f};
        bool hasPositive[3] = {false, false, false};
        bool hasNegative[3] = {false, false, false};

        for (uint16_t i = 0; i < buffer->sampleCount; i++)
        {
            const Sample &s = buffer->samples[i];
            if (!isSampleValid(s) || accelMagnitude(s) < kMinValidSampleMagnitude)
                continue;

            float vals[3] = {s.x, s.y, s.z};
            for (int axis = 0; axis < 3; axis++)
            {
                if (vals[axis] < minVals[axis]) minVals[axis] = vals[axis];
                if (vals[axis] > maxVals[axis]) maxVals[axis] = vals[axis];
                if (vals[axis] > 0.2f) hasPositive[axis] = true;
                if (vals[axis] < -0.2f) hasNegative[axis] = true;
            }
        }

        // Find axis with maximum range
        for (int axis = 0; axis < 3; axis++)
        {
            float range = maxVals[axis] - minVals[axis];
            if (range > result.range)
            {
                result.axis = axis;
                result.min = minVals[axis];
                result.max = maxVals[axis];
                result.range = range;
                result.crossedZero = hasPositive[axis] && hasNegative[axis];
            }
        }

        return result;
    }

    // Count direction changes (zero crossings) for shake detection
    int countDirectionChanges(const SampleBuffer *buffer, int axis)
    {
        if (!buffer || buffer->sampleCount < 3)
            return 0;

        int changes = 0;
        bool wasPositive = false;
        bool firstValid = true;
        constexpr float kNoiseThreshold = 30.0f;  // deg/s - ignore small noise

        for (uint16_t i = 0; i < buffer->sampleCount; i++)
        {
            const Sample &s = buffer->samples[i];
            if (!s.gyroValid || !isSampleValid(s))
                continue;

            float vals[3] = {
                s.gyroX * kGyroToDegPerSec,
                s.gyroY * kGyroToDegPerSec,
                s.gyroZ * kGyroToDegPerSec
            };

            // Ignore samples below noise threshold
            if (fabsf(vals[axis]) < kNoiseThreshold)
                continue;

            bool isPositive = vals[axis] > 0.0f;

            if (firstValid)
            {
                wasPositive = isPositive;
                firstValid = false;
            }
            else if (isPositive != wasPositive)
            {
                changes++;
                wasPositive = isPositive;
            }
        }

        return changes;
    }

    AxisAnalysis analyzeGyroAxis(const SampleBuffer *buffer)
    {
        AxisAnalysis result = {-1, 0.0f, 0.0f, 0.0f, false};

        if (!buffer || buffer->sampleCount < 3)
            return result;

        // Track min/max gyro for each axis (in deg/s)
        float minVals[3] = {1e6f, 1e6f, 1e6f};
        float maxVals[3] = {-1e6f, -1e6f, -1e6f};
        float peakVals[3] = {0.0f, 0.0f, 0.0f};  // Track absolute peak
        float maxPositive[3] = {0.0f, 0.0f, 0.0f};  // Track max positive value
        float maxNegative[3] = {0.0f, 0.0f, 0.0f};  // Track max negative value (abs)

        uint16_t validSamples = 0;

        for (uint16_t i = 0; i < buffer->sampleCount; i++)
        {
            const Sample &s = buffer->samples[i];
            if (!s.gyroValid || !isSampleValid(s))
                continue;

            float vals[3] = {
                s.gyroX * kGyroToDegPerSec,
                s.gyroY * kGyroToDegPerSec,
                s.gyroZ * kGyroToDegPerSec
            };

            for (int axis = 0; axis < 3; axis++)
            {
                if (vals[axis] < minVals[axis]) minVals[axis] = vals[axis];
                if (vals[axis] > maxVals[axis]) maxVals[axis] = vals[axis];

                float absVal = fabsf(vals[axis]);
                if (absVal > peakVals[axis]) peakVals[axis] = absVal;

                // Track peaks in each direction separately
                if (vals[axis] > maxPositive[axis]) maxPositive[axis] = vals[axis];
                if (vals[axis] < 0.0f && fabsf(vals[axis]) > maxNegative[axis])
                    maxNegative[axis] = fabsf(vals[axis]);
            }

            validSamples++;
        }

        if (validSamples < 2)
            return result;

        // Find axis with maximum peak velocity
        for (int axis = 0; axis < 3; axis++)
        {
            if (peakVals[axis] > result.range)
            {
                result.axis = axis;
                result.min = minVals[axis];
                result.max = maxVals[axis];
                result.range = peakVals[axis];  // Store peak as "range"

                // For shake: require at least 3 direction changes (not just 2 peaks)
                int dirChanges = countDirectionChanges(buffer, axis);
                result.crossedZero = (dirChanges >= 3);
            }
        }

        return result;
    }

    const char *axisName(int axis)
    {
        switch (axis)
        {
        case 0: return "X";
        case 1: return "Y";
        case 2: return "Z";
        default: return "?";
        }
    }
}

GestureRecognitionResult detectSimpleGesture(SampleBuffer *buffer, const SimpleGestureConfig &config)
{
    GestureRecognitionResult result;
    result.sensorMode = config.sensorMode;

    if (!buffer || buffer->sampleCount < 3 || buffer->samples == nullptr)
    {
        Logger::getInstance().log(String(config.sensorTag) + ": insufficient samples");
        return result;
    }

    Logger::getInstance().log(String(config.sensorTag) + ": analyzing " + String(buffer->sampleCount) + " samples");

    // Check if we have gyro data
    bool hasGyro = false;
    if (config.useGyro)
    {
        for (uint16_t i = 0; i < buffer->sampleCount; i++)
        {
            if (buffer->samples[i].gyroValid)
            {
                hasGyro = true;
                break;
            }
        }
    }

    if (hasGyro)
    {
        // === GYRO-BASED DETECTION (MPU6050) ===
        AxisAnalysis gyro = analyzeGyroAxis(buffer);

        if (gyro.axis < 0)
        {
            Logger::getInstance().log(String(config.sensorTag) + ": no valid gyro data");
            return result;
        }

        int dirChanges = countDirectionChanges(buffer, gyro.axis);
        Logger::getInstance().log(String(config.sensorTag) + ": gyro " +
                                  String(axisName(gyro.axis)) + " peak=" + String(gyro.range, 1) + " deg/s" +
                                  " [" + String(gyro.min, 1) + " to " + String(gyro.max, 1) + "]" +
                                  " dirChanges=" + String(dirChanges) +
                                  " shake=" + (gyro.crossedZero ? "YES" : "NO"));

        // Detect shake: bidirectional motion with high peak AND at least 3 direction changes
        if (gyro.crossedZero && gyro.range >= config.gyroShakeThreshold)
        {
            result.gestureID = 203;  // G_SHAKE
            result.gestureName = "G_SHAKE";
            result.confidence = std::min(1.0f, gyro.range / (config.gyroShakeThreshold * 2.0f));

            Logger::getInstance().log(String(config.sensorTag) + ": SHAKE detected (gyro peak=" +
                                      String(gyro.range, 1) + " deg/s, dirChanges=" + String(dirChanges) +
                                      ", conf=" + String(result.confidence, 2) + ")");
            return result;
        }

        // Detect swipe: unidirectional motion with moderate peak (less than 3 direction changes)
        if (dirChanges < 3 && gyro.range >= config.gyroSwipeThreshold)
        {
            // Calculate jerk (rate of change of acceleration) for better swipe detection
            float maxJerk = 0.0f;
            for (uint16_t i = 1; i < buffer->sampleCount; i++)
            {
                const Sample &s1 = buffer->samples[i - 1];
                const Sample &s2 = buffer->samples[i];
                if (!s1.gyroValid || !s2.gyroValid)
                    continue;

                float vals1[3] = {s1.gyroX * kGyroToDegPerSec, s1.gyroY * kGyroToDegPerSec, s1.gyroZ * kGyroToDegPerSec};
                float vals2[3] = {s2.gyroX * kGyroToDegPerSec, s2.gyroY * kGyroToDegPerSec, s2.gyroZ * kGyroToDegPerSec};

                float jerk = fabsf(vals2[gyro.axis] - vals1[gyro.axis]);
                if (jerk > maxJerk)
                    maxJerk = jerk;
            }

            // Determine direction from sign of dominant motion
            bool isPositive = (gyro.max > fabsf(gyro.min));
            result.gestureID = isPositive ? 201 : 202;  // G_SWIPE_RIGHT or G_SWIPE_LEFT
            result.gestureName = isPositive ? "G_SWIPE_RIGHT" : "G_SWIPE_LEFT";

            // Confidence based on both peak velocity and jerk
            float peakScore = gyro.range / (config.gyroSwipeThreshold * 2.0f);
            float jerkScore = maxJerk / 100.0f;  // Normalize jerk
            result.confidence = std::min(1.0f, (peakScore * 0.7f + jerkScore * 0.3f));

            Logger::getInstance().log(String(config.sensorTag) + ": " + result.gestureName +
                                      " detected (gyro peak=" + String(gyro.range, 1) +
                                      " deg/s, jerk=" + String(maxJerk, 1) +
                                      ", dir=" + (isPositive ? "+" : "-") +
                                      ", conf=" + String(result.confidence, 2) + ")");
            return result;
        }

        Logger::getInstance().log(String(config.sensorTag) + ": no gesture (gyro peak too low or ambiguous motion)");
    }
    else
    {
        // === ACCEL-BASED DETECTION (ADXL345) ===
        AxisAnalysis accel = analyzeAccelAxis(buffer);

        if (accel.axis < 0)
        {
            Logger::getInstance().log(String(config.sensorTag) + ": no valid accel data");
            return result;
        }

        Logger::getInstance().log(String(config.sensorTag) + ": accel range " +
                                  String(axisName(accel.axis)) + "=" + String(accel.range, 2) + "g" +
                                  " crossed_zero=" + (accel.crossedZero ? "YES" : "NO"));

        // Detect shake: bidirectional motion
        if (accel.crossedZero && accel.range >= config.accelShakeThreshold)
        {
            result.gestureID = 203;  // G_SHAKE
            result.gestureName = "G_SHAKE";
            result.confidence = std::min(1.0f, accel.range / (config.accelShakeThreshold * 2.0f));

            Logger::getInstance().log(String(config.sensorTag) + ": SHAKE detected (accel range=" +
                                      String(accel.range, 2) + "g, conf=" + String(result.confidence, 2) + ")");
            return result;
        }

        // Detect swipe: unidirectional motion
        if (!accel.crossedZero && accel.range >= config.accelSwipeThreshold)
        {
            bool isPositive = (accel.max > fabsf(accel.min));
            result.gestureID = isPositive ? 201 : 202;  // G_SWIPE_RIGHT or G_SWIPE_LEFT
            result.gestureName = isPositive ? "G_SWIPE_RIGHT" : "G_SWIPE_LEFT";
            result.confidence = std::min(1.0f, accel.range / (config.accelSwipeThreshold * 2.0f));

            Logger::getInstance().log(String(config.sensorTag) + ": " + result.gestureName +
                                      " detected (accel range=" + String(accel.range, 2) +
                                      "g, dir=" + (isPositive ? "+" : "-") +
                                      ", conf=" + String(result.confidence, 2) + ")");
            return result;
        }

        Logger::getInstance().log(String(config.sensorTag) + ": no gesture (accel range too low)");
    }

    return result;
}
