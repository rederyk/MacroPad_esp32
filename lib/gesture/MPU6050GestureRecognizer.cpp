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

#include "MPU6050GestureRecognizer.h"
#include "Logger.h"

MPU6050GestureRecognizer::MPU6050GestureRecognizer()
    : _confidenceThreshold(0.5f) {
}

MPU6050GestureRecognizer::~MPU6050GestureRecognizer() {
}

bool MPU6050GestureRecognizer::init(const String& sensorType) {
    if (sensorType != "mpu6050") {
        Logger::getInstance().log("MPU6050GestureRecognizer: Wrong sensor type: " + sensorType);
        return false;
    }

    Logger::getInstance().log("MPU6050GestureRecognizer: Initialized for shape+orientation recognition (predefined gestures only)");
    return true;
}

GestureRecognitionResult MPU6050GestureRecognizer::recognize(SampleBuffer* buffer) {
    if (!buffer || buffer->sampleCount == 0) {
        Logger::getInstance().log("MPU6050GestureRecognizer: Empty buffer");
        return GestureRecognitionResult();
    }

    // Try both recognition modes
    GestureRecognitionResult shapeResult = recognizeShape(buffer);
    GestureRecognitionResult orientationResult = recognizeOrientation(buffer);

    // Select best result
    GestureRecognitionResult result = selectBestResult(shapeResult, orientationResult);

    return result;
}

GestureRecognitionResult MPU6050GestureRecognizer::recognizeShape(SampleBuffer* buffer) {
    GestureRecognitionResult result;
    result.sensorMode = SENSOR_MODE_MPU6050;

    if (!hasSignificantMotion(buffer)) {
        return result;
    }

    // Apply low-pass filter directly to buffer
    applyLowPassFilter(buffer, 5.0f);

    // Integrate motion to get path
    MotionPath path;
    if (!_motionIntegrator.integrate(buffer, path)) {
        Logger::getInstance().log("MPU6050: Motion integration failed");
        return result;
    }

    // Analyze shape
    ShapeFeatures features;
    if (!_shapeAnalyzer.analyze(path, features)) {
        return result;
    }

    // Classify shape
    ShapeType shapeType = _shapeAnalyzer.classifyShape(features);
    result.confidence = _shapeAnalyzer.getConfidence();
    result.gestureID = shapeTypeToID(shapeType);
    result.gestureName = shapeTypeToName(shapeType);

    // Store shape type in mode-specific data
    ShapeType* storedShape = new ShapeType(shapeType);
    result.modeSpecificData = storedShape;

    Logger::getInstance().log(String("MPU6050 Shape: ") + result.gestureName +
                            " (conf: " + String(result.confidence, 2) + ")");

    return result;
}

GestureRecognitionResult MPU6050GestureRecognizer::recognizeOrientation(SampleBuffer* buffer) {
    GestureRecognitionResult result;
    result.sensorMode = SENSOR_MODE_MPU6050;

    if (!hasOrientationChange(buffer)) {
        return result;
    }

    // Extract orientation features
    OrientationFeatures features;
    if (!_orientationExtractor.extract(buffer, features)) {
        Logger::getInstance().log("MPU6050: Orientation feature extraction failed");
        return result;
    }

    // Classify orientation
    OrientationType orientationType = _orientationExtractor.classify(features);
    result.confidence = _orientationExtractor.getConfidence();
    result.gestureID = orientationTypeToID(orientationType);
    result.gestureName = orientationTypeToName(orientationType);

    // Store orientation type in mode-specific data
    OrientationType* storedOrientation = new OrientationType(orientationType);
    result.modeSpecificData = storedOrientation;

    Logger::getInstance().log(String("MPU6050 Orientation: ") + result.gestureName +
                            " (conf: " + String(result.confidence, 2) + ")");

    return result;
}

GestureRecognitionResult MPU6050GestureRecognizer::selectBestResult(
    const GestureRecognitionResult& shape,
    const GestureRecognitionResult& orientation) {

    // Return result with higher confidence
    if (shape.confidence >= orientation.confidence && shape.gestureID >= 0) {
        return shape;
    } else if (orientation.gestureID >= 0) {
        return orientation;
    }

    // Both failed
    return GestureRecognitionResult();
}

bool MPU6050GestureRecognizer::trainCustomGesture(SampleBuffer* buffer, uint8_t gestureID) {
    Logger::getInstance().log("MPU6050: Custom training not supported - use predefined gestures only");
    return false;
}

bool MPU6050GestureRecognizer::hasSignificantMotion(SampleBuffer* buffer) const {
    if (!buffer || buffer->sampleCount < 5) return false;

    float totalAccel = 0.0f;
    for (uint16_t i = 0; i < buffer->sampleCount; i++) {
        float mag = sqrt(buffer->samples[i].x * buffer->samples[i].x +
                        buffer->samples[i].y * buffer->samples[i].y +
                        buffer->samples[i].z * buffer->samples[i].z);
        totalAccel += mag;
    }

    float avgAccel = totalAccel / buffer->sampleCount;
    return avgAccel > 1.5f; // More than 1.5 G average
}

bool MPU6050GestureRecognizer::hasOrientationChange(SampleBuffer* buffer) const {
    if (!buffer || buffer->sampleCount < 5) return false;

    // Check if gyro data is available
    bool hasGyro = false;
    float totalGyro = 0.0f;

    for (uint16_t i = 0; i < buffer->sampleCount; i++) {
        if (buffer->samples[i].gyroValid) {
            hasGyro = true;
            float gyroMag = sqrt(buffer->samples[i].gyroX * buffer->samples[i].gyroX +
                               buffer->samples[i].gyroY * buffer->samples[i].gyroY +
                               buffer->samples[i].gyroZ * buffer->samples[i].gyroZ);
            totalGyro += gyroMag;
        }
    }

    if (!hasGyro) return false;

    float avgGyro = totalGyro / buffer->sampleCount;
    return avgGyro > 0.3f; // Significant rotation (rad/s)
}

int MPU6050GestureRecognizer::shapeTypeToID(ShapeType shape) const {
    // IDs 100-199 for shapes
    return 100 + static_cast<int>(shape);
}

int MPU6050GestureRecognizer::orientationTypeToID(OrientationType orientation) const {
    // IDs 200-299 for orientations
    return 200 + static_cast<int>(orientation);
}

String MPU6050GestureRecognizer::shapeTypeToName(ShapeType shape) const {
    switch (shape) {
        case SHAPE_CIRCLE: return "circle";
        case SHAPE_LINE: return "line";
        case SHAPE_TRIANGLE: return "triangle";
        case SHAPE_SQUARE: return "square";
        case SHAPE_ZIGZAG: return "zigzag";
        case SHAPE_INFINITY: return "infinity";
        case SHAPE_SPIRAL: return "spiral";
        case SHAPE_ARC: return "arc";
        default: return "unknown";
    }
}

String MPU6050GestureRecognizer::orientationTypeToName(OrientationType orientation) const {
    switch (orientation) {
        case ORIENT_ROTATE_90_CW: return "rotate_cw";
        case ORIENT_ROTATE_90_CCW: return "rotate_ccw";
        case ORIENT_ROTATE_180: return "rotate_180";
        case ORIENT_TILT_FORWARD: return "tilt_forward";
        case ORIENT_TILT_BACKWARD: return "tilt_backward";
        case ORIENT_TILT_LEFT: return "tilt_left";
        case ORIENT_TILT_RIGHT: return "tilt_right";
        default: return "unknown";
    }
}
