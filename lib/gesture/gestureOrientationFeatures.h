/*
 * ESP32 MacroPad Project
 * Copyright (C) [2025] [Enrico Mori]
 *
 * Orientation-Based Gesture Features
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef GESTURE_ORIENTATION_FEATURES_H
#define GESTURE_ORIENTATION_FEATURES_H

#include "gestureRead.h"
#include "MadgwickAHRS.h"

/**
 * Orientation-based gesture features
 * These describe how the device was rotated/tilted during gesture
 */
struct OrientationFeatures {
    // Euler angle changes (radians)
    float deltaRoll;          // Total roll change
    float deltaPitch;         // Total pitch change
    float deltaYaw;           // Total yaw change

    // Angle statistics
    float rollMean;
    float rollStd;
    float pitchMean;
    float pitchStd;
    float yawMean;
    float yawStd;

    // Total rotation
    float totalRotation;      // Sum of all angular changes

    // Gyroscope features
    float gyroMagnitudeMean;  // Average angular velocity
    float gyroMagnitudeMax;   // Peak angular velocity
    float gyroMagnitudeStd;   // Variation in angular velocity

    // Final orientation
    float finalRoll;          // Ending roll
    float finalPitch;         // Ending pitch
    float finalYaw;           // Ending yaw

    OrientationFeatures() {
        deltaRoll = deltaPitch = deltaYaw = 0;
        rollMean = rollStd = 0;
        pitchMean = pitchStd = 0;
        yawMean = yawStd = 0;
        totalRotation = 0;
        gyroMagnitudeMean = gyroMagnitudeMax = gyroMagnitudeStd = 0;
        finalRoll = finalPitch = finalYaw = 0;
    }
};

/**
 * Recognized orientation gesture types
 */
enum OrientationType {
    ORIENT_UNKNOWN = 0,
    ORIENT_ROTATE_90_CW,        // 90° clockwise (yaw)
    ORIENT_ROTATE_90_CCW,       // 90° counter-clockwise
    ORIENT_ROTATE_180,          // 180° rotation
    ORIENT_TILT_FORWARD,        // Tilt forward (pitch+)
    ORIENT_TILT_BACKWARD,       // Tilt backward (pitch-)
    ORIENT_TILT_LEFT,           // Tilt left (roll+)
    ORIENT_TILT_RIGHT,          // Tilt right (roll-)
    ORIENT_FACE_UP,             // Device face up
    ORIENT_FACE_DOWN,           // Device face down
    ORIENT_SPIN,                // Continuous spinning
    ORIENT_SHAKE_X,             // Shake along X axis
    ORIENT_SHAKE_Y,             // Shake along Y axis
    ORIENT_SHAKE_Z              // Shake along Z axis
};

/**
 * Orientation Feature Extractor
 *
 * Uses Madgwick filter to track device orientation throughout gesture
 * and extract rotation-based features
 */
class OrientationFeatureExtractor {
public:
    OrientationFeatureExtractor();

    /**
     * Extract orientation features from sample buffer
     *
     * @param buffer Sample buffer with accel and gyro data
     * @param features Output orientation features
     * @return true if extraction successful
     */
    bool extract(SampleBuffer* buffer, OrientationFeatures& features);

    /**
     * Classify orientation gesture type
     *
     * @param features Orientation features from extract()
     * @return Recognized orientation type
     */
    OrientationType classify(const OrientationFeatures& features);

    /**
     * Get confidence of last classification (0-1)
     */
    float getConfidence() const { return _confidence; }

    /**
     * Set rotation threshold for rotation gestures (degrees)
     * Default: 70° (detects 90° rotations with tolerance)
     */
    void setRotationThreshold(float degrees);

    /**
     * Set tilt threshold for tilt gestures (degrees)
     * Default: 30°
     */
    void setTiltThreshold(float degrees);

    /**
     * Set shake threshold (angular velocity rad/s)
     * Default: 2.0 rad/s
     */
    void setShakeThreshold(float radPerSec);

private:
    MadgwickAHRS _madgwick;
    float _rotationThreshold;    // In radians
    float _tiltThreshold;         // In radians
    float _shakeThreshold;        // In rad/s
    float _confidence;

    // Helper: Calculate angle difference (shortest path on circle)
    float angleDifference(float a, float b) const;

    // Helper: Check if values oscillate (for shake detection)
    bool isOscillating(float* values, uint16_t count, float threshold) const;
};

/**
 * Get human-readable name for orientation type
 */
const char* getOrientationName(OrientationType type);

#endif // GESTURE_ORIENTATION_FEATURES_H
