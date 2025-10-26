/*
 * ESP32 MacroPad Project
 * Copyright (C) [2025] [Enrico Mori]
 *
 * Automatic Axis Calibration for Accelerometer/Gyroscope
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

#ifndef GESTURE_AXIS_CALIBRATION_H
#define GESTURE_AXIS_CALIBRATION_H

#include <Arduino.h>
#include "MotionSensor.h"

/**
 * Axis calibration result
 */
struct AxisCalibrationResult {
    String axisMap;     // e.g., "xyz", "yxz", "yzx"
    String axisDir;     // e.g., "+++", "+-+", "-++"
    bool success;       // Calibration successful
    float confidence;   // Confidence of calibration (0-1)

    AxisCalibrationResult() : axisMap("xyz"), axisDir("+++"), success(false), confidence(0.0f) {}
};

/**
 * Axis Calibration Class
 *
 * Automatically determines correct axis mapping by having user hold device
 * in its normal orientation (as when pressing buttons).
 *
 * The calibration assumes:
 * - Device held vertically with buttons facing user
 * - Gravity vector points downward (Z-axis should read ~-1g)
 * - X-axis should be horizontal (left-right)
 * - Y-axis should be horizontal (forward-backward)
 */
class AxisCalibration {
public:
    AxisCalibration();

    /**
     * Perform automatic axis calibration
     *
     * User should hold device in normal position (buttons facing them, vertical)
     * and keep it still during calibration.
     *
     * @param sensor Motion sensor to calibrate
     * @param samplingTimeMs Time to collect samples (default 2000ms)
     * @return AxisCalibrationResult with detected axis mapping
     */
    AxisCalibrationResult calibrate(MotionSensor* sensor, uint32_t samplingTimeMs = 2000);

    /**
     * Apply calibration result to sensor configuration
     *
     * @param sensor Motion sensor to configure
     * @param result Calibration result from calibrate()
     * @return true if configuration applied successfully
     */
    bool applyCalibration(MotionSensor* sensor, const AxisCalibrationResult& result);

    /**
     * Save calibration to config file
     *
     * @param result Calibration result
     * @param configPath Path to config.json file
     * @return true if saved successfully
     */
    bool saveToConfig(const AxisCalibrationResult& result, const char* configPath = "/config.json");

private:
    static constexpr float GRAVITY = 9.80665f;  // Standard gravity (m/s²)
    static constexpr float GRAVITY_TOLERANCE = 0.2f;  // ±0.2g tolerance

    /**
     * Collect samples from sensor
     */
    bool collectSamples(MotionSensor* sensor, uint32_t durationMs, float* avgX, float* avgY, float* avgZ);

    /**
     * Determine which physical axis corresponds to which logical axis
     * Returns index: 0=X, 1=Y, 2=Z
     */
    int findGravityAxis(float x, float y, float z);

    /**
     * Determine axis directions based on gravity vector
     */
    void determineAxisMapping(float rawX, float rawY, float rawZ,
                             String& axisMap, String& axisDir, float& confidence);

    /**
     * Calculate confidence score for calibration
     */
    float calculateConfidence(float x, float y, float z);
};

#endif // GESTURE_AXIS_CALIBRATION_H
