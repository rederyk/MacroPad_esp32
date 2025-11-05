/*
 * ESP32 MacroPad Project
 *
 * Implementation of quaternion-based orientation tracking with Madgwick AHRS
 */

#include "GestureOrientation.h"
#include <cmath>
#include <algorithm>

// ============================================================================
// Quaternion Implementation
// ============================================================================

void Quaternion::normalize()
{
    float norm = sqrtf(w * w + x * x + y * y + z * z);
    if (norm > 1e-6f)
    {
        float invNorm = 1.0f / norm;
        w *= invNorm;
        x *= invNorm;
        y *= invNorm;
        z *= invNorm;
    }
}

Quaternion Quaternion::conjugate() const
{
    return Quaternion(w, -x, -y, -z);
}

Quaternion Quaternion::multiply(const Quaternion& q) const
{
    return Quaternion(
        w * q.w - x * q.x - y * q.y - z * q.z,
        w * q.x + x * q.w + y * q.z - z * q.y,
        w * q.y - x * q.z + y * q.w + z * q.x,
        w * q.z + x * q.y - y * q.x + z * q.w
    );
}

void Quaternion::rotateVector(float& vx, float& vy, float& vz) const
{
    // v' = q * v * q^-1
    // Optimized version for unit quaternion
    float qx = x, qy = y, qz = z, qw = w;

    // First rotation: q * v
    float tx = qw * vx + qy * vz - qz * vy;
    float ty = qw * vy + qz * vx - qx * vz;
    float tz = qw * vz + qx * vy - qy * vx;
    float tw = -qx * vx - qy * vy - qz * vz;

    // Second rotation: result * q^-1 (conjugate)
    vx = tw * (-qx) + tx * qw + ty * (-qz) - tz * (-qy);
    vy = tw * (-qy) + ty * qw + tz * (-qx) - tx * (-qz);
    vz = tw * (-qz) + tz * qw + tx * (-qy) - ty * (-qx);
}

void Quaternion::toEuler(float& pitch, float& roll, float& yaw) const
{
    // Convert quaternion to Euler angles (Tait-Bryan angles)

    // roll (x-axis rotation)
    float sinr_cosp = 2.0f * (w * x + y * z);
    float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
    roll = atan2f(sinr_cosp, cosr_cosp);

    // pitch (y-axis rotation)
    float sinp = 2.0f * (w * y - z * x);
    if (fabsf(sinp) >= 1.0f)
        pitch = copysignf(M_PI / 2.0f, sinp); // use 90 degrees if out of range
    else
        pitch = asinf(sinp);

    // yaw (z-axis rotation)
    float siny_cosp = 2.0f * (w * z + x * y);
    float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
    yaw = atan2f(siny_cosp, cosy_cosp);
}

// ============================================================================
// GestureOrientation Implementation
// ============================================================================

GestureOrientation::GestureOrientation()
    : initialized(false),
      hasNeutral(false),
      gyroBiasX(0.0f),
      gyroBiasY(0.0f),
      gyroBiasZ(0.0f),
      madgwickSampleFreq(100.0f),
      lastMotionTime(0)
{
}

void GestureOrientation::begin(const OrientationConfig& cfg)
{
    config = cfg;

    // Validate and clamp configuration
    config.madgwickBeta = constrain(config.madgwickBeta, 0.01f, 0.5f);
    config.orientationAlpha = constrain(config.orientationAlpha, 0.0f, 0.999f);
    config.smoothing = constrain(config.smoothing, 0.0f, 0.95f);

    // Initialize filter state
    filterState.currentMadgwickBeta = config.madgwickBeta;
    filterState.adaptiveSmoothingFactor = config.smoothing;
    filterState.gyroNoiseEstimate = 0.05f;
    filterState.velocityMagnitude = 0.0f;

    // Reset orientation to identity
    reset();

    initialized = true;
}

void GestureOrientation::reset()
{
    currentOrientation = Quaternion();
    lastOrientation = Quaternion();
    neutralOrientation = Quaternion();

    hasNeutral = false;

    gyroBiasX = 0.0f;
    gyroBiasY = 0.0f;
    gyroBiasZ = 0.0f;

    filterState.velocityMagnitude = 0.0f;
    filterState.gyroNoiseEstimate = 0.05f;
    filterState.adaptiveSmoothingFactor = config.smoothing;
    filterState.currentMadgwickBeta = config.madgwickBeta;

    lastMotionTime = millis();
}

