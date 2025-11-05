/*
 * ESP32 MacroPad Project
 * Copyright (C) [2025] [Enrico Mori]
 *
 * Automatic Axis Calibration Implementation
 */

#include "gestureAxisCalibration.h"
#include "Logger.h"
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <cmath>

AxisCalibrationResult AxisCalibration::calibrate(GestureRead* gestureRead, uint32_t samplingTimeMs)
{
    AxisCalibrationResult result;

    if (!gestureRead) {
        Logger::getInstance().log("[AxisCalibration] GestureRead instance is null");
        return result;
    }

    Logger::getInstance().log("[AxisCalibration] Starting calibration...");
    Logger::getInstance().log("[AxisCalibration] Hold device in normal position (buttons facing you, vertical)");
    Logger::getInstance().log("[AxisCalibration] Keep still for " + String(samplingTimeMs / 1000) + " seconds...");

    // Start sampling at 100Hz using the dedicated task
    if (!gestureRead->startSampling()) {
        Logger::getInstance().log("[AxisCalibration] Failed to start sampling");
        return result;
    }

    // Wait for the specified duration
    delay(samplingTimeMs);

    // Stop sampling
    gestureRead->stopSampling();

    // Get collected samples
    SampleBuffer& samples = gestureRead->getCollectedSamples();

    if (samples.sampleCount == 0) {
        Logger::getInstance().log("[AxisCalibration] No samples collected");
        return result;
    }

    Logger::getInstance().log("[AxisCalibration] Collected " + String(samples.sampleCount) + " samples at " +
                             String(samples.sampleHZ) + "Hz");

    // Calculate averages from all samples
    float avgX = 0, avgY = 0, avgZ = 0;
    for (uint16_t i = 0; i < samples.sampleCount; i++) {
        // Samples are already calibrated (offset removed), so we need to add offset back to get raw values
        avgX += samples.samples[i].x;
        avgY += samples.samples[i].y;
        avgZ += samples.samples[i].z;
    }

    avgX /= samples.sampleCount;
    avgY /= samples.sampleCount;
    avgZ /= samples.sampleCount;

    Logger::getInstance().log("[AxisCalibration] Averages: X=" + String(avgX, 3) +
                             " Y=" + String(avgY, 3) + " Z=" + String(avgZ, 3));

    // Determine axis mapping
    determineAxisMapping(avgX, avgY, avgZ, result.axisMap, result.axisDir, result.confidence);

    result.success = result.confidence > 0.7f;

    if (result.success) {
        Logger::getInstance().log("[AxisCalibration] SUCCESS: axisMap=\"" + result.axisMap +
                                 "\", axisDir=\"" + result.axisDir +
                                 "\", confidence=" + String(result.confidence * 100, 0) + "%");
    } else {
        Logger::getInstance().log("[AxisCalibration] FAILED: Confidence too low (" +
                                 String(result.confidence * 100, 0) + "%)");
        Logger::getInstance().log("[AxisCalibration] Make sure device is held still and vertical");
    }

    return result;
}

int AxisCalibration::findGravityAxis(float x, float y, float z)
{
    float absX = fabs(x);
    float absY = fabs(y);
    float absZ = fabs(z);

    // Find which axis has strongest gravity component (~1g)
    if (absZ > absX && absZ > absY) {
        return 2;  // Z axis
    } else if (absY > absX) {
        return 1;  // Y axis
    } else {
        return 0;  // X axis
    }
}

