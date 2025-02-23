#ifndef GESTURE_STORAGE_H
#define GESTURE_STORAGE_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#define GESTURE_STORAGE_FILE "/gesture_features.json"
#define MAX_GESTURES 9
#define MAX_SAMPLES 10
#define MAX_FEATURES 15 // Number of features per sample (x, y, z)

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
    StaticJsonDocument<8192> doc; // Increased size for more gesture data
};

extern GestureStorage gestureStorage;

#endif
