/*
 * ESP32 MacroPad Project
 *
 * Quaternion-based orientation tracking with Madgwick AHRS filter
 * for robust sensor fusion.
 */

#ifndef SENSOR_FUSION_H
#define SENSOR_FUSION_H

#include <Arduino.h>

/**
 * Quaternion representation for 3D rotations
 */
struct Quaternion
{
    float w, x, y, z;

    Quaternion() : w(1.0f), x(0.0f), y(0.0f), z(0.0f) {}
    Quaternion(float w_, float x_, float y_, float z_) : w(w_), x(x_), y(y_), z(z_) {}

    void normalize();
    Quaternion conjugate() const;
    Quaternion multiply(const Quaternion& q) const;
    void rotateVector(float& vx, float& vy, float& vz) const;
    void toEuler(float& pitch, float& roll, float& yaw) const;
};

/**
 * Sensor frame containing all IMU data for one sample
 */
struct SensorFrame
{
    float accelX, accelY, accelZ;
    float gyroX, gyroY, gyroZ;
    float accelMagnitude;
    bool gyroValid;

    SensorFrame()
        : accelX(0.0f), accelY(0.0f), accelZ(0.0f),
          gyroX(0.0f), gyroY(0.0f), gyroZ(0.0f),
          accelMagnitude(0.0f), gyroValid(false) {}
};

/**
 * Configuration for orientation tracking
 */
struct SensorFusionConfig
{
    float madgwickBeta;
    float orientationAlpha;
    float smoothing;
    bool useAdaptiveBeta;

    SensorFusionConfig()
        : madgwickBeta(0.1f),
          orientationAlpha(0.96f),
          smoothing(0.3f),
          useAdaptiveBeta(true) {}
};

/**
 * Adaptive filtering state
 */
struct FilterState
{
    float velocityMagnitude;
    float gyroNoiseEstimate;
    float adaptiveSmoothingFactor;
    float currentMadgwickBeta;

    FilterState()
        : velocityMagnitude(0.0f),
          gyroNoiseEstimate(0.05f),
          adaptiveSmoothingFactor(0.3f),
          currentMadgwickBeta(0.1f) {}
};

/**
 * Orientation tracker using Madgwick AHRS algorithm
 */
class SensorFusion
{
public:
    SensorFusion();

    void begin(const SensorFusionConfig& config);
    void update(const SensorFrame& frame, float deltaTime);
    void reset();
    void captureNeutralOrientation();

    const Quaternion& getCurrentOrientation() const { return currentOrientation; }
    const Quaternion& getNeutralOrientation() const { return neutralOrientation; }
    Quaternion getRelativeOrientation() const;

    void getLocalAngularVelocity(float& localPitchVel, float& localYawVel, float& localRollVel) const;
    void getCurrentEulerAngles(float& pitch, float& roll, float& yaw) const;
    void getGyroBias(float& biasX, float& biasY, float& biasZ) const;
    void updateGyroBias(float deltaX, float deltaY, float deltaZ);

    const FilterState& getFilterState() const { return filterState; }
    bool isInitialized() const { return initialized; }
    bool hasNeutralOrientation() const { return hasNeutral; }

private:
    SensorFusionConfig config;
    Quaternion currentOrientation;
    Quaternion neutralOrientation;
    Quaternion lastOrientation;
    bool initialized;
    bool hasNeutral;
    float gyroBiasX, gyroBiasY, gyroBiasZ;
    FilterState filterState;
    unsigned long lastMotionTime;
    float madgwickSampleFreq;

    void madgwickUpdate(float gx, float gy, float gz,
                       float ax, float ay, float az,
                       float deltaTime);
    void updateAdaptiveFiltering(const SensorFrame& frame, float deltaTime);
    void updateMadgwickBeta();
    Quaternion createQuaternionFromGyro(const SensorFrame& frame, float deltaTime) const;
};

/**
 * Utility functions
 */
namespace SensorFusionUtils
{
    constexpr float kRadToDeg = 57.2957795f;
    constexpr float kDegToRad = 1.0f / kRadToDeg;
    constexpr float kAccelReliableMin = 0.25f;
    constexpr float kAccelReliableMax = 1.85f;
    constexpr float kMinNoiseEstimate = 0.01f;
    constexpr float kMaxNoiseEstimate = 0.5f;

    inline bool isAccelerometerReliable(float accelMagnitude)
    {
        return (accelMagnitude > kAccelReliableMin) &&
               (accelMagnitude < kAccelReliableMax);
    }

    float applyDynamicDeadzone(float value, float baseThreshold, float noiseFactor);

    inline float applyEMA(float current, float target, float alpha)
    {
        return current + (target - current) * alpha;
    }
}

#endif // SENSOR_FUSION_H
