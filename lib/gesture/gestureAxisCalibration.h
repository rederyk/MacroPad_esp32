/*
 * ESP32 MacroPad Project
 * Copyright (C) [2025] [Enrico Mori]
 *
 * Automatic Axis Calibration for Accelerometer/Gyroscope
 */

#ifndef GESTURE_AXIS_CALIBRATION_H
#define GESTURE_AXIS_CALIBRATION_H

#include <Arduino.h>
#include "gestureRead.h"

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
 * Automatically determines correct axis mapping by sampling at 100Hz
 * using GestureRead infrastructure.
 */
class AxisCalibration {
public:
    AxisCalibration() = default;

    /**
     * Perform automatic axis calibration
     *
     * @param gestureRead GestureRead instance to use for sampling
     * @param samplingTimeMs Time to collect samples (default 2000ms)
     * @return AxisCalibrationResult with detected axis mapping
     */
    AxisCalibrationResult calibrate(GestureRead* gestureRead, uint32_t samplingTimeMs = 2000);

    /**
     * Save calibration to config file
     *
     * @param result Calibration result
     * @param configPath Path to config.json file
     * @return true if saved successfully
     */
    bool saveToConfig(const AxisCalibrationResult& result, const char* configPath = "/config.json");

private:
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
};

#endif // GESTURE_AXIS_CALIBRATION_H