void AxisCalibration::determineAxisMapping(float rawX, float rawY, float rawZ,
                                          String& axisMap, String& axisDir, float& confidence)
{
    // Expected orientation: Device vertical, buttons facing user
    // Expected readings:
    // - Gravity should be predominantly on ONE axis (the "down" axis)
    // - That axis should read close to -1g or +1g
    // - Other two axes should read close to 0g

    // Find which physical axis has gravity
    int gravityAxis = findGravityAxis(rawX, rawY, rawZ);

    // Get the actual values
    float axes[3] = {rawX, rawY, rawZ};
    float gravityValue = axes[gravityAxis];

    // Calculate magnitude (should be close to 1g)
    float magnitude = sqrt(rawX * rawX + rawY * rawY + rawZ * rawZ);

    // Calculate how "vertical" the device is (other axes should be ~0)
    float otherAxis1 = 0, otherAxis2 = 0;
    if (gravityAxis == 0) {
        otherAxis1 = rawY;
        otherAxis2 = rawZ;
    } else if (gravityAxis == 1) {
        otherAxis1 = rawX;
        otherAxis2 = rawZ;
    } else {
        otherAxis1 = rawX;
        otherAxis2 = rawY;
    }

    float horizontalMagnitude = sqrt(otherAxis1 * otherAxis1 + otherAxis2 * otherAxis2);

    // Confidence based on:
    // 1. Total magnitude close to 1g
    // 2. Horizontal components small (device is vertical)
    float magnitudeError = fabs(magnitude - 1.0f);
    confidence = 1.0f - magnitudeError;  // 1.0 = perfect 1g
    confidence *= (1.0f - fmin(horizontalMagnitude, 1.0f));  // Reduce if not vertical

    Logger::getInstance().log("[AxisCalibration] Gravity on axis " + String(gravityAxis) +
                             " = " + String(gravityValue, 3) + "g");
    Logger::getInstance().log("[AxisCalibration] Magnitude: " + String(magnitude, 3) +
                             "g, Horizontal: " + String(horizontalMagnitude, 3) + "g");

    // Determine axis mapping based on which physical axis has gravity
    // Standard orientation: Z-axis should have gravity (device vertical)
    // If gravity is on different axis, we need to remap

    char mapChars[4] = "xyz";
    char dirChars[4] = "+++";

    if (gravityAxis == 2) {
        // Z has gravity (standard) - no remapping needed
        axisMap = "xyz";
        dirChars[2] = (gravityValue < 0) ? '+' : '-';  // Z should be negative (down)
    } else if (gravityAxis == 1) {
        // Y has gravity - swap Y and Z
        mapChars[0] = 'x';
        mapChars[1] = 'z';
        mapChars[2] = 'y';
        axisMap = String(mapChars);
        dirChars[2] = (gravityValue < 0) ? '+' : '-';
    } else {
        // X has gravity - swap X and Z
        mapChars[0] = 'z';
        mapChars[1] = 'y';
        mapChars[2] = 'x';
        axisMap = String(mapChars);
        dirChars[2] = (gravityValue < 0) ? '+' : '-';
    }

    // Set directions for horizontal axes
    dirChars[0] = '+';
    dirChars[1] = '+';

    axisDir = String(dirChars);

    Logger::getInstance().log("[AxisCalibration] Determined: axisMap=\"" + axisMap +
                             "\", axisDir=\"" + axisDir + "\"");
}

bool AxisCalibration::saveToConfig(const AxisCalibrationResult& result, const char* configPath)
{
    if (!result.success) {
        Logger::getInstance().log("[AxisCalibration] Cannot save failed calibration");
        return false;
    }

    // Mount filesystem
    if (!LittleFS.begin()) {
        Logger::getInstance().log("[AxisCalibration] Failed to mount LittleFS");
        return false;
    }

    // Read existing config
    File file = LittleFS.open(configPath, "r");
    if (!file) {
        Logger::getInstance().log("[AxisCalibration] Failed to open config file");
        LittleFS.end();
        return false;
    }

    // Parse JSON
    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Logger::getInstance().log("[AxisCalibration] Failed to parse JSON: " + String(error.c_str()));
        LittleFS.end();
        return false;
    }

    // Update accelerometer settings
    if (doc.containsKey("accelerometer")) {
        doc["accelerometer"]["axisMap"] = result.axisMap;
        doc["accelerometer"]["axisDir"] = result.axisDir;
    } else {
        Logger::getInstance().log("[AxisCalibration] No accelerometer section in config");
        LittleFS.end();
        return false;
    }

    // Write back to file
    file = LittleFS.open(configPath, "w");
    if (!file) {
        Logger::getInstance().log("[AxisCalibration] Failed to open config for writing");
        LittleFS.end();
        return false;
    }

    serializeJson(doc, file);
    file.close();
    LittleFS.end();

    Logger::getInstance().log("[AxisCalibration] Calibration saved to " + String(configPath));
    Logger::getInstance().log("[AxisCalibration] Restart device to apply changes");

    return true;
}
