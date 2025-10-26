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

#ifndef IGESTURE_RECOGNIZER_H
#define IGESTURE_RECOGNIZER_H

#include "gestureRead.h"
#include <Arduino.h>

/**
 * Sensor-specific gesture recognition type
 */
enum SensorGestureMode {
    SENSOR_MODE_MPU6050 = 0,    // MPU6050: Shape + Orientation (gyro available)
    SENSOR_MODE_ADXL345,        // ADXL345: Legacy KNN (no gyro)
    SENSOR_MODE_AUTO            // Auto-detect based on sensor type
};

/**
 * Unified gesture result structure
 */
struct GestureRecognitionResult {
    int gestureID;              // Gesture ID (-1 if unknown, 0-8 for custom, 100+ for predefined)
    float confidence;           // Confidence score 0-1
    SensorGestureMode sensorMode; // Which sensor mode was used
    String gestureName;         // Human-readable name

    // Mode-specific data (optional)
    void* modeSpecificData;     // Can hold ShapeType, OrientationType, etc.

    GestureRecognitionResult()
        : gestureID(-1), confidence(0.0f), sensorMode(SENSOR_MODE_AUTO),
          gestureName("unknown"), modeSpecificData(nullptr) {}
};

/**
 * Base interface for sensor-specific gesture recognizers
 * Each sensor type (MPU6050, ADXL345) implements this interface
 */
class IGestureRecognizer {
public:
    virtual ~IGestureRecognizer() {}

    /**
     * Initialize recognizer with sensor configuration
     */
    virtual bool init(const String& sensorType) = 0;

    /**
     * Recognize gesture from sample buffer
     * @param buffer Raw sensor samples
     * @return Recognition result with ID, confidence, and metadata
     */
    virtual GestureRecognitionResult recognize(SampleBuffer* buffer) = 0;

    /**
     * Train/save a custom gesture with given ID
     * @param buffer Sample buffer containing gesture data
     * @param gestureID Custom gesture ID (0-8)
     * @return true if saved successfully
     */
    virtual bool trainCustomGesture(SampleBuffer* buffer, uint8_t gestureID) = 0;

    /**
     * Check if this recognizer supports custom gesture training
     */
    virtual bool supportsCustomTraining() const = 0;

    /**
     * Get sensor mode name for logging
     */
    virtual String getModeName() const = 0;

    /**
     * Set confidence threshold (0-1)
     */
    virtual void setConfidenceThreshold(float threshold) = 0;

    /**
     * Get current confidence threshold
     */
    virtual float getConfidenceThreshold() const = 0;
};

#endif // IGESTURE_RECOGNIZER_H
