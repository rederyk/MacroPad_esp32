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

#include "ADXL345GestureRecognizer.h"
#include "Logger.h"
#include <cfloat>
#include <vector>
#include <algorithm>
#include <unordered_map>

#define ADXL345_GESTURES_FILE "/gesture_features_adxl345.json"

ADXL345GestureRecognizer::ADXL345GestureRecognizer()
    : _confidenceThreshold(0.5f), _kValue(3) {
}

ADXL345GestureRecognizer::~ADXL345GestureRecognizer() {
}

bool ADXL345GestureRecognizer::init(const String& sensorType) {
    if (sensorType != "adxl345") {
        Logger::getInstance().log("ADXL345GestureRecognizer: Wrong sensor type: " + sensorType);
        return false;
    }

    // Load stored gesture features
    if (!_storage.loadGestureFeatures()) {
        Logger::getInstance().log("ADXL345GestureRecognizer: No stored gestures, ready for training");
    }

    Logger::getInstance().log("ADXL345GestureRecognizer: Initialized for KNN recognition");
    return true;
}

GestureRecognitionResult ADXL345GestureRecognizer::recognize(SampleBuffer* buffer) {
    GestureRecognitionResult result;
    result.sensorMode = SENSOR_MODE_ADXL345;

    if (!buffer || buffer->sampleCount == 0) {
        Logger::getInstance().log("ADXL345GestureRecognizer: Empty buffer");
        return result;
    }

    // Apply low-pass filter directly to buffer
    applyLowPassFilter(buffer, 5.0f);

    // Normalize rotation
    normalizeRotation(buffer);

    // Extract features
    float features[MAX_FEATURES];
    extractFeatures(buffer, features);

    // Find KNN match
    float confidence = 0.0f;
    int gestureID = findKNNMatch(features, confidence);

    result.gestureID = gestureID;
    result.confidence = confidence;
    result.gestureName = gestureIDToName(gestureID);

    Logger::getInstance().log(String("ADXL345 KNN: ") + result.gestureName +
                            " (ID: " + String(gestureID) +
                            ", conf: " + String(confidence, 2) + ")");

    return result;
}

bool ADXL345GestureRecognizer::trainCustomGesture(SampleBuffer* buffer, uint8_t gestureID) {
    if (gestureID > 8) {
        Logger::getInstance().log("ADXL345: Invalid custom gesture ID (must be 0-8)");
        return false;
    }

    // Apply low-pass filter directly to buffer
    applyLowPassFilter(buffer, 5.0f);

    // Normalize rotation
    normalizeRotation(buffer);

    // Extract features
    float features[MAX_FEATURES];
    extractFeatures(buffer, features);

    // Save features with ID
    bool result = _storage.saveGestureFeature(gestureID, features, MAX_FEATURES);

    if (result) {
        Logger::getInstance().log(String("ADXL345: Saved custom gesture ID ") + String(gestureID));
    } else {
        Logger::getInstance().log("ADXL345: Failed to save custom gesture");
    }

    return result;
}

void ADXL345GestureRecognizer::extractFeatures(SampleBuffer* buffer, float* features) {
    ::extractFeatures(buffer, features);
}

