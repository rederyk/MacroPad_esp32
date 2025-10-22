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


#ifndef GESTURE_STORAGE_H
#define GESTURE_STORAGE_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#define GESTURE_STORAGE_FILE "/gesture_features.json"
#define MAX_GESTURES 9
#define MAX_SAMPLES 10
#define MAX_FEATURES 20 // Accelerometer (15) + gyro magnitude (5)

class GestureStorage {
public:
    GestureStorage();
    bool saveGestureFeature(uint8_t gestureID, const float* features, size_t featureCount);
    bool loadGestureFeatures();
    bool clearGestureFeatures(uint8_t gestureID = UINT8_MAX);
    void printJsonFeatures();

    // New methods for 3D matrix conversion and binary storage
    float*** convertJsonToMatrix3D(size_t &numGestures, size_t &numSamples, size_t &numFeatures);
    bool saveMatrixToBinary(const char* filename, float*** matrix, size_t numGestures, size_t numSamples, size_t numFeatures);
    void clearMatrix3D(float***& matrix, size_t numGestures, size_t numSamples);
    float*** loadMatrixFromBinary(const char* filename, size_t &numGestures, size_t &numSamples, size_t &numFeatures);

private:
    bool initializeFileSystem();
    bool ensureFileExists();
    StaticJsonDocument<12288> doc; // Increased size for gyro-augmented gesture data
};

extern GestureStorage gestureStorage;

#endif
