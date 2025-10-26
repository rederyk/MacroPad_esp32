/*
 * ESP32 MacroPad Project
 * Copyright (C) [2025] [Enrico Mori]
 *
 * Motion Integrator Implementation
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

#include "gestureMotionIntegrator.h"
#include "Logger.h"

// Gravity constant (m/s^2)
static constexpr float GRAVITY = 9.80665f;

MotionIntegrator::MotionIntegrator()
    : _madgwick(100.0f, 0.1f),
      _maxIntegrationTime(3.0f),
      _driftThreshold(2.0f),
      _useMadgwick(true),
      _velocity(0, 0, 0),
      _position(0, 0, 0)
{
}

void MotionIntegrator::reset()
{
    _velocity = Vector3(0, 0, 0);
    _position = Vector3(0, 0, 0);
    _madgwick.reset();
}

void MotionIntegrator::setMaxIntegrationTime(float seconds)
{
    _maxIntegrationTime = seconds;
}

void MotionIntegrator::setDriftThreshold(float threshold)
{
    _driftThreshold = threshold;
}

void MotionIntegrator::setUseMadgwick(bool enable)
{
    _useMadgwick = enable;
}

bool MotionIntegrator::integrate(SampleBuffer* buffer, MotionPath& path)
{
    if (!buffer || buffer->sampleCount == 0 || !buffer->samples) {
        Logger::getInstance().log("[MotionIntegrator] Invalid buffer");
        return false;
    }

    // Reset state for new gesture
    reset();
    path = MotionPath();

    // Configure Madgwick with buffer's sample rate
    if (_useMadgwick && buffer->sampleHZ > 0) {
        _madgwick.setSampleFrequency(static_cast<float>(buffer->sampleHZ));
    }

    const float dt = 1.0f / buffer->sampleHZ; // Time step in seconds
    const uint16_t maxSamples = static_cast<uint16_t>(_maxIntegrationTime * buffer->sampleHZ);
    const uint16_t samplesToProcess = buffer->sampleCount < maxSamples ? buffer->sampleCount : maxSamples;

    Logger::getInstance().log("[MotionIntegrator] Processing " + String(samplesToProcess) +
                             " samples at " + String(buffer->sampleHZ) + "Hz, dt=" + String(dt, 4) + "s");

    // Process each sample
    for (uint16_t i = 0; i < samplesToProcess && path.count < MotionPath::MAX_PATH_POINTS; i++) {
        const Sample& sample = buffer->samples[i];

        Vector3 linearAccel;

        if (_useMadgwick && sample.gyroValid) {
            // Update Madgwick filter with current sample
            _madgwick.update(
                sample.gyroX, sample.gyroY, sample.gyroZ,
                sample.x, sample.y, sample.z
            );

            // Get rotation matrix to transform accelerometer to world frame
            float R[3][3];
            _madgwick.getRotationMatrix(R);

            // Remove gravity using rotation matrix
            linearAccel = removeGravity(sample.x, sample.y, sample.z, R);
        } else {
            // Simple high-pass filter: assume Z-axis points down
            // This is less accurate but faster and works without gyro
            linearAccel.x = sample.x;
            linearAccel.y = sample.y;
            linearAccel.z = sample.z + 1.0f; // Remove 1g gravity on Z-axis

            // Sanity check: if total accel is huge, likely noise
            if (linearAccel.magnitude() > 5.0f) {
                linearAccel = Vector3(0, 0, 0);
            }
        }

        // Convert from g to m/s^2
        linearAccel = linearAccel * GRAVITY;

        // Integrate acceleration to get velocity (first integration)
        _velocity = _velocity + (linearAccel * dt);

        // Integrate velocity to get position (second integration)
        _position = _position + (_velocity * dt);

        // Store in path
        path.positions[path.count] = _position;
        path.velocities[path.count] = _velocity;
        path.count++;
    }

    // Calculate path statistics
    calculatePathStats(path);

    // Validate path for drift
    path.isValid = validatePath(path);

    if (path.isValid) {
        Logger::getInstance().log("[MotionIntegrator] Path valid: length=" + String(path.totalLength, 3) +
                                 "m, maxDisp=" + String(path.maxDisplacement, 3) +
                                 "m, closed=" + String(path.isClosed));
    } else {
        Logger::getInstance().log("[MotionIntegrator] Path INVALID (likely drift): maxDisp=" +
                                 String(path.maxDisplacement, 3) + "m exceeds threshold");
    }

    return path.isValid;
}

Vector3 MotionIntegrator::removeGravity(float ax, float ay, float az, const float R[3][3]) const
{
    // Gravity vector in world frame (points down)
    const Vector3 gravityWorld(0, 0, -1.0f);

    // Transform gravity to body frame using rotation matrix transpose (inverse)
    Vector3 gravityBody(
        R[0][0] * gravityWorld.x + R[1][0] * gravityWorld.y + R[2][0] * gravityWorld.z,
        R[0][1] * gravityWorld.x + R[1][1] * gravityWorld.y + R[2][1] * gravityWorld.z,
        R[0][2] * gravityWorld.x + R[1][2] * gravityWorld.y + R[2][2] * gravityWorld.z
    );

    // Remove gravity from accelerometer reading
    return Vector3(ax, ay, az) - gravityBody;
}

bool MotionIntegrator::validatePath(const MotionPath& path) const
{
    if (path.count == 0) {
        return false;
    }

    // Check if maximum displacement is reasonable
    // For gestures drawn in air, should be < 1-2 meters
    if (path.maxDisplacement > _driftThreshold) {
        Logger::getInstance().log("[MotionIntegrator] Drift detected: " + String(path.maxDisplacement, 3) +
                                 "m exceeds " + String(_driftThreshold, 3) + "m");
        return false;
    }

    // Check if total path length is reasonable
    // Very long paths (>5m) in short time suggest drift
    if (path.totalLength > _driftThreshold * 3.0f) {
        Logger::getInstance().log("[MotionIntegrator] Path too long: " + String(path.totalLength, 3) + "m");
        return false;
    }

    return true;
}

void MotionIntegrator::calculatePathStats(MotionPath& path)
{
    if (path.count < 2) {
        path.totalLength = 0;
        path.maxDisplacement = 0;
        path.isClosed = false;
        return;
    }

    // Calculate total path length (sum of segment distances)
    path.totalLength = 0;
    for (uint16_t i = 1; i < path.count; i++) {
        path.totalLength += Vector3::distance(path.positions[i], path.positions[i - 1]);
    }

    // Find maximum displacement from origin
    path.maxDisplacement = 0;
    for (uint16_t i = 0; i < path.count; i++) {
        float dist = path.positions[i].magnitude();
        if (dist > path.maxDisplacement) {
            path.maxDisplacement = dist;
        }
    }

    // Check if path is closed (start point close to end point)
    const float closeThreshold = 0.1f; // 10cm
    float startEndDist = Vector3::distance(path.positions[0], path.positions[path.count - 1]);
    path.isClosed = (startEndDist < closeThreshold);
}