int ADXL345GestureRecognizer::findKNNMatch(const float* features, float& confidence) {
    // Load feature matrix from binary
    size_t numGestures, numSamples, numFeatures;
    float*** matrix = _storage.loadMatrixFromBinary("/gestures_adxl345.bin", numGestures, numSamples, numFeatures);

    if (!matrix || numGestures == 0) {
        Logger::getInstance().log("ADXL345: No stored gestures found");
        confidence = 0.0f;
        return -1;
    }

    // Calculate normalization parameters
    std::vector<float> minValues(numFeatures, INFINITY);
    std::vector<float> maxValues(numFeatures, -INFINITY);

    for (size_t f = 0; f < numFeatures; f++) {
        for (size_t g = 0; g < numGestures; g++) {
            for (size_t s = 0; s < numSamples; s++) {
                float val = matrix[g][s][f];
                if (val < minValues[f]) minValues[f] = val;
                if (val > maxValues[f]) maxValues[f] = val;
            }
        }
    }

    // Normalize query features
    float normalizedFeatures[MAX_FEATURES];
    for (size_t f = 0; f < numFeatures; f++) {
        if (maxValues[f] == minValues[f]) {
            normalizedFeatures[f] = 0.0f;
            continue;
        }

        float range = maxValues[f] - minValues[f];
        normalizedFeatures[f] = (features[f] - minValues[f]) / range;

        // Clamp to [0,1]
        if (normalizedFeatures[f] < 0.0f) normalizedFeatures[f] = 0.0f;
        if (normalizedFeatures[f] > 1.0f) normalizedFeatures[f] = 1.0f;
    }

    // Normalize matrix
    for (size_t f = 0; f < numFeatures; f++) {
        if (maxValues[f] == minValues[f]) continue;

        float range = maxValues[f] - minValues[f];
        for (size_t g = 0; g < numGestures; g++) {
            for (size_t s = 0; s < numSamples; s++) {
                matrix[g][s][f] = (matrix[g][s][f] - minValues[f]) / range;
            }
        }
    }

    // Calculate distances to all stored gestures
    std::vector<std::pair<float, int>> distances;

    for (size_t g = 0; g < numGestures; g++) {
        // Check if gesture has valid data
        bool allZero = true;
        for (size_t s = 0; s < numSamples && allZero; s++) {
            for (size_t f = 0; f < numFeatures; f++) {
                if (matrix[g][s][f] != 0.0f) {
                    allZero = false;
                    break;
                }
            }
        }

        if (allZero) continue;

        // Calculate average distance to all samples of this gesture
        float totalDistance = 0.0f;
        int sampleCount = 0;

        for (size_t s = 0; s < numSamples; s++) {
            float distance = calculateDistance(normalizedFeatures, matrix[g][s], numFeatures);
            totalDistance += distance;
            sampleCount++;
        }

        if (sampleCount > 0) {
            float avgDistance = totalDistance / sampleCount;
            distances.push_back({avgDistance, static_cast<int>(g)});
        }
    }

    // Clean up matrix
    _storage.clearMatrix3D(matrix, numGestures, numSamples);

    if (distances.empty()) {
        Logger::getInstance().log("ADXL345: No valid gestures to compare");
        confidence = 0.0f;
        return -1;
    }

    // Sort by distance (ascending)
    std::sort(distances.begin(), distances.end());

    // Use K nearest neighbors to vote
    int kNeighbors = std::min(_kValue, static_cast<int>(distances.size()));
    std::unordered_map<int, int> votes;
    std::unordered_map<int, float> totalDistances;

    for (int i = 0; i < kNeighbors; i++) {
        int gestureID = distances[i].second;
        float distance = distances[i].first;

        votes[gestureID]++;
        totalDistances[gestureID] += distance;
    }

    // Find gesture with most votes
    int bestGestureID = -1;
    int maxVotes = 0;
    float bestAvgDistance = INFINITY;

    for (const auto& vote : votes) {
        int gestureID = vote.first;
        int voteCount = vote.second;
        float avgDistance = totalDistances[gestureID] / voteCount;

        if (voteCount > maxVotes || (voteCount == maxVotes && avgDistance < bestAvgDistance)) {
            maxVotes = voteCount;
            bestGestureID = gestureID;
            bestAvgDistance = avgDistance;
        }
    }

    // Calculate confidence (inverse of distance, normalized)
    // Distance range is typically 0-10, so we map: 0 -> 1.0, 10 -> 0.0
    confidence = std::max(0.0f, 1.0f - (bestAvgDistance / 10.0f));

    return bestGestureID;
}

float ADXL345GestureRecognizer::calculateDistance(const float* features1, const float* features2, size_t count) {
    float sumSquares = 0.0f;

    for (size_t i = 0; i < count; i++) {
        float diff = features1[i] - features2[i];
        sumSquares += diff * diff;
    }

    return sqrt(sumSquares);
}

String ADXL345GestureRecognizer::gestureIDToName(int gestureID) const {
    if (gestureID < 0) return "unknown";
    if (gestureID <= 8) return String("custom_") + String(gestureID);
    return String("gesture_") + String(gestureID);
}
