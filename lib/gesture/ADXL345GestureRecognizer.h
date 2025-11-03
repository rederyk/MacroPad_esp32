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

#ifndef ADXL345_GESTURE_RECOGNIZER_H
#define ADXL345_GESTURE_RECOGNIZER_H

#include "IGestureRecognizer.h"

/**
 * ADXL345-specific gesture recognition
 * Detects only swipe left/right and shake gestures using accelerometer data.
 */
class ADXL345GestureRecognizer : public IGestureRecognizer {
public:
    ADXL345GestureRecognizer();
    ~ADXL345GestureRecognizer() override;

    // IGestureRecognizer interface
    bool init(const String& sensorType) override;
    GestureRecognitionResult recognize(SampleBuffer* buffer) override;
    String getModeName() const override { return "Swipe+Shake (Accel only)"; }
    void setConfidenceThreshold(float threshold) override { _confidenceThreshold = threshold; }
    float getConfidenceThreshold() const override { return _confidenceThreshold; }

private:
    float _confidenceThreshold;
};

#endif // ADXL345_GESTURE_RECOGNIZER_H
