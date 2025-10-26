/*
 * ESP32 MacroPad Project
 * Copyright (C) [2025] [Enrico Mori]
 *
 * Madgwick AHRS Filter Implementation
 * Based on: "An efficient orientation filter for inertial and
 * inertial/magnetic sensor arrays" by Sebastian O.H. Madgwick
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

#ifndef MADGWICK_AHRS_H
#define MADGWICK_AHRS_H

#include <cmath>

/**
 * Madgwick AHRS (Attitude and Heading Reference System) Filter
 *
 * Fuses accelerometer and gyroscope data to estimate device orientation
 * as a quaternion. The filter corrects gyroscope drift using accelerometer
 * gravity vector measurements.
 *
 * Key features:
 * - Lightweight: ~2KB memory footprint
 * - Fast: Optimized for embedded systems
 * - Accurate: Corrects gyro drift continuously
 * - Stable: Handles rapid movements and slow drifts
 */
class MadgwickAHRS {
public:
    /**
     * Constructor
     * @param sampleFreq Sample frequency in Hz (e.g., 100 for 100Hz)
     * @param beta Filter gain (default 0.1). Higher = faster convergence but more noise
     */
    MadgwickAHRS(float sampleFreq = 100.0f, float beta = 0.1f);

    /**
     * Update filter with new IMU measurements (6-DOF: accel + gyro)
     *
     * @param gx Gyroscope X-axis in rad/s
     * @param gy Gyroscope Y-axis in rad/s
     * @param gz Gyroscope Z-axis in rad/s
     * @param ax Accelerometer X-axis in g (gravity units)
     * @param ay Accelerometer Y-axis in g
     * @param az Accelerometer Z-axis in g
     */
    void update(float gx, float gy, float gz, float ax, float ay, float az);

    /**
     * Get current orientation as quaternion
     * @param q0 Output: scalar part (w)
     * @param q1 Output: vector i component (x)
     * @param q2 Output: vector j component (y)
     * @param q3 Output: vector k component (z)
     */
    void getQuaternion(float &q0, float &q1, float &q2, float &q3) const;

    /**
     * Get Euler angles from quaternion (Tait-Bryan angles, Z-Y-X convention)
     * @param roll Output: rotation around X-axis in radians
     * @param pitch Output: rotation around Y-axis in radians
     * @param yaw Output: rotation around Z-axis in radians
     */
    void getEulerAngles(float &roll, float &pitch, float &yaw) const;

    /**
     * Get rotation matrix (3x3) from quaternion
     * Useful for transforming vectors between body and world frames
     * @param R Output: 3x3 rotation matrix (row-major order)
     */
    void getRotationMatrix(float R[3][3]) const;

    /**
     * Reset filter to initial state (identity quaternion)
     */
    void reset();

    /**
     * Set filter gain (beta parameter)
     * Higher values = faster convergence but more susceptible to noise
     * Typical range: 0.01 to 0.5
     */
    void setBeta(float beta);

    /**
     * Set sample frequency
     * Must match actual update rate for accurate integration
     */
    void setSampleFrequency(float freq);

    /**
     * Get current filter gain
     */
    float getBeta() const { return _beta; }

    /**
     * Get sample frequency
     */
    float getSampleFrequency() const { return _sampleFreq; }

private:
    // Quaternion components (orientation estimate)
    float _q0, _q1, _q2, _q3;

    // Filter parameters
    float _beta;        // Filter gain
    float _sampleFreq;  // Sample frequency (Hz)

    // Fast inverse square root helper
    static float invSqrt(float x);
};

#endif // MADGWICK_AHRS_H
