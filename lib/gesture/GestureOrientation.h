/*
 * ESP32 MacroPad Project
 *
 * Wrapper for the SensorFusion library to provide orientation tracking
 * for gesture recognition.
 */

#ifndef GESTURE_ORIENTATION_H
#define GESTURE_ORIENTATION_H

#include <Arduino.h>
#include "SensorFusion.h"

// Forward-declare to avoid circular dependency
class SensorFusion;

/**
 * Orientation tracker using Madgwick AHRS algorithm via SensorFusion library
 */
class GestureOrientation
{
public:
    GestureOrientation();

    void begin(const SensorFusionConfig& config);
    void update(const SensorFrame& frame, float deltaTime);
    void reset();
    void captureNeutralOrientation();

    const Quaternion& getCurrentOrientation() const;
    const Quaternion& getNeutralOrientation() const;
    Quaternion getRelativeOrientation() const;

    void getLocalAngularVelocity(float& localPitchVel, float& localYawVel, float& localRollVel) const;
    void getCurrentEulerAngles(float& pitch, float& roll, float& yaw) const;
    void getGyroBias(float& biasX, float& biasY, float& biasZ) const;
    void updateGyroBias(float deltaX, float deltaY, float deltaZ);

    const FilterState& getFilterState() const;
    bool isInitialized() const;
    bool hasNeutralOrientation() const;

private:
    SensorFusion fusion;
};

#endif // GESTURE_ORIENTATION_H
