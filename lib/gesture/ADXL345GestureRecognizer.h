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
#include "gestureStorage.h"
#include "gestureFeatures.h"
#include "gestureFilter.h"
#include "gestureNormalize.h"

/**
 * ADXL345-specific gesture recognition
 * Uses legacy KNN-based recognition with feature extraction
 * Supports custom gesture training and storage
 */
class ADXL345GestureRecognizer : public IGestureRecognizer {
public:
    ADXL345GestureRecognizer();
    ~ADXL345GestureRecognizer() override;

    // IGestureRecognizer interface
    bool init(const String& sensorType) override;
    GestureRecognitionResult recognize(SampleBuffer* buffer) override;
    bool trainCustomGesture(SampleBuffer* buffer, uint8_t gestureID) override;
    bool supportsCustomTraining() const override { return true; }
    String getModeName() const override { return "ADXL345 (KNN)"; }
    void setConfidenceThreshold(float threshold) override { _confidenceThreshold = threshold; }
    float getConfidenceThreshold() const override { return _confidenceThreshold; }

    /**
     * Set K value for KNN algorithm
     */
    void setKValue(int k) { _kValue = k; }

    /**
     * Get K value
     */
    int getKValue() const { return _kValue; }

private:
    float _confidenceThreshold;
    int _kValue; // K for KNN (default 3)
    GestureStorage _storage;

    // Helper methods
    void extractFeatures(SampleBuffer* buffer, float* features);
    int findKNNMatch(const float* features, float& confidence);
    float calculateDistance(const float* features1, const float* features2, size_t count);
    String gestureIDToName(int gestureID) const;
};

#endif // ADXL345_GESTURE_RECOGNIZER_H
