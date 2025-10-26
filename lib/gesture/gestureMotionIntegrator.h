/*
 * ESP32 MacroPad Project
 * Copyright (C) [2025] [Enrico Mori]
 *
 * Motion Integrator for Gesture Path Tracking
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

#ifndef GESTURE_MOTION_INTEGRATOR_H
#define GESTURE_MOTION_INTEGRATOR_H

#include "gestureRead.h"
#include "MadgwickAHRS.h"
#include <cmath>

// 3D vector structure
struct Vector3 {
    float x, y, z;

    Vector3() : x(0), y(0), z(0) {}
    Vector3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    // Vector operations
    Vector3 operator+(const Vector3& v) const { return Vector3(x + v.x, y + v.y, z + v.z); }
    Vector3 operator-(const Vector3& v) const { return Vector3(x - v.x, y - v.y, z - v.z); }
    Vector3 operator*(float s) const { return Vector3(x * s, y * s, z * s); }
    Vector3 operator/(float s) const { return Vector3(x / s, y / s, z / s); }

    float magnitude() const { return sqrtf(x * x + y * y + z * z); }
    float magnitudeSquared() const { return x * x + y * y + z * z; }

    Vector3 normalized() const {
        float mag = magnitude();
        return mag > 0 ? (*this / mag) : Vector3(0, 0, 0);
    }

    static float dot(const Vector3& a, const Vector3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    static Vector3 cross(const Vector3& a, const Vector3& b) {
        return Vector3(
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        );
    }

    static float distance(const Vector3& a, const Vector3& b) {
        return (a - b).magnitude();
    }
};

// Motion path structure
struct MotionPath {
    static constexpr uint16_t MAX_PATH_POINTS = 300;

    Vector3 positions[MAX_PATH_POINTS];  // Position samples
    Vector3 velocities[MAX_PATH_POINTS]; // Velocity samples (for debugging)
    uint16_t count;                       // Number of valid points

    float totalLength;      // Total path length (sum of distances)
    float maxDisplacement;  // Maximum distance from origin
    bool isClosed;          // Is path closed (start ≈ end)
    bool isValid;           // Is integration result valid

    MotionPath() : count(0), totalLength(0), maxDisplacement(0), isClosed(false), isValid(false) {}
};

/**
 * Motion Integrator class
 *
 * Integrates accelerometer data to estimate relative motion path.
 * Uses Madgwick filter to remove gravity and correct orientation.
 *
 * Important: Due to IMU drift, this is only accurate for short gestures (1-3 seconds).
 * Longer gestures will accumulate significant position errors.
 */
class MotionIntegrator {
public:
    MotionIntegrator();

    /**
     * Integrate motion from sample buffer to get gesture path
     *
     * @param buffer Sample buffer with accelerometer and gyroscope data
     * @param path Output motion path structure
     * @return true if integration successful, false if data invalid
     */
    bool integrate(SampleBuffer* buffer, MotionPath& path);

    /**
     * Reset integrator state (call before new gesture)
     */
    void reset();

    /**
     * Set maximum integration time (default 3.0 seconds)
     * Longer times lead to more drift
     */
    void setMaxIntegrationTime(float seconds);

    /**
     * Set drift compensation threshold
     * If estimated position seems unrealistic, integration is rejected
     */
    void setDriftThreshold(float threshold);

    /**
     * Set gravity removal method
     * true: Use Madgwick filter (more accurate, slower)
     * false: Simple high-pass filter (faster, less accurate)
     */
    void setUseMadgwick(bool enable);

private:
    MadgwickAHRS _madgwick;

    float _maxIntegrationTime;  // Maximum time to integrate (seconds)
    float _driftThreshold;      // Max reasonable displacement (meters)
    bool _useMadgwick;          // Use Madgwick for gravity removal

    // Internal state
    Vector3 _velocity;          // Current velocity estimate
    Vector3 _position;          // Current position estimate

    /**
     * Remove gravity from accelerometer reading using orientation
     */
    Vector3 removeGravity(float ax, float ay, float az, const float R[3][3]) const;

    /**
     * Validate path for drift/unrealistic motion
     */
    bool validatePath(const MotionPath& path) const;

    /**
     * Calculate path statistics (length, closure, etc)
     */
    void calculatePathStats(MotionPath& path);
};

#endif // GESTURE_MOTION_INTEGRATOR_H
