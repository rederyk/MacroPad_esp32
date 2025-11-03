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

    // Variabili per tracciare movimento su ogni asse
    float accelXMin = 0.0f, accelXMax = 0.0f;
    float accelYMin = 0.0f, accelYMax = 0.0f;
    float accelZMin = 0.0f, accelZMax = 0.0f;

    float gyroXMin = 0.0f, gyroXMax = 0.0f;
    float gyroYMin = 0.0f, gyroYMax = 0.0f;
    float gyroZMin = 0.0f, gyroZMax = 0.0f;

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

    // SWIPE: trova l'asse con maggiore movimento (escludendo l'asse di gravità)
    // PRIORITÀ ALTA - controlliamo prima gli swipe!
    // Se giroscopio disponibile, usa la rotazione per determinare direzione
    float swipeThreshold = SWIPE_ACCEL_THRESHOLD;

    // Identifica l'asse con il movimento più significativo
    float maxMovement = 0.0f;
    int swipeAxis = -1; // -1=none, 0=X, 1=Y, 2=Z
    float swipeDirection = 0.0f; // positivo o negativo

    // Considera solo assi che NON sono l'asse di gravità principale
    if (!_gravityAxisX && accelXRange > maxMovement && accelXRange > swipeThreshold) {
        maxMovement = accelXRange;
        swipeAxis = 0;
        swipeDirection = (accelXMax + accelXMin) / 2.0f; // media come direzione
    }
    if (!_gravityAxisY && accelYRange > maxMovement && accelYRange > swipeThreshold) {
        maxMovement = accelYRange;
        swipeAxis = 1;
        swipeDirection = (accelYMax + accelYMin) / 2.0f;
    }
    if (!_gravityAxisZ && accelZRange > maxMovement && accelZRange > swipeThreshold) {
        maxMovement = accelZRange;
        swipeAxis = 2;
        swipeDirection = (accelZMax + accelZMin) / 2.0f;
    }

    // Se abbiamo un movimento significativo, determinalo come swipe
    if (swipeAxis >= 0) {
        // Usa il giroscopio per affinare la direzione se disponibile
        if (_hasGyro) {
            float gyroDirection = 0.0f;
            if (swipeAxis == 0) gyroDirection = gyroYRange > gyroZRange ? (gyroYMax + gyroYMin) : (gyroZMax + gyroZMin);
            else if (swipeAxis == 1) gyroDirection = gyroXRange > gyroZRange ? (gyroXMax + gyroXMin) : (gyroZMax + gyroZMin);
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
                                 String(swipeAxis) + " dir=" + String(swipeDirection, 2) +
                                 " (conf: " + String(result.confidence, 2) + ")");
        return result;
    }

    // SHAKE: movimento MOLTO rapido o rotazione MOLTO veloce su qualsiasi asse
    // Controlliamo DOPO gli swipe, con soglie ALTE
    bool hasVeryStrongAccel = maxAccelChange > SHAKE_ACCEL_THRESHOLD;
    bool hasVeryStrongGyro = maxGyroMag > SHAKE_GYRO_THRESHOLD;

    if (hasVeryStrongAccel || hasVeryStrongGyro) {
        result.gestureID = 203;
        result.gestureName = "G_SHAKE";
        float confidence = hasVeryStrongAccel ?
            (maxAccelChange / (SHAKE_ACCEL_THRESHOLD * 2.0f)) :
            (maxGyroMag / (SHAKE_GYRO_THRESHOLD * 2.0f));
        result.confidence = constrain(confidence, 0.5f, 1.0f);

        Logger::getInstance().log("MPU6050: SHAKE detected (conf: " +
                                 String(result.confidence, 2) + ")");
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
