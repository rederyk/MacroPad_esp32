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

#ifndef MPU6050_GESTURE_RECOGNIZER_H
#define MPU6050_GESTURE_RECOGNIZER_H

#include "IGestureRecognizer.h"
#include "PredefinedShapeRecognizer.h"
#include "gestureOrientationFeatures.h"

/**
 * MPU6050-specific gesture recognition
 * Uses dual-mode recognition: Shape + Orientation
 * DOES NOT support custom training - only recognizes predefined gestures
 */
class MPU6050GestureRecognizer : public IGestureRecognizer {
public:
    MPU6050GestureRecognizer();
    ~MPU6050GestureRecognizer() override;

    // IGestureRecognizer interface
    bool init(const String& sensorType) override;
    GestureRecognitionResult recognize(SampleBuffer* buffer) override;
    String getModeName() const override { return "MPU6050 (Shape+Orientation)"; }
    void setConfidenceThreshold(float threshold) override { _confidenceThreshold = threshold; }
    float getConfidenceThreshold() const override { return _confidenceThreshold; }

private:
    // Recognition components
    PredefinedShapeRecognizer _shapeRecognizer;
    OrientationFeatureExtractor _orientationExtractor;

    float _confidenceThreshold;

    // Helper methods
    GestureRecognitionResult recognizeShape(SampleBuffer* buffer);
    GestureRecognitionResult recognizeOrientation(SampleBuffer* buffer);

    float calculateGyroActivity(SampleBuffer* buffer) const;
    bool hasOrientationChange(SampleBuffer* buffer) const;

    int orientationTypeToID(OrientationType orientation) const;
    String orientationTypeToName(OrientationType orientation) const;
};

#endif // MPU6050_GESTURE_RECOGNIZER_H
