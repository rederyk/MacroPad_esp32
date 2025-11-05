/*
 * ESP32 MacroPad Project
 *
 * Quaternion-based orientation tracking with Madgwick AHRS filter
 * for robust gesture recognition.
 *
 * This system provides the same robust sensor fusion used in GyroMouse
 * to improve gesture detection accuracy and reduce false positives.
 */

#ifndef GESTURE_ORIENTATION_H
#define GESTURE_ORIENTATION_H

#include <Arduino.h>

/**
 * Quaternion representation for 3D rotations
 * Avoids gimbal lock and provides smooth interpolation
 */
struct Quaternion
{
    float w, x, y, z;  // w = scalar part, (x,y,z) = vector part

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
struct GestureSensorFrame
{
    float accelX, accelY, accelZ;      // Accelerometer (g)
    float gyroX, gyroY, gyroZ;          // Gyroscope (rad/s)
    float accelMagnitude;               // Pre-calculated magnitude
    bool gyroValid;                     // True if gyro data is available

    GestureSensorFrame()
        : accelX(0.0f), accelY(0.0f), accelZ(0.0f),
          gyroX(0.0f), gyroY(0.0f), gyroZ(0.0f),
          accelMagnitude(0.0f), gyroValid(false) {}
};

/**
 * Configuration for orientation tracking
 */
struct OrientationConfig
{
    float madgwickBeta;           // Filter gain (0.01-0.5, default 0.1)
    float orientationAlpha;       // Complementary filter alpha (0.9-0.999, default 0.96)
    float smoothing;              // Smoothing factor (0.0-0.95, default 0.3)
    bool useAdaptiveBeta;         // Enable adaptive beta based on motion

    OrientationConfig()
        : madgwickBeta(0.1f),
          orientationAlpha(0.96f),
          smoothing(0.3f),
          useAdaptiveBeta(true) {}
};

/**
 * Adaptive filtering state
 */
struct AdaptiveFilterState
{
    float velocityMagnitude;      // EMA of gyro magnitude
    float gyroNoiseEstimate;      // Estimated noise floor
    float adaptiveSmoothingFactor; // Current smoothing factor
    float currentMadgwickBeta;    // Current Madgwick beta

    AdaptiveFilterState()
        : velocityMagnitude(0.0f),
          gyroNoiseEstimate(0.05f),
          adaptiveSmoothingFactor(0.3f),
          currentMadgwickBeta(0.1f) {}
};

/**
 * Orientation tracker using Madgwick AHRS algorithm
 *
 * Provides robust sensor fusion of accelerometer and gyroscope data
 * to track device orientation in 3D space. Uses quaternions to avoid
 * gimbal lock and enable smooth rotation calculations.
 */
class GestureOrientation
{
public:
    GestureOrientation();

    /**
     * Initialize with configuration
     */
    void begin(const OrientationConfig& config);

    /**
     * Update orientation with new sensor data
     * @param frame Sensor data frame
     * @param deltaTime Time since last update (seconds)
     */
    void update(const GestureSensorFrame& frame, float deltaTime);

    /**
     * Reset to identity orientation (no rotation)
     */
    void reset();

    /**
     * Capture current orientation as neutral reference
     * Subsequent relative rotations will be calculated from this point
     */
    void captureNeutralOrientation();

    /**
     * Get current orientation quaternion
     */
    const Quaternion& getCurrentOrientation() const { return currentOrientation; }

    /**
     * Get neutral (reference) orientation quaternion
     */
    const Quaternion& getNeutralOrientation() const { return neutralOrientation; }

    /**
     * Get relative orientation (current rotation from neutral)
     * Returns: q_relative = q_neutral^-1 * q_current
     */
    Quaternion getRelativeOrientation() const;

    /**
     * Get angular velocity in local (neutral) reference frame
     * @param localPitchVel Output: pitch angular velocity (rad/s)
     * @param localYawVel Output: yaw angular velocity (rad/s)
     * @param localRollVel Output: roll angular velocity (rad/s)
     */
    void getLocalAngularVelocity(float& localPitchVel, float& localYawVel, float& localRollVel) const;

    /**
     * Get current Euler angles (for debugging/compatibility)
     */
    void getCurrentEulerAngles(float& pitch, float& roll, float& yaw) const;

    /**
     * Get gyro bias estimates
     */
    void getGyroBias(float& biasX, float& biasY, float& biasZ) const
    {
        biasX = gyroBiasX;
        biasY = gyroBiasY;
        biasZ = gyroBiasZ;
    }

    /**
     * Update gyro bias (for calibration)
     */
    void updateGyroBias(float deltaX, float deltaY, float deltaZ);

    /**
     * Get adaptive filter state (for debugging/tuning)
     */
    const AdaptiveFilterState& getFilterState() const { return filterState; }

    /**
     * Check if orientation is initialized
     */
    bool isInitialized() const { return initialized; }

    /**
     * Check if neutral has been captured
     */
    bool hasNeutralOrientation() const { return hasNeutral; }

private:
    // Configuration
    OrientationConfig config;

    // Current state
    Quaternion currentOrientation;    // Current absolute orientation
    Quaternion neutralOrientation;    // Reference orientation
    Quaternion lastOrientation;       // Previous frame orientation

    bool initialized;
    bool hasNeutral;

    // Gyro bias estimation
    float gyroBiasX, gyroBiasY, gyroBiasZ;

    // Adaptive filtering
    AdaptiveFilterState filterState;
    unsigned long lastMotionTime;

    // Madgwick filter state
    float madgwickSampleFreq;

    // Internal methods
    void madgwickUpdate(float gx, float gy, float gz,
                       float ax, float ay, float az,
                       float deltaTime);

    void updateAdaptiveFiltering(const GestureSensorFrame& frame, float deltaTime);
    void updateMadgwickBeta();

    Quaternion createQuaternionFromGyro(const GestureSensorFrame& frame, float deltaTime) const;
};

/**
 * Utility functions
 */
namespace GestureOrientationUtils
{
    constexpr float kRadToDeg = 57.2957795f;
    constexpr float kDegToRad = 1.0f / kRadToDeg;
    constexpr float kAccelReliableMin = 0.25f;  // g
    constexpr float kAccelReliableMax = 1.85f;  // g
    constexpr float kMinNoiseEstimate = 0.01f;
    constexpr float kMaxNoiseEstimate = 0.5f;

    /**
     * Check if accelerometer data is reliable (not during acceleration)
     */
    inline bool isAccelerometerReliable(float accelMagnitude)
    {
        return (accelMagnitude > kAccelReliableMin) &&
               (accelMagnitude < kAccelReliableMax);
    }

    /**
     * Apply dynamic deadzone with noise compensation
     */
    float applyDynamicDeadzone(float value, float baseThreshold, float noiseFactor);

    /**
     * Smooth value using exponential moving average
     */
    inline float applyEMA(float current, float target, float alpha)
    {
        return current + (target - current) * alpha;
    }
}

#endif // GESTURE_ORIENTATION_H
