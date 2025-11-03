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
#include <cmath>

// Soglie per il rilevamento
static constexpr float SWIPE_ACCEL_THRESHOLD = 0.6f;   // g per swipe (movimento laterale)
static constexpr float SHAKE_ACCEL_THRESHOLD = 2.5f;   // g per shake (movimento MOLTO rapido)
static constexpr float SHAKE_GYRO_THRESHOLD = 8.0f;    // rad/s per shake con rotazione (molto alta!)
static constexpr float MIN_GESTURE_SAMPLES = 3;        // Minimo 3 campioni per gesto valido

MPU6050GestureRecognizer::MPU6050GestureRecognizer()
    : _confidenceThreshold(0.5f),
      _hasGyro(false),
      _gravityAxisX(false),
      _gravityAxisY(false),
      _gravityAxisZ(false) {
}

MPU6050GestureRecognizer::~MPU6050GestureRecognizer() {
}

bool MPU6050GestureRecognizer::init(const String& sensorType) {
    if (sensorType != "mpu6050") {
        Logger::getInstance().log("MPU6050GestureRecognizer: Wrong sensor type: " + sensorType);
        return false;
    }

    Logger::getInstance().log("MPU6050GestureRecognizer: Simple swipe & shake detection enabled");
    return true;
}

GestureRecognitionResult MPU6050GestureRecognizer::recognize(SampleBuffer* buffer) {
    if (!buffer || buffer->sampleCount < MIN_GESTURE_SAMPLES) {
        Logger::getInstance().log("MPU6050GestureRecognizer: Not enough samples (" +
                                  String(buffer ? buffer->sampleCount : 0) + ")");
        return GestureRecognitionResult();
    }

    GestureRecognitionResult result;
    result.sensorMode = SENSOR_MODE_MPU6050;

    // Rileva il giroscopio e l'orientamento dalla prima passata
    detectOrientationFromData(buffer);

    // Inizializza min/max dal primo campione invece che da zero
    // Questo previene che lo zero influenzi i range di movimento
    Sample& firstSample = buffer->samples[0];
    float accelXMin = firstSample.x, accelXMax = firstSample.x;
    float accelYMin = firstSample.y, accelYMax = firstSample.y;
    float accelZMin = firstSample.z, accelZMax = firstSample.z;

    float gyroXMin = firstSample.gyroX, gyroXMax = firstSample.gyroX;
    float gyroYMin = firstSample.gyroY, gyroYMax = firstSample.gyroY;
    float gyroZMin = firstSample.gyroZ, gyroZMax = firstSample.gyroZ;

    float maxGyroMag = 0.0f;
    float maxAccelChange = 0.0f;

    // Analizza tutti i campioni
    for (uint16_t i = 0; i < buffer->sampleCount; i++) {
        Sample& sample = buffer->samples[i];

        // Traccia min/max per ogni asse accelerometro
        if (sample.x < accelXMin) accelXMin = sample.x;
        if (sample.x > accelXMax) accelXMax = sample.x;
        if (sample.y < accelYMin) accelYMin = sample.y;
        if (sample.y > accelYMax) accelYMax = sample.y;
        if (sample.z < accelZMin) accelZMin = sample.z;
        if (sample.z > accelZMax) accelZMax = sample.z;

        // Calcola variazione accelerazione totale
        float accelChange = sqrtf(sample.x * sample.x +
                                 sample.y * sample.y +
                                 sample.z * sample.z);
        if (accelChange > maxAccelChange) {
            maxAccelChange = accelChange;
        }

        // Analizza giroscopio se disponibile
        if (sample.gyroValid) {
            if (sample.gyroX < gyroXMin) gyroXMin = sample.gyroX;
            if (sample.gyroX > gyroXMax) gyroXMax = sample.gyroX;
            if (sample.gyroY < gyroYMin) gyroYMin = sample.gyroY;
            if (sample.gyroY > gyroYMax) gyroYMax = sample.gyroY;
            if (sample.gyroZ < gyroZMin) gyroZMin = sample.gyroZ;
            if (sample.gyroZ > gyroZMax) gyroZMax = sample.gyroZ;

            float gyroMag = sqrtf(sample.gyroX * sample.gyroX +
                                 sample.gyroY * sample.gyroY +
                                 sample.gyroZ * sample.gyroZ);
            if (gyroMag > maxGyroMag) {
                maxGyroMag = gyroMag;
            }
        }
    }

    // Calcola escursione (range) su ogni asse
    float accelXRange = accelXMax - accelXMin;
    float accelYRange = accelYMax - accelYMin;
    float accelZRange = accelZMax - accelZMin;

    float gyroXRange = gyroXMax - gyroXMin;
    float gyroYRange = gyroYMax - gyroYMin;
    float gyroZRange = gyroZMax - gyroZMin;

    // Log dettagliato per debug
    Logger::getInstance().log("MPU6050: accelRange X=" + String(accelXRange, 2) +
                             " Y=" + String(accelYRange, 2) +
                             " Z=" + String(accelZRange, 2));
    Logger::getInstance().log("MPU6050: gyroRange X=" + String(gyroXRange, 2) +
                             " Y=" + String(gyroYRange, 2) +
                             " Z=" + String(gyroZRange, 2) +
                             " maxMag=" + String(maxGyroMag, 2));

    // Strategia: trova l'asse con maggior movimento (escludendo gravità)
    // Poi controlla se è oscillazione (SHAKE) o movimento unidirezionale (SWIPE)

    float swipeThreshold = SWIPE_ACCEL_THRESHOLD;

    // Trova l'asse con movimento più significativo
    float maxMovement = 0.0f;
    int gestureAxis = -1; // -1=none, 0=X, 1=Y, 2=Z

    if (!_gravityAxisX && accelXRange > maxMovement && accelXRange > swipeThreshold) {
        maxMovement = accelXRange;
        gestureAxis = 0;
    }
    if (!_gravityAxisY && accelYRange > maxMovement && accelYRange > swipeThreshold) {
        maxMovement = accelYRange;
        gestureAxis = 1;
    }
    if (!_gravityAxisZ && accelZRange > maxMovement && accelZRange > swipeThreshold) {
        maxMovement = accelZRange;
        gestureAxis = 2;
    }

    if (gestureAxis >= 0) {
        // Abbiamo movimento significativo su un asse
        // Determina se è SHAKE (bidirezionale) o SWIPE (unidirezionale)

        float axisMin = (gestureAxis == 0) ? accelXMin : (gestureAxis == 1) ? accelYMin : accelZMin;
        float axisMax = (gestureAxis == 0) ? accelXMax : (gestureAxis == 1) ? accelYMax : accelZMax;

        // SHAKE: movimento in ENTRAMBE le direzioni (min negativo E max positivo)
        // Soglie più alte per evitare falsi positivi: min < -0.7g E max > 0.7g
        // Inoltre richiede un range minimo di 1.8g per essere considerato shake
        bool isBidirectional = (axisMin < -0.7f && axisMax > 0.7f && maxMovement > 1.8f);

        if (isBidirectional) {
            // È uno SHAKE!
            result.gestureID = 203;
            result.gestureName = "G_SHAKE";
            result.confidence = constrain(maxMovement / 3.0f, 0.5f, 1.0f);

            Logger::getInstance().log("MPU6050: SHAKE detected on axis=" + String(gestureAxis) +
                                     " range=" + String(maxMovement, 2) +
                                     " min=" + String(axisMin, 2) +
                                     " max=" + String(axisMax, 2) +
                                     " (conf: " + String(result.confidence, 2) + ")");
            return result;
        }

        // SWIPE: movimento principalmente unidirezionale
        float swipeDirection = (axisMax + axisMin) / 2.0f;

        // Usa il giroscopio per affinare la direzione se disponibile
        if (_hasGyro) {
            float gyroDirection = 0.0f;
            if (gestureAxis == 0) gyroDirection = gyroYRange > gyroZRange ? (gyroYMax + gyroYMin) : (gyroZMax + gyroZMin);
            else if (gestureAxis == 1) gyroDirection = gyroXRange > gyroZRange ? (gyroXMax + gyroXMin) : (gyroZMax + gyroZMin);
            else gyroDirection = gyroXRange > gyroYRange ? (gyroXMax + gyroXMin) : (gyroYMax + gyroYMin);

            // Il giroscopio determina se è left o right
            if (fabsf(gyroDirection) > 0.5f) {
                swipeDirection = gyroDirection;
            }
        }

        // Determina left vs right in base alla direzione
        if (swipeDirection > 0) {
            result.gestureID = 201;
            result.gestureName = "G_SWIPE_RIGHT";
        } else {
            result.gestureID = 202;
            result.gestureName = "G_SWIPE_LEFT";
        }

        result.confidence = constrain(maxMovement / (swipeThreshold * 2.0f), 0.5f, 1.0f);

        Logger::getInstance().log("MPU6050: " + result.gestureName + " detected on axis=" +
                                 String(gestureAxis) + " dir=" + String(swipeDirection, 2) +
                                 " min=" + String(axisMin, 2) + " max=" + String(axisMax, 2) +
                                 " (conf: " + String(result.confidence, 2) + ")");
        return result;
    }

    Logger::getInstance().log("MPU6050: No gesture recognized");
    return result;
}

