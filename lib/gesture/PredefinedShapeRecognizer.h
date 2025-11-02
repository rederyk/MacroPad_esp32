/*
 * ESP32 MacroPad Project
 * Copyright (C) [2025]
 *
 * Predefined shape gesture recognition helper shared by sensor-specific recognizers.
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

#ifndef PREDEFINED_SHAPE_RECOGNIZER_H
#define PREDEFINED_SHAPE_RECOGNIZER_H

#include "IGestureRecognizer.h"
#include "gestureMotionIntegrator.h"
#include "gestureShapeAnalysis.h"
#include "gestureNormalize.h"
#include "gestureFilter.h"

/**
 * Helper that encapsulates shape-based gesture recognition shared by
 * both MPU6050 and ADXL345 recognizers.
 */
class PredefinedShapeRecognizer
{
public:
    PredefinedShapeRecognizer();

    /**
     * Minimum standard deviation (in g) required across gesture samples
     * to consider the motion significant enough for classification.
     */
    void setMinMotionStdDev(float stdDev) { _minMotionStdDev = stdDev; }

    /**
     * Enable/disable Madgwick orientation correction inside the motion integrator.
     * Should be disabled for sensors that do not expose gyroscope data.
     */
    void setUseMadgwick(bool enable) { _motionIntegrator.setUseMadgwick(enable); }

    /**
     * Execute shape classification on the provided buffer.
     *
     * @param buffer Sample buffer captured for the gesture.
     * @param sensorMode Mode to encode in the recognition result.
     * @return Filled GestureRecognitionResult (gestureID < 0 if nothing recognised).
     */
    GestureRecognitionResult recognize(SampleBuffer *buffer, SensorGestureMode sensorMode);

private:
    MotionIntegrator _motionIntegrator;
    ShapeAnalysis _shapeAnalyzer;
    float _minMotionStdDev;

    bool hasSignificantMotion(SampleBuffer *buffer) const;
    int shapeTypeToID(ShapeType shape) const;
    String shapeTypeToName(ShapeType shape) const;
};

#endif // PREDEFINED_SHAPE_RECOGNIZER_H
