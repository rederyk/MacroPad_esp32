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

#include "gestureAnalyze.h"
#include "gestureNormalize.h"
#include "gestureFeatures.h"
#include "gestureStorage.h"
#include "gestureMotionIntegrator.h"
#include "gestureShapeAnalysis.h"
#include "gestureOrientationFeatures.h"
#include "Logger.h"
#include <cfloat>
#include <unordered_map>

GestureAnalyze::GestureAnalyze(GestureRead &gestureReader)
    : _gestureReader(gestureReader),
      _isAnalyzing(false),
      _mode(MODE_AUTO),
      _confidenceThreshold(0.5f)
{
}

SampleBuffer &GestureAnalyze::getFiltered()
{
    // Get reference to collected samples
    SampleBuffer &samples = getRawSample();

    // Apply low-pass filter with 5Hz cutoff frequency
    applyLowPassFilter(&samples, 5.0f);
    // normalizeRotation expects a SampleBuffer* pointer
    normalizeRotation(&samples);

    return samples;
}

SampleBuffer &GestureAnalyze::getRawSample()
{
    return _gestureReader.getCollectedSamples();
}

void GestureAnalyze::clearSamples()
{
    _gestureReader.clearMemory();
}

void GestureAnalyze::normalizeRotation(SampleBuffer *buffer)
{
    ::normalizeRotation(buffer);
}

void GestureAnalyze::extractFeatures(SampleBuffer *buffer, float *features)
{
    ::extractFeatures(buffer, features);
}

bool GestureAnalyze::saveFeaturesWithID(uint8_t gestureID)
{
    // Check available memory before proceeding
    size_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < 10240)
    { // 10KB minimum
        Logger::getInstance().log("[Error] Insufficient memory to save features");
        return false;
    }

    GestureStorage storage;
    float features[MAX_FEATURES];

    // Get filtered samples
    SampleBuffer &buffer = getFiltered();

    // Verify buffer is valid
    if (buffer.sampleCount == 0 || !buffer.samples)
    {
        Logger::getInstance().log("[Error] No valid samples collected");
        return false;
    }

    // Check memory before feature extraction
    if (ESP.getFreeHeap() < 8192)
    { // 8KB buffer
        Logger::getInstance().log("[Warning] Low memory before feature extraction");
    }

    // Extract features
    extractFeatures(&buffer, features);

    // Check memory before saving
    if (ESP.getFreeHeap() < 6144)
    { // 6KB buffer
        Logger::getInstance().log("[Warning] Low memory before saving features");
    }

    // Save features with ID
    bool result = storage.saveGestureFeature(gestureID, features, MAX_FEATURES);

    // Log memory status after operation

    Logger::getInstance().log("[Debug] Memory after save: " + String(ESP.getFreeHeap()) + " bytes");

    return result;
}