void MPU6050GestureRecognizer::detectOrientationFromData(SampleBuffer* buffer) {
    if (!buffer || buffer->sampleCount == 0) return;

    // Usa il primo campione per determinare l'orientamento
    Sample& first = buffer->samples[0];

    _hasGyro = first.gyroValid;

    // Determina quale asse ha la maggiore componente di gravità
    float absX = fabsf(first.x);
    float absY = fabsf(first.y);
    float absZ = fabsf(first.z);

    // L'asse con valore più vicino a 1g (o comunque il più alto) è l'asse verticale
    _gravityAxisX = (absX > absY && absX > absZ && absX > 0.5f);
    _gravityAxisY = (absY > absX && absY > absZ && absY > 0.5f);
    _gravityAxisZ = (absZ > absX && absZ > absY && absZ > 0.5f);

    Logger::getInstance().log("MPU6050: Gravity on " +
                             String(_gravityAxisX ? "X" : _gravityAxisY ? "Y" : _gravityAxisZ ? "Z" : "NONE") +
                             " (hasGyro=" + String(_hasGyro) + ")");
}

// Metodi stub (mantenuti per compatibilità)
GestureRecognitionResult MPU6050GestureRecognizer::recognizeShape(SampleBuffer* buffer) {
    return GestureRecognitionResult();
}

GestureRecognitionResult MPU6050GestureRecognizer::recognizeOrientation(SampleBuffer* buffer) {
    return GestureRecognitionResult();
}

float MPU6050GestureRecognizer::calculateGyroActivity(SampleBuffer* buffer) const {
    return 0.0f;
}

bool MPU6050GestureRecognizer::hasOrientationChange(SampleBuffer* buffer) const {
    return false;
}

int MPU6050GestureRecognizer::orientationTypeToID(int orientation) const {
    return -1;
}

String MPU6050GestureRecognizer::orientationTypeToName(int orientation) const {
    return "G_UNKNOWN";
}
