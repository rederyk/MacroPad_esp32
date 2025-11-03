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
#include "SimpleGestureDetector.h"

static constexpr uint16_t kMinSamples = 3;

MPU6050GestureRecognizer::MPU6050GestureRecognizer()
    : _confidenceThreshold(0.5f)
{
}

MPU6050GestureRecognizer::~MPU6050GestureRecognizer() = default;

bool MPU6050GestureRecognizer::init(const String &sensorType)
{
    if (!sensorType.equalsIgnoreCase("mpu6050"))
    {
        Logger::getInstance().log("MPU6050GestureRecognizer: Wrong sensor type: " + sensorType);
        return false;
    }

    Logger::getInstance().log("MPU6050GestureRecognizer: using swipe/shake detection (accelerometer + gyro)");
    return true;
}

GestureRecognitionResult MPU6050GestureRecognizer::recognize(SampleBuffer *buffer)
{
    if (!buffer || buffer->sampleCount < kMinSamples)
    {
        Logger::getInstance().log("MPU6050GestureRecognizer: insufficient samples (" +
                                  String(buffer ? buffer->sampleCount : 0) + ")");
        return GestureRecognitionResult();
    }

    SimpleGestureConfig config;
    config.sensorTag = "MPU6050";
    config.sensorMode = SENSOR_MODE_MPU6050;
    config.useGyro = true;
    config.swipeAccelThreshold = 0.6f;
    config.shakeBidirectionalMin = 0.7f;
    config.shakeBidirectionalMax = 0.7f;
    config.shakeRangeThreshold = 1.8f;

    GestureRecognitionResult result = detectSimpleGesture(buffer, config);
    if (result.gestureID >= 0 && result.confidence < _confidenceThreshold)
    {
        Logger::getInstance().log("MPU6050GestureRecognizer: gesture discarded (confidence " +
                                  String(result.confidence, 2) + ")");
        return GestureRecognitionResult();
    }

    return result;
}