void GestureOrientation::update(const GestureSensorFrame& frame, float deltaTime)
{
    if (!initialized)
    {
        return;
    }

    // Clamp deltaTime to reasonable range
    if (deltaTime > 0.1f || deltaTime <= 0.0f)
    {
        deltaTime = 0.01f; // Assume 100Hz
    }

    // Update adaptive filtering parameters
    updateAdaptiveFiltering(frame, deltaTime);

    // Store previous orientation
    lastOrientation = currentOrientation;

    // Update Madgwick beta if adaptive mode enabled
    if (config.useAdaptiveBeta)
    {
        updateMadgwickBeta();
    }

    if (frame.gyroValid)
    {
        // Apply bias-corrected gyro values
        float gx = frame.gyroX - gyroBiasX;
        float gy = frame.gyroY - gyroBiasY;
        float gz = frame.gyroZ - gyroBiasZ;

        // Check if accelerometer data is reliable
        bool accelReliable = GestureOrientationUtils::isAccelerometerReliable(frame.accelMagnitude);

        if (accelReliable)
        {
            // Use Madgwick filter with accelerometer correction
            madgwickUpdate(gx, gy, gz, frame.accelX, frame.accelY, frame.accelZ, deltaTime);
        }
        else
        {
            // Gyro-only update when accelerometer is unreliable
            // (e.g., during acceleration or vibration)
            Quaternion deltaQ = createQuaternionFromGyro(frame, deltaTime);
            currentOrientation = currentOrientation.multiply(deltaQ);
            currentOrientation.normalize();
        }
    }
    else
    {
        // Accelerometer-only mode (fallback)
        // Use simple complementary filter
        if (GestureOrientationUtils::isAccelerometerReliable(frame.accelMagnitude))
        {
            float pitchAcc = atan2f(-frame.accelX,
                                   sqrtf(frame.accelY * frame.accelY + frame.accelZ * frame.accelZ));
            float rollAcc = atan2f(frame.accelY, frame.accelZ);

            // Convert to quaternion
            float cr = cosf(rollAcc * 0.5f);
            float sr = sinf(rollAcc * 0.5f);
            float cp = cosf(pitchAcc * 0.5f);
            float sp = sinf(pitchAcc * 0.5f);

            Quaternion accelOrientation(
                cr * cp,
                sr * cp,
                cr * sp,
                -sr * sp
            );

            // Simple blend with previous orientation
            // This is a simplified version - proper SLERP would be better
            float alpha = config.orientationAlpha;
            currentOrientation.w = alpha * currentOrientation.w + (1.0f - alpha) * accelOrientation.w;
            currentOrientation.x = alpha * currentOrientation.x + (1.0f - alpha) * accelOrientation.x;
            currentOrientation.y = alpha * currentOrientation.y + (1.0f - alpha) * accelOrientation.y;
            currentOrientation.z = alpha * currentOrientation.z + (1.0f - alpha) * accelOrientation.z;
            currentOrientation.normalize();
        }
    }
}

void GestureOrientation::captureNeutralOrientation()
{
    neutralOrientation = currentOrientation;
    hasNeutral = true;
}

Quaternion GestureOrientation::getRelativeOrientation() const
{
    if (!hasNeutral)
    {
        return Quaternion(); // Identity
    }

    // q_relative = q_neutral^-1 * q_current
    return neutralOrientation.conjugate().multiply(currentOrientation);
}

void GestureOrientation::getLocalAngularVelocity(float& localPitchVel, float& localYawVel, float& localRollVel) const
{
    if (!hasNeutral)
    {
        localPitchVel = localYawVel = localRollVel = 0.0f;
        return;
    }

    // Calculate relative orientation in neutral frame
    Quaternion relativeOrientation = getRelativeOrientation();

    // Extract Euler angles from relative quaternion
    float pitch, roll, yaw;
    relativeOrientation.toEuler(pitch, roll, yaw);

    // These represent the current rotation from neutral
    // For velocity, we'd need to track change over time
    // This is a simplified version - full implementation would calculate
    // angular velocity from quaternion derivative
    localPitchVel = pitch;
    localYawVel = yaw;
    localRollVel = roll;
}

void GestureOrientation::getCurrentEulerAngles(float& pitch, float& roll, float& yaw) const
{
    currentOrientation.toEuler(pitch, roll, yaw);
}

void GestureOrientation::updateGyroBias(float deltaX, float deltaY, float deltaZ)
{
    gyroBiasX += deltaX;
    gyroBiasY += deltaY;
    gyroBiasZ += deltaZ;
}

