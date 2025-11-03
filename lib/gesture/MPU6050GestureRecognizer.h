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

/**
 * MPU6050-specific gesture recognition - SIMPLIFIED VERSION
 * Recognizes only: SWIPE_LEFT, SWIPE_RIGHT, SHAKE
 * Uses direct accelerometer and gyroscope data analysis
 */
class MPU6050GestureRecognizer : public IGestureRecognizer {
public:
    MPU6050GestureRecognizer();
    ~MPU6050GestureRecognizer() override;

    // IGestureRecognizer interface
    bool init(const String& sensorType) override;
    GestureRecognitionResult recognize(SampleBuffer* buffer) override;
    String getModeName() const override { return "Swipe+Shake (Accel+Gyro)"; }
    void setConfidenceThreshold(float threshold) override { _confidenceThreshold = threshold; }
    float getConfidenceThreshold() const override { return _confidenceThreshold; }

private:
    float _confidenceThreshold;
};

#endif // MPU6050_GESTURE_RECOGNIZER_H
