/*
 * ESP32 MacroPad Project
 * Copyright (C)
 *
 * ADXL345 predefined gesture recognizer implementation.
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

ADXL345GestureRecognizer::ADXL345GestureRecognizer()
    : _shapeRecognizer(),
      _confidenceThreshold(0.5f)
{
    // ADXL345 exposes only accelerometer data, so rely on linear motion analysis.
    _shapeRecognizer.setMinMotionStdDev(0.18f); // Lower threshold for less sensitive sensor
    _shapeRecognizer.setUseMadgwick(false);     // No gyro available
}

ADXL345GestureRecognizer::~ADXL345GestureRecognizer() = default;

bool ADXL345GestureRecognizer::init(const String &sensorType)
{
    if (!sensorType.equalsIgnoreCase("adxl345"))
    {
        Logger::getInstance().log("ADXL345GestureRecognizer: Wrong sensor type: " + sensorType);
        return false;
    }

    Logger::getInstance().log("ADXL345GestureRecognizer: Initialized for predefined shape recognition (no training required)");
    return true;
}

GestureRecognitionResult ADXL345GestureRecognizer::recognize(SampleBuffer *buffer)
{
    GestureRecognitionResult result = _shapeRecognizer.recognize(buffer, SENSOR_MODE_ADXL345);

    if (result.gestureID < 0)
    {
        Logger::getInstance().log("ADXL345GestureRecognizer: No shape gesture recognised");
        return result;
    }

    if (result.confidence < _confidenceThreshold)
    {
        Logger::getInstance().log("ADXL345GestureRecognizer: Discarded low confidence gesture (" +
                                  String(result.confidence, 2) + ")");
        return GestureRecognitionResult();
    }

    Logger::getInstance().log(String("ADXL345 Shape: ") + result.gestureName +
                              " (conf: " + String(result.confidence, 2) + ")");
    return result;
}
