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
};

struct SampleBuffer
{
    Sample *samples; // Pointer to dynamically allocated samples
    uint16_t sampleCount;
    uint16_t maxSamples;
    uint16_t sampleHZ;
};

class AccelerometerDriver;

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

private:
    float getAxisValue(uint8_t axisIndex);
    void applyAxisMap(const String &axisMap);

    std::unique_ptr<AccelerometerDriver> _driver;
    AccelerometerConfig _config;
    bool _configLoaded;
    TwoWire *_wire;

    Offset _calibrationOffset;
    bool _isCalibrated;
    String _axisMap;
    String _axisDir;

    // New members for continuous sampling
    std::mutex _bufferMutex;
    bool _isSampling;
    bool _bufferFull;
    unsigned long lastSampleTime;

    SampleBuffer _sampleBuffer;
    uint16_t _maxSamples;
    uint16_t _sampleHZ;
};

extern GestureRead gestureSensor; // Dichiarazione extern per l'istanza globale

#endif
