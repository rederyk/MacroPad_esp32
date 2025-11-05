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
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

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
    void ensureMinimumSamplingTime(); // Wait if sampling hasn't reached minimum time yet
    bool stopSampling();
    bool isSampling();
    SampleBuffer &getCollectedSamples();
    void clearMemory();
    void flushSensorBuffer(); // Flush hardware buffer to discard stale data

    // Mapped axis accessors
    float getMappedX();
    float getMappedY();
    float getMappedZ();
    void getMappedGyro(float &x, float &y, float &z);
    float getMappedGyroX();
    float getMappedGyroY();
    float getMappedGyroZ();

    void updateSampling(); // Driven by internal sampling task; exposed for testing if needed

    // Get underlying motion sensor (for axis calibration)
    MotionSensor* getMotionSensor() { return _sensor.get(); }

    void setStreamingMode(bool enable);
    bool isStreamingMode() const { return _streamingMode; }

private:
    static void samplingTaskTrampoline(void *param);
    bool ensureSamplingTask();
    void samplingTaskLoop();
    void drainSensorBuffer(uint32_t timeoutMs, uint32_t waitMs, uint8_t stableReads);
    bool waitForGyroReady(uint32_t timeoutMs);
    bool waitForFreshAccelerometer(uint32_t timeoutMs);
    void clearMemoryNoLock(); // Clear buffer without acquiring lock (must be called with lock held)

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
    unsigned long samplingStartTime;
    bool _motionWakeEnabled;
    bool _expectGyro;
    bool _streamingMode;
    uint16_t _writeIndex;
    uint32_t _totalSamples;

    SampleBuffer _sampleBuffer;
    uint16_t _maxSamples;
    uint16_t _sampleHZ;
    TaskHandle_t _samplingTaskHandle;
    volatile bool _samplingTaskShouldRun;
};

extern GestureRead gestureSensor; // Dichiarazione extern per l'istanza globale

#endif