int GestureAnalyze::findKNNMatch(int k)
{
    // Load feature matrix from binary
    GestureStorage storage;
    size_t numGestures, numSamples, numFeatures;
    float ***matrix = storage.loadMatrixFromBinary("/gestures.bin", numGestures, numSamples, numFeatures);

    if (!matrix || numGestures == 0)
    {
        Logger::getInstance().log("[Error] Failed to load feature matrix");
        return -1;
    }

    // Get filtered samples and extract features
    SampleBuffer &buffer = getFiltered();
    float queryFeatures[MAX_FEATURES];
    extractFeatures(&buffer, queryFeatures);

    // Calculate normalization parameters from the raw matrix
    std::vector<float> minValues(numFeatures, INFINITY);
    std::vector<float> maxValues(numFeatures, -INFINITY);

    // Find min and max for each feature
    for (size_t f = 0; f < numFeatures; f++)
    {
        for (size_t g = 0; g < numGestures; g++)
        {
            for (size_t s = 0; s < numSamples; s++)
            {
                float val = matrix[g][s][f];
                if (val < minValues[f])
                    minValues[f] = val;
                if (val > maxValues[f])
                    maxValues[f] = val;
            }
        }
    }

    // Normalize the matrix in-place
    for (size_t f = 0; f < numFeatures; f++)
    {
        // Skip features with no range
        if (maxValues[f] == minValues[f])
            continue;

        float range = maxValues[f] - minValues[f];
        for (size_t g = 0; g < numGestures; g++)
        {
            for (size_t s = 0; s < numSamples; s++)
            {
                matrix[g][s][f] = (matrix[g][s][f] - minValues[f]) / range;
            }
        }
    }

    // Normalize query features using the same parameters
    for (size_t f = 0; f < numFeatures; f++)
    {
        // Skip features with no range
        if (maxValues[f] == minValues[f])
            continue;

        float range = maxValues[f] - minValues[f];
        queryFeatures[f] = (queryFeatures[f] - minValues[f]) / range;

        // Clamp to [0,1] in case the query feature is outside the training range
        if (queryFeatures[f] < 0.0f)
            queryFeatures[f] = 0.0f;
        if (queryFeatures[f] > 1.0f)
            queryFeatures[f] = 1.0f;
    }

    // Calculate distances to all stored gestures using matrix index as ID
    std::vector<std::pair<float, int>> distances;
    for (size_t i = 0; i < numGestures; i++)
    {
        // Use matrix index as gesture ID
        const int gestureID = i;

        // Check if all samples for this gesture ID are filled with 0.0
        bool allZero = true;
        for (size_t s = 0; s < numSamples; s++)
        {
            for (size_t f = 0; f < numFeatures; f++)
            {
                if (matrix[i][s][f] != 0.0f)
                {
                    allZero = false;
                    break;
                }
            }
            if (!allZero)
                break;
        }

        if (allZero)
        {
            continue;
        }

        float minDist = FLT_MAX;

        // Find minimum distance across samples
        for (size_t s = 0; s < numSamples; s++)
        {
            float dist = 0.0f;
            for (size_t f = 0; f < numFeatures; f++)
            {
                float diff = queryFeatures[f] - matrix[i][s][f];
                dist += diff * diff;
            }
            if (dist < minDist)
                minDist = dist;
        }

        distances.emplace_back(sqrt(minDist), gestureID);
    }

    // Sort distances and find k nearest neighbors
    std::sort(distances.begin(), distances.end());

    Logger::getInstance().log("Distances sorted:");
    for (const auto &entry : distances)
    {
        float dist = entry.first;
        int id = entry.second;
        Logger::getInstance().log("Distance: " + String(dist) + ", ID: " + String(id));
    }


    // Count votes among top k matches
    std::unordered_map<int, int> votes;
    for (int i = 0; i < std::min(k, static_cast<int>(distances.size())); i++)
    {
        votes[distances[i].second]++;
    }

    // Find gesture with most votes, using smallest distance for ties
    int bestMatch = -1;
    int maxVotes = 0;
    float minDistance = FLT_MAX;

    for (const auto &entry : votes)
    {
        int id = entry.first;
        int count = entry.second;

        // Find the smallest distance for this gesture
        float currDist = FLT_MAX;
        for (const auto &d : distances)
        {
            if (d.second == id && d.first < currDist)
            {
                currDist = d.first;
            }
        }

        if (count > maxVotes || (count == maxVotes && currDist < minDistance))
        {
            maxVotes = count;
            bestMatch = id;
            minDistance = currDist;
        }
    }
    Logger::getInstance().log("Best Match ID: " + String(bestMatch));

    // Cleanup matrix memory
    for (size_t i = 0; i < numGestures; i++)
    {
        for (size_t s = 0; s < numSamples; s++)
        {
            delete[] matrix[i][s];
        }
        delete[] matrix[i];
    }
    delete[] matrix;

    return bestMatch;
}

// ============ NEW DUAL-MODE GESTURE RECOGNITION IMPLEMENTATION ============

const char* GestureResult::getName() const
{
    if (mode == MODE_SHAPE_RECOGNITION && shapeType != SHAPE_UNKNOWN) {
        return getShapeName(shapeType);
    } else if (mode == MODE_ORIENTATION && orientationType != ORIENT_UNKNOWN) {
        return getOrientationName(orientationType);
    } else if (gestureID >= 0) {
        static char buffer[32];
        snprintf(buffer, sizeof(buffer), "Gesture %d", gestureID);
        return buffer;
    }
    return "Unknown";
}