Quaternion GestureOrientation::createQuaternionFromGyro(const GestureSensorFrame& frame, float deltaTime) const
{
    // Create quaternion from angular velocity (axis-angle representation)
    float gx = (frame.gyroX - gyroBiasX) * deltaTime;
    float gy = (frame.gyroY - gyroBiasY) * deltaTime;
    float gz = (frame.gyroZ - gyroBiasZ) * deltaTime;

    float angle = sqrtf(gx * gx + gy * gy + gz * gz);

    if (angle < 1e-6f)
    {
        return Quaternion(1.0f, 0.0f, 0.0f, 0.0f);
    }

    float halfAngle = angle * 0.5f;
    float sinHalfAngle = sinf(halfAngle);
    float invAngle = 1.0f / angle;

    return Quaternion(
        cosf(halfAngle),
        gx * invAngle * sinHalfAngle,
        gy * invAngle * sinHalfAngle,
        gz * invAngle * sinHalfAngle
    );
}

void GestureOrientation::madgwickUpdate(float gx, float gy, float gz,
                                       float ax, float ay, float az,
                                       float deltaTime)
{
    // Madgwick AHRS algorithm
    // Based on: https://x-io.co.uk/open-source-imu-and-ahrs-algorithms/

    // Update sample frequency
    if (deltaTime > 1e-6f)
    {
        madgwickSampleFreq = 1.0f / deltaTime;
    }

    float beta = filterState.currentMadgwickBeta;

    // Rate of change of quaternion from gyroscope
    float qDot1 = 0.5f * (-currentOrientation.x * gx - currentOrientation.y * gy - currentOrientation.z * gz);
    float qDot2 = 0.5f * (currentOrientation.w * gx + currentOrientation.y * gz - currentOrientation.z * gy);
    float qDot3 = 0.5f * (currentOrientation.w * gy - currentOrientation.x * gz + currentOrientation.z * gx);
    float qDot4 = 0.5f * (currentOrientation.w * gz + currentOrientation.x * gy - currentOrientation.y * gx);

    // Compute feedback only if accelerometer measurement valid (avoids NaN in accelerometer normalisation)
    if (!((ax == 0.0f) && (ay == 0.0f) && (az == 0.0f)))
    {
        // Normalise accelerometer measurement
        float recipNorm = 1.0f / sqrtf(ax * ax + ay * ay + az * az);
        ax *= recipNorm;
        ay *= recipNorm;
        az *= recipNorm;

        // Auxiliary variables to avoid repeated arithmetic
        float _2q0 = 2.0f * currentOrientation.w;
        float _2q1 = 2.0f * currentOrientation.x;
        float _2q2 = 2.0f * currentOrientation.y;
        float _2q3 = 2.0f * currentOrientation.z;
        float _4q0 = 4.0f * currentOrientation.w;
        float _4q1 = 4.0f * currentOrientation.x;
        float _4q2 = 4.0f * currentOrientation.y;
        float _8q1 = 8.0f * currentOrientation.x;
        float _8q2 = 8.0f * currentOrientation.y;
        float q0q0 = currentOrientation.w * currentOrientation.w;
        float q1q1 = currentOrientation.x * currentOrientation.x;
        float q2q2 = currentOrientation.y * currentOrientation.y;
        float q3q3 = currentOrientation.z * currentOrientation.z;

        // Gradient descent algorithm corrective step
        float s0 = _4q0 * q2q2 + _2q2 * ax + _4q0 * q1q1 - _2q1 * ay;
        float s1 = _4q1 * q3q3 - _2q3 * ax + 4.0f * q0q0 * currentOrientation.x - _2q0 * ay - _4q1 + _8q1 * q1q1 + _8q1 * q2q2 + _4q1 * az;
        float s2 = 4.0f * q0q0 * currentOrientation.y + _2q0 * ax + _4q2 * q3q3 - _2q3 * ay - _4q2 + _8q2 * q1q1 + _8q2 * q2q2 + _4q2 * az;
        float s3 = 4.0f * q1q1 * currentOrientation.z - _2q1 * ax + 4.0f * q2q2 * currentOrientation.z - _2q2 * ay;

        // Normalise step magnitude
        recipNorm = 1.0f / sqrtf(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3);
        s0 *= recipNorm;
        s1 *= recipNorm;
        s2 *= recipNorm;
        s3 *= recipNorm;

        // Apply feedback step
        qDot1 -= beta * s0;
        qDot2 -= beta * s1;
        qDot3 -= beta * s2;
        qDot4 -= beta * s3;
    }

    // Integrate rate of change of quaternion to yield quaternion
    currentOrientation.w += qDot1 * deltaTime;
    currentOrientation.x += qDot2 * deltaTime;
    currentOrientation.y += qDot3 * deltaTime;
    currentOrientation.z += qDot4 * deltaTime;

    // Normalise quaternion
    currentOrientation.normalize();
}

