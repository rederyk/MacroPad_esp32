/*
 * ESP32 MacroPad Project
 * Copyright (C) [2025] [Enrico Mori]
 *
 * Shape Analysis Implementation
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

#include "gestureShapeAnalysis.h"
#include "Logger.h"
#include <cmath>
#include <cfloat>

ShapeAnalysis::ShapeAnalysis()
    : _circularityThreshold(0.7f),
      _sharpTurnAngle(60.0f),
      _confidence(0.0f)
{
}

void ShapeAnalysis::setCircularityThreshold(float threshold)
{
    _circularityThreshold = threshold;
}

void ShapeAnalysis::setSharpTurnAngle(float angleDegrees)
{
    _sharpTurnAngle = angleDegrees;
}

bool ShapeAnalysis::analyze(const MotionPath& path, ShapeFeatures& features)
{
    if (path.count < 3 || !path.isValid) {
        Logger::getInstance().log("[ShapeAnalysis] Invalid path for analysis");
        return false;
    }

    features = ShapeFeatures();

    // Calculate all features
    calculateBoundingBox(path, features);
    calculateCentroid(path, features);
    calculateRadiusStats(path, features);
    calculateAngularFeatures(path, features);
    calculateVelocityFeatures(path, features);
    calculateCircularity(path, features);

    // Copy path characteristics
    features.pathLength = path.totalLength;
    features.isClosed = path.isClosed;

    // Calculate straightness (direct distance / path length)
    if (features.pathLength > 0) {
        float displacement = Vector3::distance(path.positions[0], path.positions[path.count - 1]);
        features.straightness = displacement / features.pathLength;
    }

    Logger::getInstance().log("[ShapeAnalysis] Features: circ=" + String(features.circularity, 3) +
                             ", aspect=" + String(features.aspectRatio, 2) +
                             ", turns=" + String(features.sharpTurns) +
                             ", radVar=" + String(features.radiusVariation, 3));

    return true;
}

ShapeType ShapeAnalysis::classifyShape(const ShapeFeatures& features)
{
    _confidence = 0.0f;

    // Try each shape in order of specificity
    if (isCircle(features)) {
        _confidence = features.circularity; // Higher circularity = more confident
        return SHAPE_CIRCLE;
    }

    if (isInfinity(features)) {
        _confidence = 0.8f; // Infinity is fairly distinctive
        return SHAPE_INFINITY;
    }

    if (isSquare(features)) {
        _confidence = 0.75f; // 4 corners detected
        return SHAPE_SQUARE;
    }

    if (isTriangle(features)) {
        _confidence = 0.75f; // 3 corners detected
        return SHAPE_TRIANGLE;
    }

    if (isLine(features)) {
        _confidence = features.straightness; // Straighter = more confident
        return SHAPE_LINE;
    }

    if (isZigzag(features)) {
        _confidence = 0.6f; // Zigzag is less distinctive
        return SHAPE_ZIGZAG;
    }

    // Unknown shape
    _confidence = 0.0f;
    return SHAPE_UNKNOWN;
}

void ShapeAnalysis::calculateBoundingBox(const MotionPath& path, ShapeFeatures& features)
{
    Vector3 minBound(FLT_MAX, FLT_MAX, FLT_MAX);
    Vector3 maxBound(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (uint16_t i = 0; i < path.count; i++) {
        const Vector3& p = path.positions[i];

        if (p.x < minBound.x) minBound.x = p.x;
        if (p.y < minBound.y) minBound.y = p.y;
        if (p.z < minBound.z) minBound.z = p.z;

        if (p.x > maxBound.x) maxBound.x = p.x;
        if (p.y > maxBound.y) maxBound.y = p.y;
        if (p.z > maxBound.z) maxBound.z = p.z;
    }

    features.width = maxBound.x - minBound.x;
    features.height = maxBound.y - minBound.y;
    features.depth = maxBound.z - minBound.z;

    // Calculate aspect ratio (avoid division by zero)
    if (features.height > 0.01f) {
        features.aspectRatio = features.width / features.height;
    } else {
        features.aspectRatio = 10.0f; // Very elongated
    }
}

void ShapeAnalysis::calculateCentroid(const MotionPath& path, ShapeFeatures& features)
{
    Vector3 sum(0, 0, 0);

    for (uint16_t i = 0; i < path.count; i++) {
        sum = sum + path.positions[i];
    }

    features.centroid = sum / static_cast<float>(path.count);
}

void ShapeAnalysis::calculateRadiusStats(const MotionPath& path, ShapeFeatures& features)
{
    if (path.count == 0) return;

    float sumRadius = 0;
    float sumRadiusSq = 0;

    for (uint16_t i = 0; i < path.count; i++) {
        float radius = Vector3::distance(path.positions[i], features.centroid);
        sumRadius += radius;
        sumRadiusSq += radius * radius;
    }

    features.avgRadius = sumRadius / path.count;

    // Calculate standard deviation
    float variance = (sumRadiusSq / path.count) - (features.avgRadius * features.avgRadius);
    features.stdRadius = sqrtf(fmaxf(variance, 0.0f));

    // Radius variation (normalized)
    if (features.avgRadius > 0.01f) {
        features.radiusVariation = features.stdRadius / features.avgRadius;
    } else {
        features.radiusVariation = 0;
    }
}

void ShapeAnalysis::calculateAngularFeatures(const MotionPath& path, ShapeFeatures& features)
{
    if (path.count < 3) return;

    float sumAngles = 0;
    float totalRotation = 0;
    uint8_t sharpTurns = 0;
    const float sharpTurnRad = _sharpTurnAngle * M_PI / 180.0f;

    for (uint16_t i = 1; i < path.count - 1; i++) {
        Vector3 v1 = path.positions[i] - path.positions[i - 1];
        Vector3 v2 = path.positions[i + 1] - path.positions[i];

        // Skip if either vector is too short (noise)
        if (v1.magnitude() < 0.01f || v2.magnitude() < 0.01f) {
            continue;
        }

        float angle = calculateAngleBetweenVectors(v1, v2);
        sumAngles += angle;
        totalRotation += fabsf(angle);

        // Count sharp turns
        if (fabsf(angle) > sharpTurnRad) {
            sharpTurns++;
        }
    }

    uint16_t segments = path.count - 2;
    features.avgTurningAngle = segments > 0 ? sumAngles / segments : 0;
    features.totalTurningAngle = totalRotation;
    features.sharpTurns = sharpTurns;
}

void ShapeAnalysis::calculateVelocityFeatures(const MotionPath& path, ShapeFeatures& features)
{
    if (path.count < 2) return;

    float sumVel = 0;
    float sumVelSq = 0;
    float maxVel = 0;

    for (uint16_t i = 0; i < path.count; i++) {
        float vel = path.velocities[i].magnitude();
        sumVel += vel;
        sumVelSq += vel * vel;

        if (vel > maxVel) {
            maxVel = vel;
        }
    }

    features.avgVelocity = sumVel / path.count;
    features.peakVelocity = maxVel;

    // Velocity variation
    float velVariance = (sumVelSq / path.count) - (features.avgVelocity * features.avgVelocity);
    float velStdDev = sqrtf(fmaxf(velVariance, 0.0f));

    if (features.avgVelocity > 0.01f) {
        features.velocityVariation = velStdDev / features.avgVelocity;
    }
}

void ShapeAnalysis::calculateCircularity(const MotionPath& path, ShapeFeatures& features)
{
    // Estimate area using shoelace formula (2D projection)
    float area = 0;
    for (uint16_t i = 0; i < path.count - 1; i++) {
        area += (path.positions[i].x * path.positions[i + 1].y -
                 path.positions[i + 1].x * path.positions[i].y);
    }
    area = fabsf(area) / 2.0f;

    // Circularity = 4π * Area / Perimeter²
    // Perfect circle = 1.0, line approaches 0
    if (features.pathLength > 0.01f) {
        features.circularity = (4.0f * M_PI * area) / (features.pathLength * features.pathLength);
        features.compactness = area / (features.pathLength * features.pathLength);
    } else {
        features.circularity = 0;
        features.compactness = 0;
    }

    // Clamp to [0, 1] (can exceed due to noise)
    if (features.circularity > 1.0f) features.circularity = 1.0f;
    if (features.compactness > 1.0f) features.compactness = 1.0f;
}

bool ShapeAnalysis::isCircle(const ShapeFeatures& f) const
{
    // Circle criteria:
    // - High circularity (>0.7)
    // - Low radius variation (<0.3)
    // - Aspect ratio close to 1 (0.5 to 2.0)
    // - Closed path
    return (f.circularity > _circularityThreshold &&
            f.radiusVariation < 0.35f &&
            f.aspectRatio > 0.5f && f.aspectRatio < 2.0f &&
            f.isClosed);
}

bool ShapeAnalysis::isLine(const ShapeFeatures& f) const
{
    // Line criteria:
    // - High straightness (>0.8)
    // - High aspect ratio (>3.0)
    // - Few or no sharp turns (0-1)
    // - NOT closed
    return (f.straightness > 0.75f &&
            f.aspectRatio > 2.5f &&
            f.sharpTurns <= 1 &&
            !f.isClosed);
}

bool ShapeAnalysis::isTriangle(const ShapeFeatures& f) const
{
    // Triangle criteria:
    // - Exactly 3 sharp turns
    // - Closed path
    // - Low circularity (not a circle)
    return (f.sharpTurns == 3 &&
            f.isClosed &&
            f.circularity < 0.6f);
}

bool ShapeAnalysis::isSquare(const ShapeFeatures& f) const
{
    // Square/Rectangle criteria:
    // - 4 sharp turns
    // - Closed path
    // - Aspect ratio reasonable for rectangle (0.5 to 2.0)
    return (f.sharpTurns == 4 &&
            f.isClosed &&
            f.aspectRatio > 0.4f && f.aspectRatio < 2.5f);
}

bool ShapeAnalysis::isZigzag(const ShapeFeatures& f) const
{
    // Zigzag criteria:
    // - Many sharp turns (>4)
    // - High aspect ratio (elongated)
    // - NOT closed
    return (f.sharpTurns > 4 &&
            f.aspectRatio > 2.0f &&
            !f.isClosed);
}

bool ShapeAnalysis::isInfinity(const ShapeFeatures& f) const
{
    // Infinity (figure-8) criteria:
    // - Two distinct lobes: 2 sharp turns
    // - High aspect ratio (wide shape)
    // - Medium circularity (two circles)
    // - Closed or nearly closed
    return (f.sharpTurns == 2 &&
            f.aspectRatio > 1.5f &&
            f.circularity > 0.3f && f.circularity < 0.7f);
}

float ShapeAnalysis::calculateAngleBetweenVectors(const Vector3& v1, const Vector3& v2) const
{
    float dot = Vector3::dot(v1, v2);
    float mag1 = v1.magnitude();
    float mag2 = v2.magnitude();

    if (mag1 < 0.0001f || mag2 < 0.0001f) {
        return 0;
    }

    // Clamp to avoid acos domain error
    float cosAngle = dot / (mag1 * mag2);
    if (cosAngle > 1.0f) cosAngle = 1.0f;
    if (cosAngle < -1.0f) cosAngle = -1.0f;

    return acosf(cosAngle);
}

const char* getShapeName(ShapeType type)
{
    switch (type) {
        case SHAPE_CIRCLE:   return "Circle";
        case SHAPE_LINE:     return "Line";
        case SHAPE_TRIANGLE: return "Triangle";
        case SHAPE_SQUARE:   return "Square";
        case SHAPE_ZIGZAG:   return "Zigzag";
        case SHAPE_INFINITY: return "Infinity";
        case SHAPE_SPIRAL:   return "Spiral";
        case SHAPE_ARC:      return "Arc";
        default:             return "Unknown";
    }
}