GestureResult GestureAnalyze::recognizeGesture()
{
    SampleBuffer& buffer = getRawSample();

    if (buffer.sampleCount == 0 || !buffer.samples) {
        Logger::getInstance().log("[GestureAnalyze] No samples to analyze");
        return GestureResult();
    }

    // Determine best mode if AUTO
    GestureMode mode = _mode;
    if (mode == MODE_AUTO) {
        mode = selectBestMode(&buffer);
        Logger::getInstance().log("[GestureAnalyze] Auto-selected mode: " + String(mode));
    }

    return recognizeGesture(mode);
}

GestureResult GestureAnalyze::recognizeGesture(GestureMode mode)
{
    switch (mode) {
        case MODE_SHAPE_RECOGNITION:
            return recognizeShape();

        case MODE_ORIENTATION:
            return recognizeOrientation();

        case MODE_LEGACY_KNN:
        default: {
            // Use legacy KNN method
            GestureResult result;
            result.gestureID = findKNNMatch(3);
            result.mode = MODE_LEGACY_KNN;
            result.confidence = result.gestureID >= 0 ? 0.7f : 0.0f;
            return result;
        }
    }
}

GestureResult GestureAnalyze::recognizeShape()
{
    GestureResult result;
    result.mode = MODE_SHAPE_RECOGNITION;

    SampleBuffer& buffer = getRawSample();

    // Apply light filtering (higher cutoff than legacy to preserve motion dynamics)
    applyLowPassFilter(&buffer, 10.0f);

    // Integrate motion to get path
    MotionPath path;
    if (!_motionIntegrator.integrate(&buffer, path)) {
        Logger::getInstance().log("[ShapeRecognition] Motion integration failed");
        return result;
    }

    // Analyze shape
    ShapeFeatures features;
    if (!_shapeAnalyzer.analyze(path, features)) {
        Logger::getInstance().log("[ShapeRecognition] Shape analysis failed");
        return result;
    }

    // Classify shape
    ShapeType type = _shapeAnalyzer.classifyShape(features);
    float confidence = _shapeAnalyzer.getConfidence();

    Logger::getInstance().log("[ShapeRecognition] Detected: " + String(getShapeName(type)) +
                             " (confidence: " + String(confidence, 2) + ")");

    // Check confidence threshold
    if (confidence < _confidenceThreshold) {
        Logger::getInstance().log("[ShapeRecognition] Confidence too low, rejecting");
        return result;
    }

    result.shapeType = type;
    result.confidence = confidence;
    result.gestureID = static_cast<int>(type); // Map shape type to ID

    return result;
}

GestureResult GestureAnalyze::recognizeOrientation()
{
    GestureResult result;
    result.mode = MODE_ORIENTATION;

    if (!hasGyroscope()) {
        Logger::getInstance().log("[OrientationRecognition] No gyroscope available");
        return result;
    }

    SampleBuffer& buffer = getRawSample();

    // Extract orientation features
    OrientationFeatures features;
    if (!_orientationExtractor.extract(&buffer, features)) {
        Logger::getInstance().log("[OrientationRecognition] Feature extraction failed");
        return result;
    }

    // Classify orientation gesture
    OrientationType type = _orientationExtractor.classify(features);
    float confidence = _orientationExtractor.getConfidence();

    Logger::getInstance().log("[OrientationRecognition] Detected: " + String(getOrientationName(type)) +
                             " (confidence: " + String(confidence, 2) + ")");

    // Check confidence threshold
    if (confidence < _confidenceThreshold) {
        Logger::getInstance().log("[OrientationRecognition] Confidence too low, rejecting");
        return result;
    }

    result.orientationType = type;
    result.confidence = confidence;
    result.gestureID = 100 + static_cast<int>(type); // Map orientation to ID (offset by 100)

    return result;
}

