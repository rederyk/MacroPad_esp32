/*
 * ESP32 MacroPad Project
 *
 * Wrapper for the SensorFusion library to provide orientation tracking
 * for gesture recognition.
 */

#include "GestureOrientation.h"

GestureOrientation::GestureOrientation() = default;

void GestureOrientation::begin(const SensorFusionConfig& config)
{
    fusion.begin(config);
}

void GestureOrientation::update(const SensorFrame& frame, float deltaTime)
{
    fusion.update(frame, deltaTime);
}

void GestureOrientation::reset()
{
    fusion.reset();
}

void GestureOrientation::captureNeutralOrientation()
{
    fusion.captureNeutralOrientation();
}

const Quaternion& GestureOrientation::getCurrentOrientation() const
{
    return fusion.getCurrentOrientation();
}

const Quaternion& GestureOrientation::getNeutralOrientation() const
{
    return fusion.getNeutralOrientation();
}

Quaternion GestureOrientation::getRelativeOrientation() const
{
    return fusion.getRelativeOrientation();
}

void GestureOrientation::getLocalAngularVelocity(float& localPitchVel, float& localYawVel, float& localRollVel) const
{
    fusion.getLocalAngularVelocity(localPitchVel, localYawVel, localRollVel);
}

void GestureOrientation::getCurrentEulerAngles(float& pitch, float& roll, float& yaw) const
{
    fusion.getCurrentEulerAngles(pitch, roll, yaw);
}

void GestureOrientation::getGyroBias(float& biasX, float& biasY, float& biasZ) const
{
    fusion.getGyroBias(biasX, biasY, biasZ);
}

void GestureOrientation::updateGyroBias(float deltaX, float deltaY, float deltaZ)
{
    fusion.updateGyroBias(deltaX, deltaY, deltaZ);
}

const FilterState& GestureOrientation::getFilterState() const
{
    return fusion.getFilterState();
}

bool GestureOrientation::isInitialized() const
{
    return fusion.isInitialized();
}

bool GestureOrientation::hasNeutralOrientation() const
{
    return fusion.hasNeutralOrientation();
}

