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
    : _shapeRecognizer(),
      _confidenceThreshold(0.5f) {
    _shapeRecognizer.setMinMotionStdDev(0.25f);
    _shapeRecognizer.setUseMadgwick(true);
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

    // Fast check: prioritize orientation-based gestures if significant gyro activity
    float gyroActivity = calculateGyroActivity(buffer);

    GestureRecognitionResult result;

    // If significant rotation detected, try orientation first (more efficient)
    if (gyroActivity > 0.5f) {
        result = recognizeOrientation(buffer);

        // If orientation fails, try shape as fallback
        if (result.gestureID < 0) {
            result = recognizeShape(buffer);
        }
    } else {
        // For low gyro activity, try shape first
        result = recognizeShape(buffer);

        // If shape fails, try orientation as fallback
        if (result.gestureID < 0) {
            result = recognizeOrientation(buffer);
        }
    }

    if (result.gestureID >= 0 && result.confidence < _confidenceThreshold) {
        Logger::getInstance().log("MPU6050GestureRecognizer: Discarded low confidence gesture (" +
                                  String(result.confidence, 2) + ")");
        return GestureRecognitionResult();
    }

    return result;
}

GestureRecognitionResult MPU6050GestureRecognizer::recognizeShape(SampleBuffer* buffer) {
    GestureRecognitionResult result = _shapeRecognizer.recognize(buffer, SENSOR_MODE_MPU6050);

    if (result.gestureID >= 0) {
        Logger::getInstance().log(String("MPU6050 Shape: ") + result.gestureName +
                                  " (conf: " + String(result.confidence, 2) + ")");
    }

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
    if (orientationType == ORIENT_UNKNOWN || result.confidence <= 0.0f) {
        Logger::getInstance().log("MPU6050 Orientation: No valid orientation detected");
        return result;
    }
    result.gestureID = orientationTypeToID(orientationType);
    result.gestureName = orientationTypeToName(orientationType);

    Logger::getInstance().log(String("MPU6050 Orientation: ") + result.gestureName +
                            " (conf: " + String(result.confidence, 2) + ")");

    return result;
}

float MPU6050GestureRecognizer::calculateGyroActivity(SampleBuffer* buffer) const {
    if (!buffer || buffer->sampleCount < 3) return 0.0f;

    float maxGyro = 0.0f;
    float totalGyro = 0.0f;
    uint16_t validCount = 0;

    for (uint16_t i = 0; i < buffer->sampleCount; i++) {
        if (buffer->samples[i].gyroValid) {
            float gyroMag = sqrtf(buffer->samples[i].gyroX * buffer->samples[i].gyroX +
                                 buffer->samples[i].gyroY * buffer->samples[i].gyroY +
                                 buffer->samples[i].gyroZ * buffer->samples[i].gyroZ);
            totalGyro += gyroMag;
            if (gyroMag > maxGyro) maxGyro = gyroMag;
            validCount++;
        }
    }

    if (validCount == 0) return 0.0f;

    // Return average of mean and max to capture both sustained and peak rotation
    float avgGyro = totalGyro / validCount;
    return (avgGyro + maxGyro) * 0.5f;
}

bool MPU6050GestureRecognizer::hasOrientationChange(SampleBuffer* buffer) const {
    float activity = calculateGyroActivity(buffer);
    // Lower threshold for better detection (was 0.3)
    return activity > 0.2f; // ~11.5 degrees/sec average rotation
}

int MPU6050GestureRecognizer::orientationTypeToID(OrientationType orientation) const {
    // IDs 200-299 for orientations
    return 200 + static_cast<int>(orientation);
}

String MPU6050GestureRecognizer::orientationTypeToName(OrientationType orientation) const {
    switch (orientation) {
        case ORIENT_ROTATE_90_CW: return "G_ROTATE_90_CW";
        case ORIENT_ROTATE_90_CCW: return "G_ROTATE_90_CCW";
        case ORIENT_ROTATE_180: return "G_ROTATE_180";
        case ORIENT_TILT_FORWARD: return "G_TILT_FORWARD";
        case ORIENT_TILT_BACKWARD: return "G_TILT_BACKWARD";
        case ORIENT_TILT_LEFT: return "G_TILT_LEFT";
        case ORIENT_TILT_RIGHT: return "G_TILT_RIGHT";
        case ORIENT_FACE_UP: return "G_FACE_UP";
        case ORIENT_FACE_DOWN: return "G_FACE_DOWN";
        case ORIENT_SPIN: return "G_SPIN";
        case ORIENT_SHAKE_X_POS: return "G_SHAKE_X_POS";
        case ORIENT_SHAKE_X_NEG: return "G_SHAKE_X_NEG";
        case ORIENT_SHAKE_Y_POS: return "G_SHAKE_Y_POS";
        case ORIENT_SHAKE_Y_NEG: return "G_SHAKE_Y_NEG";
        case ORIENT_SHAKE_Z_POS: return "G_SHAKE_Z_POS";
        case ORIENT_SHAKE_Z_NEG: return "G_SHAKE_Z_NEG";
        default: return "G_UNKNOWN_ORIENT";
    }
}