void GestureAnalyze::setRecognitionMode(GestureMode mode)
{
    _mode = mode;
    Logger::getInstance().log("[GestureAnalyze] Recognition mode set to: " + String(mode));
}

void GestureAnalyze::setConfidenceThreshold(float threshold)
{
    _confidenceThreshold = threshold;
}

bool GestureAnalyze::hasGyroscope() const
{
    // Access gesture reader directly since getRawSample() is not const
    SampleBuffer& buffer = _gestureReader.getCollectedSamples();

    if (buffer.sampleCount == 0 || !buffer.samples) {
        return false;
    }

    // Check if any sample has valid gyro data
    for (uint16_t i = 0; i < buffer.sampleCount; i++) {
        if (buffer.samples[i].gyroValid) {
            return true;
        }
    }

    return false;
}

GestureMode GestureAnalyze::selectBestMode(SampleBuffer* buffer)
{
    if (!buffer || buffer->sampleCount == 0) {
        return MODE_LEGACY_KNN;
    }

    bool hasGyro = false;
    for (uint16_t i = 0; i < buffer->sampleCount; i++) {
        if (buffer->samples[i].gyroValid) {
            hasGyro = true;
            break;
        }
    }

    // If no gyro, can only do shape recognition or legacy KNN
    if (!hasGyro) {
        Logger::getInstance().log("[AutoSelect] No gyro, using shape recognition");
        return MODE_SHAPE_RECOGNITION;
    }

    // Check if gesture has significant motion (for shape) or orientation change
    bool hasMotion = hasSignificantMotion(buffer);
    bool hasOrient = hasOrientationChange(buffer);

    Logger::getInstance().log("[AutoSelect] Motion=" + String(hasMotion) +
                             ", Orient=" + String(hasOrient));

    // If both, prefer orientation (usually more intentional)
    if (hasOrient) {
        return MODE_ORIENTATION;
    } else if (hasMotion) {
        return MODE_SHAPE_RECOGNITION;
    }

    // Fallback to legacy
    return MODE_LEGACY_KNN;
}

bool GestureAnalyze::hasSignificantMotion(SampleBuffer* buffer) const
{
    if (!buffer || buffer->sampleCount < 10) {
        return false;
    }

    // Calculate total acceleration magnitude variation
    float sumMag = 0;
    float sumMagSq = 0;

    for (uint16_t i = 0; i < buffer->sampleCount; i++) {
        float mag = sqrtf(buffer->samples[i].x * buffer->samples[i].x +
                         buffer->samples[i].y * buffer->samples[i].y +
                         buffer->samples[i].z * buffer->samples[i].z);
        sumMag += mag;
        sumMagSq += mag * mag;
    }

    float avgMag = sumMag / buffer->sampleCount;
    float variance = (sumMagSq / buffer->sampleCount) - (avgMag * avgMag);
    float stdDev = sqrtf(fmaxf(variance, 0.0f));

    // Significant motion if std deviation > 0.3g
    return stdDev > 0.3f;
}

bool GestureAnalyze::hasOrientationChange(SampleBuffer* buffer) const
{
    if (!buffer || buffer->sampleCount < 10) {
        return false;
    }

    // Check if gyroscope shows significant rotation
    float maxGyroMag = 0;
    float sumGyroMag = 0;

    for (uint16_t i = 0; i < buffer->sampleCount; i++) {
        if (buffer->samples[i].gyroValid) {
            float gyroMag = sqrtf(buffer->samples[i].gyroX * buffer->samples[i].gyroX +
                                 buffer->samples[i].gyroY * buffer->samples[i].gyroY +
                                 buffer->samples[i].gyroZ * buffer->samples[i].gyroZ);
            sumGyroMag += gyroMag;
            if (gyroMag > maxGyroMag) {
                maxGyroMag = gyroMag;
            }
        }
    }

    float avgGyroMag = sumGyroMag / buffer->sampleCount;

    // Significant orientation change if:
    // - Peak gyro > 1.0 rad/s (57 deg/s)
    // - OR average gyro > 0.5 rad/s (28 deg/s)
    return (maxGyroMag > 1.0f) || (avgGyroMag > 0.5f);
}