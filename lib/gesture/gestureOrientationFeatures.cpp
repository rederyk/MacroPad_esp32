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

    // DO NOT RESET - Madgwick should track orientation continuously
    // This allows proper world-frame transformation even if device is held tilted
    // _madgwick.reset();

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
    // Use calibrated accelerometer for shake detection (orientation-independent)
    std::unique_ptr<float[]> accelXHistory(new (std::nothrow) float[sampleCount]);
    std::unique_ptr<float[]> accelYHistory(new (std::nothrow) float[sampleCount]);
    std::unique_ptr<float[]> accelZHistory(new (std::nothrow) float[sampleCount]);
    // World-frame acceleration (gravity removed, rotation-corrected)
    std::unique_ptr<float[]> worldAccelXHistory(new (std::nothrow) float[sampleCount]);
    std::unique_ptr<float[]> worldAccelYHistory(new (std::nothrow) float[sampleCount]);
    std::unique_ptr<float[]> worldAccelZHistory(new (std::nothrow) float[sampleCount]);

    if (!rollHistory || !pitchHistory || !yawHistory || !gyroMagnitudes ||
        !gyroXHistory || !gyroYHistory || !gyroZHistory ||
        !accelXHistory || !accelYHistory || !accelZHistory ||
        !worldAccelXHistory || !worldAccelYHistory || !worldAccelZHistory) {
        Logger::getInstance().log("[OrientationFeatures] Failed to allocate history arrays (OOM)");
        return false;
    }

    // Initial orientation
    float initialRoll = 0, initialPitch = 0, initialYaw = 0;

    // Process each sample through Madgwick filter
    for (uint16_t i = 0; i < sampleCount; i++) {
        const Sample& s = buffer->samples[startIndex + i];

        // Store calibrated accelerometer data (sensor frame, gravity removed)
        accelXHistory[i] = s.x;
        accelYHistory[i] = s.y;
        accelZHistory[i] = s.z;

        // Only update if gyro data is valid
        if (s.gyroValid) {
            _madgwick.update(s.gyroX, s.gyroY, s.gyroZ, s.x, s.y, s.z);

            // Get current orientation quaternion
            float q0, q1, q2, q3;
            _madgwick.getQuaternion(q0, q1, q2, q3);

            // Get Euler angles for logging/analysis
            float roll, pitch, yaw;
            _madgwick.getEulerAngles(roll, pitch, yaw);

            // Store orientation history
            rollHistory[i] = roll;
            pitchHistory[i] = pitch;
            yawHistory[i] = yaw;

            // Store raw gyro data (for reference only)
            gyroXHistory[i] = s.gyroX;
            gyroYHistory[i] = s.gyroY;
            gyroZHistory[i] = s.gyroZ;

            // Calculate gyro magnitude
            gyroMagnitudes[i] = sqrtf(s.gyroX * s.gyroX + s.gyroY * s.gyroY + s.gyroZ * s.gyroZ);

            // Transform calibrated acceleration to world frame using quaternion rotation
            // This removes the effect of device orientation, making shake detection intuitive
            // Formula: v_world = q * v_sensor * q_conjugate
            // Simplified: rotateVectorByQuaternion(s.x, s.y, s.z, q0, q1, q2, q3)

            // Quaternion rotation: v' = q * v * q^-1
            // For unit quaternion: q^-1 = conjugate(q) = [q0, -q1, -q2, -q3]
            float ax = s.x, ay = s.y, az = s.z;

            // First: multiply quaternion q with vector [0, ax, ay, az]
            float t0 = -q1 * ax - q2 * ay - q3 * az;
            float t1 =  q0 * ax + q2 * az - q3 * ay;
            float t2 =  q0 * ay + q3 * ax - q1 * az;
            float t3 =  q0 * az + q1 * ay - q2 * ax;

            // Then: multiply result with conjugate [q0, -q1, -q2, -q3]
            worldAccelXHistory[i] = t1 * q0 - t0 * q1 + t2 * q3 - t3 * q2;
            worldAccelYHistory[i] = t2 * q0 - t0 * q2 + t3 * q1 - t1 * q3;
            worldAccelZHistory[i] = t3 * q0 - t0 * q3 + t1 * q2 - t2 * q1;

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
            worldAccelXHistory[i] = 0;
            worldAccelYHistory[i] = 0;
            worldAccelZHistory[i] = 0;
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

    // Calibrated accelerometer statistics (sensor frame)
    float sumAccelX = 0, sumAccelXSq = 0, maxAccelX = 0;
    float sumAccelY = 0, sumAccelYSq = 0, maxAccelY = 0;
    float sumAccelZ = 0, sumAccelZSq = 0, maxAccelZ = 0;
    float sumSignedAccelX = 0, sumSignedAccelY = 0, sumSignedAccelZ = 0;
    float maxAccelXPos = 0, maxAccelXNeg = 0;
    float maxAccelYPos = 0, maxAccelYNeg = 0;
    float maxAccelZPos = 0, maxAccelZNeg = 0;

    // World-frame acceleration statistics (INTUITIVE shake detection)
    float sumWorldAccelX = 0, sumWorldAccelXSq = 0, maxWorldAccelX = 0;
    float sumWorldAccelY = 0, sumWorldAccelYSq = 0, maxWorldAccelY = 0;
    float sumWorldAccelZ = 0, sumWorldAccelZSq = 0, maxWorldAccelZ = 0;
    float sumSignedWorldAccelX = 0, sumSignedWorldAccelY = 0, sumSignedWorldAccelZ = 0;
    float maxWorldAccelXPos = 0, maxWorldAccelXNeg = 0;
    float maxWorldAccelYPos = 0, maxWorldAccelYNeg = 0;
    float maxWorldAccelZPos = 0, maxWorldAccelZNeg = 0;

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

        // Calibrated accelerometer axis statistics (user reference frame)
        float rawAccelX = accelXHistory[i];
        float rawAccelY = accelYHistory[i];
        float rawAccelZ = accelZHistory[i];

        float absAccelX = fabsf(rawAccelX);
        float absAccelY = fabsf(rawAccelY);
        float absAccelZ = fabsf(rawAccelZ);

        sumAccelX += absAccelX;
        sumAccelXSq += absAccelX * absAccelX;
        if (absAccelX > maxAccelX) maxAccelX = absAccelX;
        sumSignedAccelX += rawAccelX;
        if (rawAccelX > maxAccelXPos) maxAccelXPos = rawAccelX;
        if (-rawAccelX > maxAccelXNeg) maxAccelXNeg = -rawAccelX;

        sumAccelY += absAccelY;
        sumAccelYSq += absAccelY * absAccelY;
        if (absAccelY > maxAccelY) maxAccelY = absAccelY;
        sumSignedAccelY += rawAccelY;
        if (rawAccelY > maxAccelYPos) maxAccelYPos = rawAccelY;
        if (-rawAccelY > maxAccelYNeg) maxAccelYNeg = -rawAccelY;

        sumAccelZ += absAccelZ;
        sumAccelZSq += absAccelZ * absAccelZ;
        if (absAccelZ > maxAccelZ) maxAccelZ = absAccelZ;
        sumSignedAccelZ += rawAccelZ;
        if (rawAccelZ > maxAccelZPos) maxAccelZPos = rawAccelZ;
        if (-rawAccelZ > maxAccelZNeg) maxAccelZNeg = -rawAccelZ;

        // World-frame acceleration statistics (orientation-independent shake)
        float rawWorldAccelX = worldAccelXHistory[i];
        float rawWorldAccelY = worldAccelYHistory[i];
        float rawWorldAccelZ = worldAccelZHistory[i];

        float absWorldAccelX = fabsf(rawWorldAccelX);
        float absWorldAccelY = fabsf(rawWorldAccelY);
        float absWorldAccelZ = fabsf(rawWorldAccelZ);

        sumWorldAccelX += absWorldAccelX;
        sumWorldAccelXSq += absWorldAccelX * absWorldAccelX;
        if (absWorldAccelX > maxWorldAccelX) maxWorldAccelX = absWorldAccelX;
        sumSignedWorldAccelX += rawWorldAccelX;
        if (rawWorldAccelX > maxWorldAccelXPos) maxWorldAccelXPos = rawWorldAccelX;
        if (-rawWorldAccelX > maxWorldAccelXNeg) maxWorldAccelXNeg = -rawWorldAccelX;

        sumWorldAccelY += absWorldAccelY;
        sumWorldAccelYSq += absWorldAccelY * absWorldAccelY;
        if (absWorldAccelY > maxWorldAccelY) maxWorldAccelY = absWorldAccelY;
        sumSignedWorldAccelY += rawWorldAccelY;
        if (rawWorldAccelY > maxWorldAccelYPos) maxWorldAccelYPos = rawWorldAccelY;
        if (-rawWorldAccelY > maxWorldAccelYNeg) maxWorldAccelYNeg = -rawWorldAccelY;

        sumWorldAccelZ += absWorldAccelZ;
        sumWorldAccelZSq += absWorldAccelZ * absWorldAccelZ;
        if (absWorldAccelZ > maxWorldAccelZ) maxWorldAccelZ = absWorldAccelZ;
        sumSignedWorldAccelZ += rawWorldAccelZ;
        if (rawWorldAccelZ > maxWorldAccelZPos) maxWorldAccelZPos = rawWorldAccelZ;
        if (-rawWorldAccelZ > maxWorldAccelZNeg) maxWorldAccelZNeg = -rawWorldAccelZ;
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

    // Calibrated accelerometer features (user reference frame)
    features.accelXMean = sumAccelX / n;
    features.accelYMean = sumAccelY / n;
    features.accelZMean = sumAccelZ / n;
    features.accelXMax = maxAccelX;
    features.accelYMax = maxAccelY;
    features.accelZMax = maxAccelZ;
    features.accelXSignedMean = sumSignedAccelX / n;
    features.accelYSignedMean = sumSignedAccelY / n;
    features.accelZSignedMean = sumSignedAccelZ / n;
    features.accelXPosPeak = maxAccelXPos;
    features.accelXNegPeak = maxAccelXNeg;
    features.accelYPosPeak = maxAccelYPos;
    features.accelYNegPeak = maxAccelYNeg;
    features.accelZPosPeak = maxAccelZPos;
    features.accelZNegPeak = maxAccelZNeg;

    float accelXVar = (sumAccelXSq / n) - (features.accelXMean * features.accelXMean);
    float accelYVar = (sumAccelYSq / n) - (features.accelYMean * features.accelYMean);
    float accelZVar = (sumAccelZSq / n) - (features.accelZMean * features.accelZMean);

    features.accelXStd = sqrtf(fmaxf(accelXVar, 0.0f));
    features.accelYStd = sqrtf(fmaxf(accelYVar, 0.0f));
    features.accelZStd = sqrtf(fmaxf(accelZVar, 0.0f));

    // World-frame accelerometer features (orientation-independent)
    features.worldAccelXMean = sumWorldAccelX / n;
    features.worldAccelYMean = sumWorldAccelY / n;
    features.worldAccelZMean = sumWorldAccelZ / n;
    features.worldAccelXMax = maxWorldAccelX;
    features.worldAccelYMax = maxWorldAccelY;
    features.worldAccelZMax = maxWorldAccelZ;
    features.worldAccelXSignedMean = sumSignedWorldAccelX / n;
    features.worldAccelYSignedMean = sumSignedWorldAccelY / n;
    features.worldAccelZSignedMean = sumSignedWorldAccelZ / n;
    features.worldAccelXPosPeak = maxWorldAccelXPos;
    features.worldAccelXNegPeak = maxWorldAccelXNeg;
    features.worldAccelYPosPeak = maxWorldAccelYPos;
    features.worldAccelYNegPeak = maxWorldAccelYNeg;
    features.worldAccelZPosPeak = maxWorldAccelZPos;
    features.worldAccelZNegPeak = maxWorldAccelZNeg;

    float worldAccelXVar = (sumWorldAccelXSq / n) - (features.worldAccelXMean * features.worldAccelXMean);
    float worldAccelYVar = (sumWorldAccelYSq / n) - (features.worldAccelYMean * features.worldAccelYMean);
    float worldAccelZVar = (sumWorldAccelZSq / n) - (features.worldAccelZMean * features.worldAccelZMean);

    features.worldAccelXStd = sqrtf(fmaxf(worldAccelXVar, 0.0f));
    features.worldAccelYStd = sqrtf(fmaxf(worldAccelYVar, 0.0f));
    features.worldAccelZStd = sqrtf(fmaxf(worldAccelZVar, 0.0f));

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

    // Log first calibrated accelerometer sample for diagnostic
    if (buffer && buffer->sampleCount > 0) {
        const Sample& firstSample = buffer->samples[0];
        Logger::getInstance().log("[ACCEL_CALIBRATED] First sample: accelX=" + String(firstSample.x, 3) +
                                 " accelY=" + String(firstSample.y, 3) +
                                 " accelZ=" + String(firstSample.z, 3));
    }

    Logger::getInstance().log("[OrientationFeatures] Accel (sensor frame): X(max=" + String(features.accelXMax, 2) +
                             " mean=" + String(features.accelXMean, 2) +
                             " pos=" + String(features.accelXPosPeak, 2) +
                             " neg=" + String(features.accelXNegPeak, 2) +
                             " signed=" + String(features.accelXSignedMean, 2) + ") " +
                             "Y(max=" + String(features.accelYMax, 2) +
                             " mean=" + String(features.accelYMean, 2) +
                             " pos=" + String(features.accelYPosPeak, 2) +
                             " neg=" + String(features.accelYNegPeak, 2) +
                             " signed=" + String(features.accelYSignedMean, 2) + ") " +
                             "Z(max=" + String(features.accelZMax, 2) +
                             " mean=" + String(features.accelZMean, 2) +
                             " pos=" + String(features.accelZPosPeak, 2) +
                             " neg=" + String(features.accelZNegPeak, 2) +
                             " signed=" + String(features.accelZSignedMean, 2) + ")");

    Logger::getInstance().log("[OrientationFeatures] Accel (world frame - INTUITIVE): X(max=" + String(features.worldAccelXMax, 2) +
                             " mean=" + String(features.worldAccelXMean, 2) +
                             " pos=" + String(features.worldAccelXPosPeak, 2) +
                             " neg=" + String(features.worldAccelXNegPeak, 2) +
                             " signed=" + String(features.worldAccelXSignedMean, 2) + ") " +
                             "Y(max=" + String(features.worldAccelYMax, 2) +
                             " mean=" + String(features.worldAccelYMean, 2) +
                             " pos=" + String(features.worldAccelYPosPeak, 2) +
                             " neg=" + String(features.worldAccelYNegPeak, 2) +
                             " signed=" + String(features.worldAccelYSignedMean, 2) + ") " +
                             "Z(max=" + String(features.worldAccelZMax, 2) +
                             " mean=" + String(features.worldAccelZMean, 2) +
                             " pos=" + String(features.worldAccelZPosPeak, 2) +
                             " neg=" + String(features.worldAccelZNegPeak, 2) +
                             " signed=" + String(features.worldAccelZSignedMean, 2) + ")");

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

    // Check for shake using WORLD-FRAME ACCELEROMETER data
    // World-frame = orientation-independent, INTUITIVE for user
    // No matter how device is tilted, moving it LEFT will always trigger SHAKE_X_NEG (for example)
    // Shake = rapid linear motion along one axis in world coordinates
    // Use peak values to detect quick movements
    const float accelShakeThresholdPeak = 0.25f;  // Lowered from 0.3g to 0.25g for better sensitivity
    const float accelShakeThresholdMean = 0.12f;  // Lowered from 0.15g to 0.12g

    // Check each axis independently using WORLD-FRAME accelerometer (rotation-corrected)
    bool xShake = (features.worldAccelXMax > accelShakeThresholdPeak) ||
                  (features.worldAccelXMean > accelShakeThresholdMean && features.worldAccelXStd > 0.08f);
    bool yShake = (features.worldAccelYMax > accelShakeThresholdPeak) ||
                  (features.worldAccelYMean > accelShakeThresholdMean && features.worldAccelYStd > 0.08f);
    bool zShake = (features.worldAccelZMax > accelShakeThresholdPeak) ||
                  (features.worldAccelZMean > accelShakeThresholdMean && features.worldAccelZStd > 0.08f);

    if (xShake || yShake || zShake) {
        // Determine which axis has the strongest shake
        // Use combination of peak and mean for robustness
        float xStrength = features.worldAccelXMax * 0.7f + features.worldAccelXMean * 0.3f;
        float yStrength = features.worldAccelYMax * 0.7f + features.worldAccelYMean * 0.3f;
        float zStrength = features.worldAccelZMax * 0.7f + features.worldAccelZMean * 0.3f;

        float maxStrength = fmaxf(xStrength, fmaxf(yStrength, zStrength));

        auto chooseDirection = [&](OrientationType posType, OrientationType negType,
                                   float posPeak, float negPeak, float signedMean) -> OrientationType {
            const float dominanceFactor = 1.15f;  // Reduced from 1.2 to 1.15 (15% instead of 20%)
            const float meanBias = 0.04f;  // Reduced from 0.05 to 0.04

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

        // Require dominant axis to be clearly stronger (reduced from 1.25 to 1.15 for better detection)
        const float axisDominanceThreshold = 1.15f;
        if (xStrength >= maxStrength * 0.75f && xStrength > yStrength * axisDominanceThreshold && xStrength > zStrength * axisDominanceThreshold) {
            _confidence = 0.80f + fminf(xStrength / (accelShakeThresholdPeak * 2.0f), 0.10f);
            OrientationType dir = chooseDirection(ORIENT_SHAKE_X_POS, ORIENT_SHAKE_X_NEG,
                                                  features.worldAccelXPosPeak, features.worldAccelXNegPeak,
                                                  features.worldAccelXSignedMean);
            if (dir != ORIENT_UNKNOWN) {
                return dir;
            }
        } else if (yStrength >= maxStrength * 0.75f && yStrength > xStrength * axisDominanceThreshold && yStrength > zStrength * axisDominanceThreshold) {
            _confidence = 0.80f + fminf(yStrength / (accelShakeThresholdPeak * 2.0f), 0.10f);
            OrientationType dir = chooseDirection(ORIENT_SHAKE_Y_POS, ORIENT_SHAKE_Y_NEG,
                                                  features.worldAccelYPosPeak, features.worldAccelYNegPeak,
                                                  features.worldAccelYSignedMean);
            if (dir != ORIENT_UNKNOWN) {
                return dir;
            }
        } else if (zStrength >= maxStrength * 0.75f && zStrength > xStrength * axisDominanceThreshold && zStrength > yStrength * axisDominanceThreshold) {
            _confidence = 0.80f + fminf(zStrength / (accelShakeThresholdPeak * 2.0f), 0.10f);
            OrientationType dir = chooseDirection(ORIENT_SHAKE_Z_POS, ORIENT_SHAKE_Z_NEG,
                                                  features.worldAccelZPosPeak, features.worldAccelZNegPeak,
                                                  features.worldAccelZSignedMean);
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
