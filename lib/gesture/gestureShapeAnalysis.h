/*
 * ESP32 MacroPad Project
 * Copyright (C) [2025] [Enrico Mori]
 *
 * Shape Analysis for Gesture Recognition
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

#ifndef GESTURE_SHAPE_ANALYSIS_H
#define GESTURE_SHAPE_ANALYSIS_H

#include "gestureMotionIntegrator.h"

/**
 * Geometric shape descriptors
 * These features characterize the shape of the gesture path
 */
struct ShapeFeatures {
    // Bounding box
    float width;          // Width of bounding box
    float height;         // Height of bounding box
    float depth;          // Depth (Z-axis extent)
    float aspectRatio;    // width / height (>3 = line, ~1 = square/circle)

    // Circularity/Compactness
    float circularity;    // 4π * area / perimeter² (1.0 = perfect circle)
    float compactness;    // area / perimeter²

    // Distance from centroid statistics
    Vector3 centroid;     // Center of mass of path
    float avgRadius;      // Average distance from centroid
    float stdRadius;      // Std deviation of radius (low = circle)
    float radiusVariation; // stdRadius / avgRadius

    // Angular features
    float avgTurningAngle;     // Average angle change between segments
    float totalTurningAngle;   // Total rotation (2π = circle, 3π = triangle, etc)
    uint8_t sharpTurns;        // Number of sharp turns (>60° change)

    // Path characteristics
    float pathLength;          // Total path length
    float straightness;        // Displacement / pathLength (1.0 = straight line)
    bool isClosed;             // Is path closed

    // Velocity features
    float avgVelocity;         // Average velocity magnitude
    float peakVelocity;        // Maximum velocity
    float velocityVariation;   // Std of velocity / mean

    ShapeFeatures() {
        width = height = depth = 0;
        aspectRatio = 1.0f;
        circularity = compactness = 0;
        centroid = Vector3(0, 0, 0);
        avgRadius = stdRadius = radiusVariation = 0;
        avgTurningAngle = totalTurningAngle = 0;
        sharpTurns = 0;
        pathLength = straightness = 0;
        isClosed = false;
        avgVelocity = peakVelocity = velocityVariation = 0;
    }
};

/**
 * Recognized shape types
 */
enum ShapeType {
    SHAPE_UNKNOWN = 0,
    SHAPE_CIRCLE,       // ⭕ Circular motion
    SHAPE_LINE,         // ➖ Straight line
    SHAPE_TRIANGLE,     // 🔺 Triangle (3 sharp turns)
    SHAPE_SQUARE,       // ⬜ Square/Rectangle (4 sharp turns)
    SHAPE_ZIGZAG,       // ⚡ Zigzag pattern (multiple turns)
    SHAPE_INFINITY,     // ∞ Infinity/figure-8
    SHAPE_SPIRAL,       // 🌀 Spiral pattern
    SHAPE_ARC           // ⌒ Arc/semi-circle
};

/**
 * Shape Analysis Class
 *
 * Analyzes motion paths to extract geometric features and classify shapes
 */
class ShapeAnalysis {
public:
    ShapeAnalysis();

    /**
     * Analyze motion path and extract shape features
     *
     * @param path Motion path to analyze
     * @param features Output shape features
     * @return true if analysis successful
     */
    bool analyze(const MotionPath& path, ShapeFeatures& features);

    /**
     * Classify shape based on features
     *
     * @param features Shape features from analyze()
     * @return Recognized shape type
     */
    ShapeType classifyShape(const ShapeFeatures& features);

    /**
     * Get confidence score for classification (0-1)
     */
    float getConfidence() const { return _confidence; }

    /**
     * Set thresholds for shape classification
     */
    void setCircularityThreshold(float threshold);
    void setSharpTurnAngle(float angleDegrees);

private:
    float _circularityThreshold;  // Min circularity for circle (default 0.7)
    float _sharpTurnAngle;        // Min angle for sharp turn in degrees (default 60)
    float _confidence;            // Last classification confidence

    // Feature extraction helpers
    void calculateBoundingBox(const MotionPath& path, ShapeFeatures& features);
    void calculateCentroid(const MotionPath& path, ShapeFeatures& features);
    void calculateRadiusStats(const MotionPath& path, ShapeFeatures& features);
    void calculateAngularFeatures(const MotionPath& path, ShapeFeatures& features);
    void calculateVelocityFeatures(const MotionPath& path, ShapeFeatures& features);
    void calculateCircularity(const MotionPath& path, ShapeFeatures& features);

    // Shape classification helpers
    bool isCircle(const ShapeFeatures& f) const;
    bool isLine(const ShapeFeatures& f) const;
    bool isTriangle(const ShapeFeatures& f) const;
    bool isSquare(const ShapeFeatures& f) const;
    bool isZigzag(const ShapeFeatures& f) const;
    bool isInfinity(const ShapeFeatures& f) const;

    // Utility
    float calculateAngleBetweenVectors(const Vector3& v1, const Vector3& v2) const;
};

/**
 * Get human-readable shape name
 */
const char* getShapeName(ShapeType type);

#endif // GESTURE_SHAPE_ANALYSIS_H
