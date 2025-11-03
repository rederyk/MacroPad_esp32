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
#include <algorithm>
#include <cmath>
#include <memory>
#include <new>

OrientationFeatureExtractor::OrientationFeatureExtractor()
    : _madgwick(100.0f, 0.1f),
      _rotationThreshold(50.0f * M_PI / 180.0f),  // Reduced from 70° to 50°
      _tiltThreshold(20.0f * M_PI / 180.0f),      // Reduced from 30° to 20°
      _shakeThreshold(1.0f),                      // Reduced from 2.0 to 1.0 rad/s
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

    const uint16_t totalSamples = buffer->sampleCount;
    const uint16_t sampleCount = std::min<uint16_t>(totalSamples, MAX_HISTORY_SAMPLES);
    const uint16_t startIndex = totalSamples > sampleCount ? totalSamples - sampleCount : 0;

    std::unique_ptr<float[]> rollHistory(new (std::nothrow) float[sampleCount]);
    std::unique_ptr<float[]> pitchHistory(new (std::nothrow) float[sampleCount]);
    std::unique_ptr<float[]> yawHistory(new (std::nothrow) float[sampleCount]);
    std::unique_ptr<float[]> gyroMagnitudes(new (std::nothrow) float[sampleCount]);
    std::unique_ptr<float[]> gyroXHistory(new (std::nothrow) float[sampleCount]);
    std::unique_ptr<float[]> gyroYHistory(new (std::nothrow) float[sampleCount]);
    std::unique_ptr<float[]> gyroZHistory(new (std::nothrow) float[sampleCount]);

    if (!rollHistory || !pitchHistory || !yawHistory || !gyroMagnitudes ||
        !gyroXHistory || !gyroYHistory || !gyroZHistory) {
        Logger::getInstance().log("[OrientationFeatures] Failed to allocate history arrays (OOM)");
        return false;
    }

    // Initial orientation
    float initialRoll = 0, initialPitch = 0, initialYaw = 0;

    // Process each sample through Madgwick filter
    for (uint16_t i = 0; i < sampleCount; i++) {
        const Sample& s = buffer->samples[startIndex + i];

        // Only update if gyro data is valid
        if (s.gyroValid) {
            _madgwick.update(s.gyroX, s.gyroY, s.gyroZ, s.x, s.y, s.z);

            // Get current orientation
            float roll, pitch, yaw;
            _madgwick.getEulerAngles(roll, pitch, yaw);

            // Store orientation history
            rollHistory[i] = roll;
            pitchHistory[i] = pitch;
            yawHistory[i] = yaw;

            // Store raw gyro data (for motion-based gesture detection)
            gyroXHistory[i] = s.gyroX;
            gyroYHistory[i] = s.gyroY;
            gyroZHistory[i] = s.gyroZ;

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
            gyroXHistory[i] = 0;
            gyroYHistory[i] = 0;
            gyroZHistory[i] = 0;
        }
    }

    // Calculate final orientation and deltas
    features.finalRoll = rollHistory[sampleCount - 1];
    features.finalPitch = pitchHistory[sampleCount - 1];
    features.finalYaw = yawHistory[sampleCount - 1];

    features.deltaRoll = angleDifference(features.finalRoll, initialRoll);
    features.deltaPitch = angleDifference(features.finalPitch, initialPitch);
    features.deltaYaw = angleDifference(features.finalYaw, initialYaw);

    // Calculate statistics for each angle and gyro axis
    float sumRoll = 0, sumRollSq = 0;
    float sumPitch = 0, sumPitchSq = 0;
    float sumYaw = 0, sumYawSq = 0;
    float sumGyro = 0, sumGyroSq = 0;
    float maxGyro = 0;

    float sumGyroX = 0, sumGyroXSq = 0, maxGyroX = 0;
    float sumGyroY = 0, sumGyroYSq = 0, maxGyroY = 0;
    float sumGyroZ = 0, sumGyroZSq = 0, maxGyroZ = 0;
    float sumSignedGyroX = 0, sumSignedGyroY = 0, sumSignedGyroZ = 0;
    float maxGyroXPos = 0, maxGyroXNeg = 0;
    float maxGyroYPos = 0, maxGyroYNeg = 0;
    float maxGyroZPos = 0, maxGyroZNeg = 0;

    for (uint16_t i = 0; i < sampleCount; i++) {
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

        // Raw gyro axis statistics
        float rawGyroX = gyroXHistory[i];
        float rawGyroY = gyroYHistory[i];
        float rawGyroZ = gyroZHistory[i];

        float absGyroX = fabsf(rawGyroX);
        float absGyroY = fabsf(rawGyroY);
        float absGyroZ = fabsf(rawGyroZ);

        sumGyroX += absGyroX;
        sumGyroXSq += absGyroX * absGyroX;
        if (absGyroX > maxGyroX) maxGyroX = absGyroX;
        sumSignedGyroX += rawGyroX;
        if (rawGyroX > maxGyroXPos) maxGyroXPos = rawGyroX;
        if (-rawGyroX > maxGyroXNeg) maxGyroXNeg = -rawGyroX;

        sumGyroY += absGyroY;
        sumGyroYSq += absGyroY * absGyroY;
        if (absGyroY > maxGyroY) maxGyroY = absGyroY;
        sumSignedGyroY += rawGyroY;
        if (rawGyroY > maxGyroYPos) maxGyroYPos = rawGyroY;
        if (-rawGyroY > maxGyroYNeg) maxGyroYNeg = -rawGyroY;

        sumGyroZ += absGyroZ;
        sumGyroZSq += absGyroZ * absGyroZ;
        if (absGyroZ > maxGyroZ) maxGyroZ = absGyroZ;
        sumSignedGyroZ += rawGyroZ;
        if (rawGyroZ > maxGyroZPos) maxGyroZPos = rawGyroZ;
        if (-rawGyroZ > maxGyroZNeg) maxGyroZNeg = -rawGyroZ;
    }

    uint16_t n = sampleCount;

    // Calculate means
    features.rollMean = sumRoll / n;
    features.pitchMean = sumPitch / n;
    features.yawMean = sumYaw / n;
    features.gyroMagnitudeMean = sumGyro / n;
    features.gyroMagnitudeMax = maxGyro;

    features.gyroXMean = sumGyroX / n;
    features.gyroYMean = sumGyroY / n;
    features.gyroZMean = sumGyroZ / n;
    features.gyroXMax = maxGyroX;
    features.gyroYMax = maxGyroY;
    features.gyroZMax = maxGyroZ;
    features.gyroXSignedMean = sumSignedGyroX / n;
    features.gyroYSignedMean = sumSignedGyroY / n;
    features.gyroZSignedMean = sumSignedGyroZ / n;
    features.gyroXPosPeak = maxGyroXPos;
    features.gyroXNegPeak = maxGyroXNeg;
    features.gyroYPosPeak = maxGyroYPos;
    features.gyroYNegPeak = maxGyroYNeg;
    features.gyroZPosPeak = maxGyroZPos;
    features.gyroZNegPeak = maxGyroZNeg;

    // Calculate standard deviations
    float rollVar = (sumRollSq / n) - (features.rollMean * features.rollMean);
    float pitchVar = (sumPitchSq / n) - (features.pitchMean * features.pitchMean);
    float yawVar = (sumYawSq / n) - (features.yawMean * features.yawMean);
    float gyroVar = (sumGyroSq / n) - (features.gyroMagnitudeMean * features.gyroMagnitudeMean);

    float gyroXVar = (sumGyroXSq / n) - (features.gyroXMean * features.gyroXMean);
    float gyroYVar = (sumGyroYSq / n) - (features.gyroYMean * features.gyroYMean);
    float gyroZVar = (sumGyroZSq / n) - (features.gyroZMean * features.gyroZMean);

    features.rollStd = sqrtf(fmaxf(rollVar, 0.0f));
    features.pitchStd = sqrtf(fmaxf(pitchVar, 0.0f));
    features.yawStd = sqrtf(fmaxf(yawVar, 0.0f));
    features.gyroMagnitudeStd = sqrtf(fmaxf(gyroVar, 0.0f));

    features.gyroXStd = sqrtf(fmaxf(gyroXVar, 0.0f));
    features.gyroYStd = sqrtf(fmaxf(gyroYVar, 0.0f));
    features.gyroZStd = sqrtf(fmaxf(gyroZVar, 0.0f));

    // Calculate total rotation (sum of absolute angle changes)
    features.totalRotation = 0;
    for (uint16_t i = 1; i < sampleCount; i++) {
        float dr = fabsf(angleDifference(rollHistory[i], rollHistory[i - 1]));
        float dp = fabsf(angleDifference(pitchHistory[i], pitchHistory[i - 1]));
        float dy = fabsf(angleDifference(yawHistory[i], yawHistory[i - 1]));
        features.totalRotation += dr + dp + dy;
    }

    Logger::getInstance().log("[OrientationFeatures] dRoll=" + String(features.deltaRoll * 180 / M_PI, 1) +
                             "° dPitch=" + String(features.deltaPitch * 180 / M_PI, 1) +
                             "° dYaw=" + String(features.deltaYaw * 180 / M_PI, 1) + "°");

    // Log first raw sample for axis mapping diagnostic
    if (buffer && buffer->sampleCount > 0) {
        const Sample& firstSample = buffer->samples[0];
        Logger::getInstance().log("[GYRO_RAW] First sample: gyroX=" + String(firstSample.gyroX, 3) +
                                 " gyroY=" + String(firstSample.gyroY, 3) +
                                 " gyroZ=" + String(firstSample.gyroZ, 3));
    }

    Logger::getInstance().log("[OrientationFeatures] Gyro: X(max=" + String(features.gyroXMax, 2) +
                             " mean=" + String(features.gyroXMean, 2) +
                             " pos=" + String(features.gyroXPosPeak, 2) +
                             " neg=" + String(features.gyroXNegPeak, 2) +
                             " signed=" + String(features.gyroXSignedMean, 2) + ") " +
                             "Y(max=" + String(features.gyroYMax, 2) +
                             " mean=" + String(features.gyroYMean, 2) +
                             " pos=" + String(features.gyroYPosPeak, 2) +
                             " neg=" + String(features.gyroYNegPeak, 2) +
                             " signed=" + String(features.gyroYSignedMean, 2) + ") " +
                             "Z(max=" + String(features.gyroZMax, 2) +
                             " mean=" + String(features.gyroZMean, 2) +
                             " pos=" + String(features.gyroZPosPeak, 2) +
                             " neg=" + String(features.gyroZNegPeak, 2) +
                             " signed=" + String(features.gyroZSignedMean, 2) + ")");

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

    // Tilt gestures intentionally disabled to reduce false positives

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

    // Check for shake using RAW gyroscope data
    // Shake = rapid rotation around one axis independent of device orientation
    // Use peak values to detect quick movements
    float shakeThresholdPeak = _shakeThreshold * 1.5f;  // Higher threshold for peak detection
    float shakeThresholdMean = _shakeThreshold * 0.5f;  // Lower threshold for sustained motion

    // Check each axis independently using raw gyro data
    bool xShake = (features.gyroXMax > shakeThresholdPeak) ||
                  (features.gyroXMean > shakeThresholdMean && features.gyroXStd > _shakeThreshold * 0.3f);
    bool yShake = (features.gyroYMax > shakeThresholdPeak) ||
                  (features.gyroYMean > shakeThresholdMean && features.gyroYStd > _shakeThreshold * 0.3f);
    bool zShake = (features.gyroZMax > shakeThresholdPeak) ||
                  (features.gyroZMean > shakeThresholdMean && features.gyroZStd > _shakeThreshold * 0.3f);

    if (xShake || yShake || zShake) {
        // Determine which axis has the strongest shake
        // Use combination of peak and mean for robustness
        float xStrength = features.gyroXMax * 0.6f + features.gyroXMean * 0.4f;
        float yStrength = features.gyroYMax * 0.6f + features.gyroYMean * 0.4f;
        float zStrength = features.gyroZMax * 0.6f + features.gyroZMean * 0.4f;

        float maxStrength = fmaxf(xStrength, fmaxf(yStrength, zStrength));

        auto chooseDirection = [&](OrientationType posType, OrientationType negType,
                                   float posPeak, float negPeak, float signedMean) -> OrientationType {
            const float dominanceFactor = 1.15f;  // Reduced from 1.25 to 1.15 for symmetric gestures
            const float meanBias = _shakeThreshold * 0.15f;  // Reduced from 0.2 to 0.15 for better direction detection

            if (posPeak <= 0.0f && negPeak <= 0.0f) {
                return ORIENT_UNKNOWN;
            }
            if (posPeak >= negPeak * dominanceFactor) {
                return posType;
            }
            if (negPeak >= posPeak * dominanceFactor) {
                return negType;
            }
            if (fabsf(signedMean) > meanBias) {
                return signedMean >= 0.0f ? posType : negType;
            }
            return (posPeak >= negPeak) ? posType : negType;
        };

        // Require dominant axis to be stronger (15% more) - reduced from 20% for better detection
        if (xStrength >= maxStrength * 0.8f && xStrength > yStrength * 1.15f && xStrength > zStrength * 1.15f) {
            _confidence = 0.75f + fminf(xStrength / (shakeThresholdPeak * 2.0f), 0.15f);
            OrientationType dir = chooseDirection(ORIENT_SHAKE_X_POS, ORIENT_SHAKE_X_NEG,
                                                  features.gyroXPosPeak, features.gyroXNegPeak,
                                                  features.gyroXSignedMean);
            if (dir != ORIENT_UNKNOWN) {
                return dir;
            }
        } else if (yStrength >= maxStrength * 0.8f && yStrength > xStrength * 1.15f && yStrength > zStrength * 1.15f) {
            _confidence = 0.75f + fminf(yStrength / (shakeThresholdPeak * 2.0f), 0.15f);
            OrientationType dir = chooseDirection(ORIENT_SHAKE_Y_POS, ORIENT_SHAKE_Y_NEG,
                                                  features.gyroYPosPeak, features.gyroYNegPeak,
                                                  features.gyroYSignedMean);
            if (dir != ORIENT_UNKNOWN) {
                return dir;
            }
        } else if (zStrength >= maxStrength * 0.8f && zStrength > xStrength * 1.15f && zStrength > yStrength * 1.15f) {
            _confidence = 0.75f + fminf(zStrength / (shakeThresholdPeak * 2.0f), 0.15f);
            OrientationType dir = chooseDirection(ORIENT_SHAKE_Z_POS, ORIENT_SHAKE_Z_NEG,
                                                  features.gyroZPosPeak, features.gyroZNegPeak,
                                                  features.gyroZSignedMean);
            if (dir != ORIENT_UNKNOWN) {
                return dir;
            }
        }
        // If no clear dominant axis, fall through to unknown
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
        case ORIENT_SHAKE_X_POS:     return "Shake X+";
        case ORIENT_SHAKE_X_NEG:     return "Shake X-";
        case ORIENT_SHAKE_Y_POS:     return "Shake Y+";
        case ORIENT_SHAKE_Y_NEG:     return "Shake Y-";
        case ORIENT_SHAKE_Z_POS:     return "Shake Z+";
        case ORIENT_SHAKE_Z_NEG:     return "Shake Z-";
        default:                     return "Unknown";
    }
}
