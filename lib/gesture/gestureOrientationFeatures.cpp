/*
 * ESP32 MacroPad Project
 * Copyright (C) [2025] [Enrico Mori]
 *
 * Orientation Features Implementation
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

#include "gestureOrientationFeatures.h"
#include "Logger.h"
#include <cmath>

OrientationFeatureExtractor::OrientationFeatureExtractor()
    : _madgwick(100.0f, 0.1f),
      _rotationThreshold(70.0f * M_PI / 180.0f),
      _tiltThreshold(30.0f * M_PI / 180.0f),
      _shakeThreshold(2.0f),
      _confidence(0.0f)
{
}

void OrientationFeatureExtractor::setRotationThreshold(float degrees)
{
    _rotationThreshold = degrees * M_PI / 180.0f;
}

void OrientationFeatureExtractor::setTiltThreshold(float degrees)
{
    _tiltThreshold = degrees * M_PI / 180.0f;
}

void OrientationFeatureExtractor::setShakeThreshold(float radPerSec)
{
    _shakeThreshold = radPerSec;
}

bool OrientationFeatureExtractor::extract(SampleBuffer* buffer, OrientationFeatures& features)
{
    if (!buffer || buffer->sampleCount < 5 || !buffer->samples) {
        Logger::getInstance().log("[OrientationFeatures] Invalid buffer");
        return false;
    }

    features = OrientationFeatures();

    // Configure Madgwick with buffer's sample rate
    if (buffer->sampleHZ > 0) {
        _madgwick.setSampleFrequency(static_cast<float>(buffer->sampleHZ));
    }

    // Reset filter
    _madgwick.reset();

    // Arrays to store angle history
    float* rollHistory = new float[buffer->sampleCount];
    float* pitchHistory = new float[buffer->sampleCount];
    float* yawHistory = new float[buffer->sampleCount];
    float* gyroMagnitudes = new float[buffer->sampleCount];

    if (!rollHistory || !pitchHistory || !yawHistory || !gyroMagnitudes) {
        Logger::getInstance().log("[OrientationFeatures] Failed to allocate history arrays");
        return false;
    }

    // Initial orientation
    float initialRoll = 0, initialPitch = 0, initialYaw = 0;

    // Process each sample through Madgwick filter
    for (uint16_t i = 0; i < buffer->sampleCount; i++) {
        const Sample& s = buffer->samples[i];

        // Only update if gyro data is valid
        if (s.gyroValid) {
            _madgwick.update(s.gyroX, s.gyroY, s.gyroZ, s.x, s.y, s.z);

            // Get current orientation
            float roll, pitch, yaw;
            _madgwick.getEulerAngles(roll, pitch, yaw);

            // Store history
            rollHistory[i] = roll;
            pitchHistory[i] = pitch;
            yawHistory[i] = yaw;

            // Calculate gyro magnitude
            gyroMagnitudes[i] = sqrtf(s.gyroX * s.gyroX + s.gyroY * s.gyroY + s.gyroZ * s.gyroZ);

            // Save initial orientation
            if (i == 0) {
                initialRoll = roll;
                initialPitch = pitch;
                initialYaw = yaw;
            }
        } else {
            rollHistory[i] = 0;
            pitchHistory[i] = 0;
            yawHistory[i] = 0;
            gyroMagnitudes[i] = 0;
        }
    }

    // Calculate final orientation and deltas
    features.finalRoll = rollHistory[buffer->sampleCount - 1];
    features.finalPitch = pitchHistory[buffer->sampleCount - 1];
    features.finalYaw = yawHistory[buffer->sampleCount - 1];

    features.deltaRoll = angleDifference(features.finalRoll, initialRoll);
    features.deltaPitch = angleDifference(features.finalPitch, initialPitch);
    features.deltaYaw = angleDifference(features.finalYaw, initialYaw);

    // Calculate statistics for each angle
    float sumRoll = 0, sumRollSq = 0;
    float sumPitch = 0, sumPitchSq = 0;
    float sumYaw = 0, sumYawSq = 0;
    float sumGyro = 0, sumGyroSq = 0;
    float maxGyro = 0;

    for (uint16_t i = 0; i < buffer->sampleCount; i++) {
        sumRoll += rollHistory[i];
        sumRollSq += rollHistory[i] * rollHistory[i];

        sumPitch += pitchHistory[i];
        sumPitchSq += pitchHistory[i] * pitchHistory[i];

        sumYaw += yawHistory[i];
        sumYawSq += yawHistory[i] * yawHistory[i];

        sumGyro += gyroMagnitudes[i];
        sumGyroSq += gyroMagnitudes[i] * gyroMagnitudes[i];

        if (gyroMagnitudes[i] > maxGyro) {
            maxGyro = gyroMagnitudes[i];
        }
    }

    uint16_t n = buffer->sampleCount;

    // Calculate means
    features.rollMean = sumRoll / n;
    features.pitchMean = sumPitch / n;
    features.yawMean = sumYaw / n;
    features.gyroMagnitudeMean = sumGyro / n;
    features.gyroMagnitudeMax = maxGyro;

    // Calculate standard deviations
    float rollVar = (sumRollSq / n) - (features.rollMean * features.rollMean);
    float pitchVar = (sumPitchSq / n) - (features.pitchMean * features.pitchMean);
    float yawVar = (sumYawSq / n) - (features.yawMean * features.yawMean);
    float gyroVar = (sumGyroSq / n) - (features.gyroMagnitudeMean * features.gyroMagnitudeMean);

    features.rollStd = sqrtf(fmaxf(rollVar, 0.0f));
    features.pitchStd = sqrtf(fmaxf(pitchVar, 0.0f));
    features.yawStd = sqrtf(fmaxf(yawVar, 0.0f));
    features.gyroMagnitudeStd = sqrtf(fmaxf(gyroVar, 0.0f));

    // Calculate total rotation (sum of absolute angle changes)
    features.totalRotation = 0;
    for (uint16_t i = 1; i < buffer->sampleCount; i++) {
        float dr = fabsf(angleDifference(rollHistory[i], rollHistory[i - 1]));
        float dp = fabsf(angleDifference(pitchHistory[i], pitchHistory[i - 1]));
        float dy = fabsf(angleDifference(yawHistory[i], yawHistory[i - 1]));
        features.totalRotation += dr + dp + dy;
    }

    // Cleanup
    delete[] rollHistory;
    delete[] pitchHistory;
    delete[] yawHistory;
    delete[] gyroMagnitudes;

    Logger::getInstance().log("[OrientationFeatures] dRoll=" + String(features.deltaRoll * 180 / M_PI, 1) +
                             "° dPitch=" + String(features.deltaPitch * 180 / M_PI, 1) +
                             "° dYaw=" + String(features.deltaYaw * 180 / M_PI, 1) + "°");

    return true;
}

OrientationType OrientationFeatureExtractor::classify(const OrientationFeatures& features)
{
    _confidence = 0.0f;

    const float deg90 = M_PI / 2.0f;
    const float deg180 = M_PI;

    // Check for rotations (yaw changes)
    if (fabsf(features.deltaYaw) > _rotationThreshold) {
        if (fabsf(features.deltaYaw - deg90) < 0.3f) {
            _confidence = 0.9f;
            return features.deltaYaw > 0 ? ORIENT_ROTATE_90_CCW : ORIENT_ROTATE_90_CW;
        }
        else if (fabsf(fabsf(features.deltaYaw) - deg180) < 0.4f) {
            _confidence = 0.85f;
            return ORIENT_ROTATE_180;
        }
        else if (features.totalRotation > deg180 * 2) {
            // Multiple rotations = spinning
            _confidence = 0.8f;
            return ORIENT_SPIN;
        }
    }

    // Check for tilts (pitch/roll changes)
    if (fabsf(features.deltaPitch) > _tiltThreshold) {
        _confidence = 0.85f;
        return features.deltaPitch > 0 ? ORIENT_TILT_FORWARD : ORIENT_TILT_BACKWARD;
    }

    if (fabsf(features.deltaRoll) > _tiltThreshold) {
        _confidence = 0.85f;
        return features.deltaRoll > 0 ? ORIENT_TILT_LEFT : ORIENT_TILT_RIGHT;
    }

    // Check for face up/down (based on final orientation)
    const float faceUpPitch = -M_PI / 2.0f;   // -90° (face up)
    const float faceDownPitch = M_PI / 2.0f;  // +90° (face down)

    if (fabsf(features.finalPitch - faceUpPitch) < 0.5f) {
        _confidence = 0.8f;
        return ORIENT_FACE_UP;
    }

    if (fabsf(features.finalPitch - faceDownPitch) < 0.5f) {
        _confidence = 0.8f;
        return ORIENT_FACE_DOWN;
    }

    // Check for shake (high gyro variation but low net rotation)
    if (features.gyroMagnitudeMax > _shakeThreshold && features.gyroMagnitudeStd > _shakeThreshold * 0.5f) {
        // Determine shake axis based on which has highest variation
        if (features.rollStd > features.pitchStd && features.rollStd > features.yawStd) {
            _confidence = 0.75f;
            return ORIENT_SHAKE_X;
        } else if (features.pitchStd > features.yawStd) {
            _confidence = 0.75f;
            return ORIENT_SHAKE_Y;
        } else {
            _confidence = 0.75f;
            return ORIENT_SHAKE_Z;
        }
    }

    return ORIENT_UNKNOWN;
}

float OrientationFeatureExtractor::angleDifference(float a, float b) const
{
    // Calculate shortest angular distance between two angles
    float diff = a - b;

    // Normalize to [-π, π]
    while (diff > M_PI) diff -= 2 * M_PI;
    while (diff < -M_PI) diff += 2 * M_PI;

    return diff;
}

const char* getOrientationName(OrientationType type)
{
    switch (type) {
        case ORIENT_ROTATE_90_CW:    return "Rotate 90 CW";
        case ORIENT_ROTATE_90_CCW:   return "Rotate 90 CCW";
        case ORIENT_ROTATE_180:      return "Rotate 180";
        case ORIENT_TILT_FORWARD:    return "Tilt Forward";
        case ORIENT_TILT_BACKWARD:   return "Tilt Backward";
        case ORIENT_TILT_LEFT:       return "Tilt Left";
        case ORIENT_TILT_RIGHT:      return "Tilt Right";
        case ORIENT_FACE_UP:         return "Face Up";
        case ORIENT_FACE_DOWN:       return "Face Down";
        case ORIENT_SPIN:            return "Spin";
        case ORIENT_SHAKE_X:         return "Shake X";
        case ORIENT_SHAKE_Y:         return "Shake Y";
        case ORIENT_SHAKE_Z:         return "Shake Z";
        default:                     return "Unknown";
    }
}
