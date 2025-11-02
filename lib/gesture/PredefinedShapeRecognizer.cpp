/*
 * ESP32 MacroPad Project
 * Copyright (C)
 *
 * Shared predefined shape recognition helper.
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

#include "PredefinedShapeRecognizer.h"
#include "Logger.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <new>

PredefinedShapeRecognizer::PredefinedShapeRecognizer()
    : _motionIntegrator(),
      _shapeAnalyzer(),
      _minMotionStdDev(0.25f) // Default threshold ~0.25 g
{
}

GestureRecognitionResult PredefinedShapeRecognizer::recognize(SampleBuffer *buffer, SensorGestureMode sensorMode)
{
    GestureRecognitionResult result;
    result.sensorMode = sensorMode;

    if (!buffer || buffer->sampleCount == 0 || !buffer->samples)
    {
        Logger::getInstance().log("[ShapeRecognizer] Invalid sample buffer");
        return result;
    }

    if (!hasSignificantMotion(buffer))
    {
        Logger::getInstance().log("[ShapeRecognizer] Motion below threshold");
        return result;
    }

    const uint16_t sampleCount = buffer->sampleCount;
    const uint16_t workingCount = std::min<uint16_t>(sampleCount, MotionPath::MAX_PATH_POINTS);
    std::unique_ptr<Sample[]> workingSamples(new (std::nothrow) Sample[workingCount]);
    if (!workingSamples)
    {
        Logger::getInstance().log("[ShapeRecognizer] Failed to allocate working buffer (OOM)");
        return result;
    }

    // Work on a copy so original samples remain untouched for other recognizers
    SampleBuffer working;
    working.sampleCount = workingCount;
    working.sampleHZ = buffer->sampleHZ;
    working.maxSamples = workingCount;
    working.samples = workingSamples.get();

    const uint16_t startIndex = sampleCount > workingCount ? sampleCount - workingCount : 0;
    memcpy(working.samples, buffer->samples + startIndex, workingCount * sizeof(Sample));

    // Filter high frequency noise then normalise rotation
    applyLowPassFilter(&working, 5.0f);
    normalizeRotation(&working);

    std::unique_ptr<MotionPath> path(new (std::nothrow) MotionPath());
    if (!path)
    {
        Logger::getInstance().log("[ShapeRecognizer] Failed to allocate motion path (OOM)");
        return result;
    }

    if (!_motionIntegrator.integrate(&working, *path) || !path->isValid)
    {
        Logger::getInstance().log("[ShapeRecognizer] Motion integration failed");
        return result;
    }

    ShapeFeatures features;
    if (!_shapeAnalyzer.analyze(*path, features))
    {
        Logger::getInstance().log("[ShapeRecognizer] Shape feature extraction failed");
        return result;
    }

    ShapeType shapeType = _shapeAnalyzer.classifyShape(features);
    float confidence = _shapeAnalyzer.getConfidence();

    if (shapeType == SHAPE_UNKNOWN || confidence <= 0.0f)
    {
        Logger::getInstance().log("[ShapeRecognizer] Shape classification failed");
        return result;
    }

    result.gestureID = shapeTypeToID(shapeType);
    result.gestureName = shapeTypeToName(shapeType);
    result.confidence = confidence;

    Logger::getInstance().log("[ShapeRecognizer] Detected " + result.gestureName +
                              " (ID " + String(result.gestureID) +
                              ", conf " + String(confidence, 2) + ")");
    return result;
}

bool PredefinedShapeRecognizer::hasSignificantMotion(SampleBuffer *buffer) const
{
    if (!buffer || buffer->sampleCount < 5)
    {
        return false;
    }

    // Optimized: calculate variance using magnitude squared (avoids extra sqrt calls)
    float sumMagSq = 0.0f;
    float sumMagSqSq = 0.0f;

    for (uint16_t i = 0; i < buffer->sampleCount; i++)
    {
        const Sample &s = buffer->samples[i];
        float magSq = s.x * s.x + s.y * s.y + s.z * s.z;
        sumMagSq += magSq;
        sumMagSqSq += magSq * magSq;
    }

    float meanSq = sumMagSq / buffer->sampleCount;
    float variance = (sumMagSqSq / buffer->sampleCount) - (meanSq * meanSq);
    float stdDev = sqrtf(fmaxf(variance, 0.0f));

    // Adjust threshold to work with squared magnitudes
    float thresholdSq = _minMotionStdDev * _minMotionStdDev * buffer->sampleCount * 0.5f;
    return stdDev > thresholdSq;
}

int PredefinedShapeRecognizer::shapeTypeToID(ShapeType shape) const
{
    return 100 + static_cast<int>(shape);
}

String PredefinedShapeRecognizer::shapeTypeToName(ShapeType shape) const
{
    switch (shape)
    {
    case SHAPE_LINE:
        return "G_LINE";
    case SHAPE_CIRCLE:
        return "G_CIRCLE";
    case SHAPE_TRIANGLE:
        return "G_TRIANGLE";
    case SHAPE_SQUARE:
        return "G_SQUARE";
    case SHAPE_ZIGZAG:
        return "G_ZIGZAG";
    case SHAPE_INFINITY:
        return "G_INFINITY";
    case SHAPE_SPIRAL:
        return "G_SPIRAL";
    case SHAPE_ARC:
        return "G_ARC";
    default:
        return "G_UNKNOWN_SHAPE";
    }
}