void GestureOrientation::updateAdaptiveFiltering(const GestureSensorFrame& frame, float deltaTime)
{
    if (!frame.gyroValid)
    {
        return;
    }

    float gyroX = frame.gyroX - gyroBiasX;
    float gyroY = frame.gyroY - gyroBiasY;
    float gyroZ = frame.gyroZ - gyroBiasZ;

    float gyroMagnitude = sqrtf(gyroX * gyroX + gyroY * gyroY + gyroZ * gyroZ);

    // Simple EMA on overall motion energy
    const float velocityAlpha = 0.2f;
    filterState.velocityMagnitude = GestureOrientationUtils::applyEMA(
        filterState.velocityMagnitude, gyroMagnitude, velocityAlpha);

    // Map the current motion into a 0..1 range
    float motionIntensity = constrain((filterState.velocityMagnitude - 0.05f) * 2.0f, 0.0f, 1.0f);

    // Adaptive smoothing: slower for low motion, faster for high motion
    float baseSmoothing = constrain(config.smoothing, 0.0f, 0.95f);
    float slowSmoothing = constrain(baseSmoothing * 1.2f + 0.05f, 0.05f, 0.9f);
    float fastSmoothing = constrain(baseSmoothing * 0.35f + 0.05f, 0.05f, slowSmoothing);
    float targetSmoothing = slowSmoothing + (fastSmoothing - slowSmoothing) * motionIntensity;

    filterState.adaptiveSmoothingFactor = GestureOrientationUtils::applyEMA(
        filterState.adaptiveSmoothingFactor, targetSmoothing, 0.25f);

    // Noise estimation: track gyro magnitude when quiet
    float noiseTarget = gyroMagnitude;
    if (gyroMagnitude > 0.4f)
    {
        noiseTarget = filterState.gyroNoiseEstimate; // Don't update during motion
    }
    filterState.gyroNoiseEstimate = GestureOrientationUtils::applyEMA(
        filterState.gyroNoiseEstimate, noiseTarget, 0.1f);
    filterState.gyroNoiseEstimate = constrain(filterState.gyroNoiseEstimate,
                                             GestureOrientationUtils::kMinNoiseEstimate,
                                             GestureOrientationUtils::kMaxNoiseEstimate);

    // Track last motion time
    if (gyroMagnitude > 0.05f)
    {
        lastMotionTime = millis();
    }
}

void GestureOrientation::updateMadgwickBeta()
{
    // Adaptive beta based on motion
    // Higher motion -> higher beta (more trust in gyro, faster response)
    // Lower motion -> lower beta (more trust in accel, better drift correction)

    float baseBeta = config.madgwickBeta;

    if (filterState.velocityMagnitude < 0.05f)
    {
        // Very low motion - minimize drift
        filterState.currentMadgwickBeta = 0.033f;  // ~2 degree error convergence
    }
    else if (filterState.velocityMagnitude < 0.2f)
    {
        // Low motion - good balance
        filterState.currentMadgwickBeta = 0.066f;  // ~4 degree error convergence
    }
    else if (filterState.velocityMagnitude < 0.5f)
    {
        // Moderate motion
        filterState.currentMadgwickBeta = baseBeta;  // ~6 degree error convergence
    }
    else if (filterState.velocityMagnitude < 1.0f)
    {
        // High motion - trust gyro more
        filterState.currentMadgwickBeta = 0.15f;  // ~9 degree error convergence
    }
    else
    {
        // Very high motion - maximum gyro trust
        filterState.currentMadgwickBeta = 0.2f;  // ~12 degree error convergence
    }

    // Additional scaling based on noise estimate
    float noiseFactor = constrain(filterState.gyroNoiseEstimate / 0.1f, 0.5f, 2.0f);
    filterState.currentMadgwickBeta *= noiseFactor;

    // Keep beta in reasonable range
    filterState.currentMadgwickBeta = constrain(filterState.currentMadgwickBeta, 0.01f, 0.5f);
}

// ============================================================================
// Utility Functions
// ============================================================================

namespace GestureOrientationUtils
{
    float applyDynamicDeadzone(float value, float baseThreshold, float noiseFactor)
    {
        float base = fmaxf(baseThreshold, 0.0f);
        float dynamicThreshold = base + noiseFactor * kRadToDeg * 1.2f;

        const float minThreshold = 0.05f;
        if (base <= 0.0f)
        {
            dynamicThreshold = fmaxf(dynamicThreshold, minThreshold);
        }
        else
        {
            dynamicThreshold = constrain(dynamicThreshold, base * 0.6f, base * 2.2f + minThreshold);
            dynamicThreshold = fmaxf(dynamicThreshold, minThreshold);
        }

        float absValue = fabsf(value);
        if (absValue <= dynamicThreshold)
        {
            return 0.0f;
        }

        // Smooth transition beyond deadzone
        float excess = absValue - dynamicThreshold;
        float response = dynamicThreshold + excess * 0.75f;
        return (value >= 0.0f) ? response : -response;
    }
}
