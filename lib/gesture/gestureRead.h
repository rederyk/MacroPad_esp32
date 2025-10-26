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


#ifndef GESTUREREAD_H
#define GESTUREREAD_H

#include <Arduino.h>
#include <Wire.h>
#include "configTypes.h"
#include "MotionSensor.h"
#include <memory>
#include <mutex>
#include <vector>

struct Offset
{
    float x;
    float y;
    float z;
};

struct Sample
{
    float x;
    float y;
    float z;
    float gyroX;
    float gyroY;
    float gyroZ;
    float temperature;
    bool gyroValid;
    bool temperatureValid;
};

struct SampleBuffer
{
    Sample *samples; // Pointer to dynamically allocated samples
    uint16_t sampleCount;
    uint16_t maxSamples;
    uint16_t sampleHZ;
};

class GestureRead
{
public:
    GestureRead(TwoWire *wire = &Wire);
    ~GestureRead(); // Destructor declaration

    bool begin(const AccelerometerConfig &config);
    bool calibrate(uint16_t calibrationSamples = 5);

    // Power management methods
    bool enableLowPowerMode();
    bool disableLowPowerMode();
    bool standby();
    bool wakeup();

    bool configureMotionWakeup(uint8_t threshold, uint8_t duration, uint8_t highPassCode, uint8_t cycleRateCode);
    bool disableMotionWakeup();
    bool isMotionWakeEnabled() const;
    bool clearMotionWakeInterrupt();
    bool isMotionWakeTriggered();

    // New methods for continuous sampling
    bool startSampling();
    bool stopSampling();
    bool isSampling();
    SampleBuffer &getCollectedSamples();
    void clearMemory();

    // Mapped axis accessors
    float getMappedX();
    float getMappedY();
    float getMappedZ();

    void updateSampling(); // Call this regularly from main loop

    // Auto-calibration controls
    void setAutoCalibrationEnabled(bool enable);
    void setAutoCalibrationParameters(float gyroStillThresholdRad, uint16_t minStableSamples, float smoothingFactor);
    bool isAutoCalibrationEnabled() const;

    // Get underlying motion sensor (for axis calibration)
    MotionSensor* getMotionSensor() { return _sensor.get(); }

private:
    void getMappedGyro(float &x, float &y, float &z);
    void resetAutoCalibrationState();
    void updateAutoCalibration(const float rawAccel[3], const float mappedGyro[3], bool gyroValid);
    bool waitForGyroReady(uint32_t timeoutMs);

    std::unique_ptr<MotionSensor> _sensor;
    AccelerometerConfig _config;
    bool _configLoaded;
    TwoWire *_wire;

    Offset _calibrationOffset;
    bool _isCalibrated;
    // New members for continuous sampling
    std::mutex _bufferMutex;
    bool _isSampling;
    bool _bufferFull;
    unsigned long lastSampleTime;
    bool _motionWakeEnabled;
    bool _expectGyro;

    SampleBuffer _sampleBuffer;
    uint16_t _maxSamples;
    uint16_t _sampleHZ;

    struct AutoCalibrationState
    {
        bool enabled;
        float gyroStillThreshold;
        uint16_t minStableSamples;
        float smoothingFactor;
        uint16_t stableCount;
    } _autoCalib;
};

extern GestureRead gestureSensor; // Dichiarazione extern per l'istanza globale

#endif
