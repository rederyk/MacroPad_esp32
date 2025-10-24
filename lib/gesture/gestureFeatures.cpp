/*
 * ESP32 MacroPad Project
 * Copyright (C) [2025] [Enrico Mori]
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


#include "gestureFeatures.h"
#include <math.h>
#include <cmath>

static constexpr float kGyroFeatureWeight = 0.05f;

void extractFeatures(SampleBuffer *buffer, float *features) {
    if (!buffer || !buffer->samples || buffer->sampleCount == 0) {
        if (features) {
            for (size_t i = 0; i < TOTAL_FEATURES; ++i) {
                features[i] = 0.0f;
            }
        }
        return;
    }

    for (size_t i = 0; i < TOTAL_FEATURES; ++i) {
        features[i] = 0.0f;
    }

    // Variables to hold intermediate values
    float sumX = 0, sumY = 0, sumZ = 0;
    float sumSqX = 0, sumSqY = 0, sumSqZ = 0;
    float maxX = -INFINITY, maxY = -INFINITY, maxZ = -INFINITY;
    float minX = INFINITY, minY = INFINITY, minZ = INFINITY;
    int zeroCrossingsX = 0, zeroCrossingsY = 0, zeroCrossingsZ = 0;
    uint16_t accelSamples = 0;

    // Gyroscope magnitude metrics
    float sumGyroMag = 0.0f;
    float sumSqGyroMag = 0.0f;
    float maxGyroMag = -INFINITY;
    float minGyroMag = INFINITY;
    uint16_t gyroSamples = 0;

    // Previous samples for zero-crossing calculation
    float prevX = 0, prevY = 0, prevZ = 0;
    int isFirstSample = 1;

    // Iterate through all samples
    for (int i = 0; i < buffer->sampleCount; i++) {
        const Sample &sample = buffer->samples[i];
        const float x = sample.x;
        const float y = sample.y;
        const float z = sample.z;
        const bool accelValid = std::isfinite(x) && std::isfinite(y) && std::isfinite(z);

        if (accelValid) {
            ++accelSamples;

            // Accumulate sums and squared sums
            sumX += x;
            sumY += y;
            sumZ += z;

            sumSqX += x * x;
            sumSqY += y * y;
            sumSqZ += z * z;

            // Update max and min values
            if (x > maxX) maxX = x;
            if (y > maxY) maxY = y;
            if (z > maxZ) maxZ = z;

            if (x < minX) minX = x;
            if (y < minY) minY = y;
            if (z < minZ) minZ = z;

            // Calculate zero-crossings
            if (!isFirstSample) {
                if ((x > 0 && prevX < 0) || (x < 0 && prevX > 0)) zeroCrossingsX++;
                if ((y > 0 && prevY < 0) || (y < 0 && prevY > 0)) zeroCrossingsY++;
                if ((z > 0 && prevZ < 0) || (z < 0 && prevZ > 0)) zeroCrossingsZ++;
            }

            prevX = x;
            prevY = y;
            prevZ = z;
            isFirstSample = 0;
        }

        if (sample.gyroValid) {
            const float gyroMagnitude = sqrtf(
                sample.gyroX * sample.gyroX +
                sample.gyroY * sample.gyroY +
                sample.gyroZ * sample.gyroZ);
            if (std::isfinite(gyroMagnitude)) {
                sumGyroMag += gyroMagnitude;
                sumSqGyroMag += gyroMagnitude * gyroMagnitude;
                if (gyroMagnitude > maxGyroMag) maxGyroMag = gyroMagnitude;
                if (gyroMagnitude < minGyroMag) minGyroMag = gyroMagnitude;
                ++gyroSamples;
            }
        }
    }

    auto sanitize = [](float value) -> float {
        return std::isfinite(value) ? value : 0.0f;
    };

    if (accelSamples == 0) {
        // No valid accelerometer data collected; accelerometer features stay zero
        maxX = maxY = maxZ = 0.0f;
        minX = minY = minZ = 0.0f;
    }

    const float accelSampleInv = accelSamples > 0 ? 1.0f / accelSamples : 0.0f;
    const float meanX = sumX * accelSampleInv;
    const float meanY = sumY * accelSampleInv;
    const float meanZ = sumZ * accelSampleInv;

    const float varianceX = accelSamples > 0 ? (sumSqX * accelSampleInv - meanX * meanX) : 0.0f;
    const float varianceY = accelSamples > 0 ? (sumSqY * accelSampleInv - meanY * meanY) : 0.0f;
    const float varianceZ = accelSamples > 0 ? (sumSqZ * accelSampleInv - meanZ * meanZ) : 0.0f;

    const float stdDevX = sqrtf(fmaxf(varianceX, 0.0f));
    const float stdDevY = sqrtf(fmaxf(varianceY, 0.0f));
    const float stdDevZ = sqrtf(fmaxf(varianceZ, 0.0f));

    // Populate the feature vector for accelerometer axes
    features[0] = sanitize(meanX);
    features[1] = sanitize(stdDevX);
    features[2] = sanitize(sumSqX);
    features[3] = sanitize(maxX);
    features[4] = sanitize(minX);

    features[5] = sanitize(meanY);
    features[6] = sanitize(stdDevY);
    features[7] = sanitize(sumSqY);
    features[8] = sanitize(maxY);
    features[9] = sanitize(minY);

    features[10] = sanitize(meanZ);
    features[11] = sanitize(stdDevZ);
    features[12] = sanitize(sumSqZ);
    features[13] = sanitize(maxZ);
    features[14] = sanitize(minZ);

    // Populate gyro magnitude features with lower weight to reduce their impact
    const size_t gyroOffset = TOTAL_ACCEL_FEATURES;
    if (gyroSamples == 0) {
        features[gyroOffset + 0] = 0.0f;
        features[gyroOffset + 1] = 0.0f;
        features[gyroOffset + 2] = 0.0f;
        features[gyroOffset + 3] = 0.0f;
        features[gyroOffset + 4] = 0.0f;
        return;
    }

    const float gyroSampleInv = 1.0f / gyroSamples;
    const float meanGyroMag = sumGyroMag * gyroSampleInv;
    const float varianceGyroMag = fmaxf(sumSqGyroMag * gyroSampleInv - meanGyroMag * meanGyroMag, 0.0f);
    const float stdGyroMag = sqrtf(varianceGyroMag);
    const float energyGyroMag = sumSqGyroMag;
    const float maxGyro = maxGyroMag;
    const float minGyro = minGyroMag == INFINITY ? 0.0f : minGyroMag;

    features[gyroOffset + 0] = sanitize(meanGyroMag * kGyroFeatureWeight);
    features[gyroOffset + 1] = sanitize(stdGyroMag * kGyroFeatureWeight);
    features[gyroOffset + 2] = sanitize(energyGyroMag * kGyroFeatureWeight);
    features[gyroOffset + 3] = sanitize(maxGyro * kGyroFeatureWeight);
    features[gyroOffset + 4] = sanitize(minGyro * kGyroFeatureWeight);
}
