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


#ifndef GESTUREANALYZE_H
#define GESTUREANALYZE_H

#include "gestureRead.h"
#include "gestureNormalize.h"
#include "gestureFilter.h"
#include "gestureFeatures.h"
#include "gestureMotionIntegrator.h"
#include "gestureShapeAnalysis.h"
#include "gestureOrientationFeatures.h"
#include <vector>

struct GestureAnalysis
{
    SampleBuffer samples;
};

/**
 * Gesture recognition mode
 */
enum GestureMode {
    MODE_LEGACY_KNN = 0,        // Original KNN with accel features
    MODE_SHAPE_RECOGNITION,     // Motion path shape recognition
    MODE_ORIENTATION,           // Orientation-based gestures
    MODE_AUTO                   // Auto-select based on sensor capabilities
};

/**
 * Unified gesture result
 */
struct GestureResult {
    int gestureID;              // Gesture ID (-1 if unknown)
    GestureMode mode;           // Which mode was used
    float confidence;           // Confidence score 0-1

    // Mode-specific results
    ShapeType shapeType;        // For shape recognition mode
    OrientationType orientationType;  // For orientation mode

    // Human-readable name
    const char* getName() const;

    GestureResult() : gestureID(-1), mode(MODE_AUTO), confidence(0.0f),
                     shapeType(SHAPE_UNKNOWN), orientationType(ORIENT_UNKNOWN) {}
};

class GestureAnalyze
{
public:
    GestureAnalyze(GestureRead &gestureReader);

    // Memory management
    void clearSamples();

    // Process and return analysis data
    SampleBuffer& getFiltered();

    SampleBuffer& getRawSample();

    // Normalize rotation of gesture samples
    void normalizeRotation(SampleBuffer *buffer);

    void extractFeatures(SampleBuffer *buffer, float *features);

    // Save extracted features with gesture ID
    bool saveFeaturesWithID(uint8_t gestureID);

    // ============ LEGACY METHODS (backward compatibility) ============
    int findKNNMatch(int k);

    // ============ NEW DUAL-MODE GESTURE RECOGNITION ============

    /**
     * Recognize gesture using best available method
     * Automatically selects mode based on sensor capabilities and gesture type
     *
     * @return GestureResult with ID, confidence, and type information
     */
    GestureResult recognizeGesture();

    /**
     * Recognize gesture using specific mode
     *
     * @param mode Recognition mode to use
     * @return GestureResult
     */
    GestureResult recognizeGesture(GestureMode mode);

    /**
     * Recognize shape gesture (circle, line, triangle, etc)
     * Requires at least 1-2 seconds of motion data
     *
     * @return GestureResult with shapeType filled
     */
    GestureResult recognizeShape();

    /**
     * Recognize orientation gesture (rotate, tilt, shake, etc)
     * Requires gyroscope data (MPU6050)
     *
     * @return GestureResult with orientationType filled
     */
    GestureResult recognizeOrientation();

    /**
     * Set recognition mode
     * MODE_AUTO automatically selects best mode based on available sensors
     */
    void setRecognitionMode(GestureMode mode);

    /**
     * Get current recognition mode
     */
    GestureMode getRecognitionMode() const { return _mode; }

    /**
     * Check if gyroscope is available
     */
    bool hasGyroscope() const;

    /**
     * Set minimum confidence threshold for gesture acceptance
     * Gestures with confidence below this threshold will be rejected
     * Default: 0.5 (50%)
     */
    void setConfidenceThreshold(float threshold);

private:
    GestureRead &_gestureReader;
    bool _isAnalyzing;
    SampleBuffer _samples;

    // Recognition components
    GestureMode _mode;
    MotionIntegrator _motionIntegrator;
    ShapeAnalysis _shapeAnalyzer;
    OrientationFeatureExtractor _orientationExtractor;

    float _confidenceThreshold;

    // Helper: Determine best recognition mode based on sensor and data
    GestureMode selectBestMode(SampleBuffer* buffer);

    // Helper: Check if gesture has significant motion
    bool hasSignificantMotion(SampleBuffer* buffer) const;

    // Helper: Check if gesture has significant orientation change
    bool hasOrientationChange(SampleBuffer* buffer) const;
};

extern GestureAnalyze gestureAnalyzer;  // Dichiarazione extern per l'istanza globale

#endif
