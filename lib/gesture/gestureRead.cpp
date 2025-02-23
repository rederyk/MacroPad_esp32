#include "gestureRead.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Logger.h>
#include <float.h> // For FLT_MAX
#include <vector>

GestureRead::GestureRead(uint8_t i2cAddress, TwoWire *wire)
    : _accelerometer(i2cAddress, wire), _isCalibrated(false), lastSampleTime(0)
{
    // Initialize in standby mode
    standby();
    _calibrationOffset = {0, 0, 0};
    _isSampling = false;
    _bufferFull = false;
    _maxSamples = 300; // Reduced buffer size
    _sampleHZ = 100;
    _axisMap = "zyx";
    _axisDir = "++-";

    // Allocate sample buffer
    _sampleBuffer.samples = new Sample[_maxSamples];
    if (!_sampleBuffer.samples)
    {
        Logger::getInstance().log("FATAL: Failed to allocate sample buffer!");
        while (1)
            ; // Halt if allocation fails
    }
    _sampleBuffer.sampleCount = 0;
    _sampleBuffer.maxSamples = _maxSamples;
    _sampleBuffer.sampleHZ = _sampleHZ;
}

bool GestureRead::begin()
{
    if (!_accelerometer.start())
    {
        return false;
    }

    // Default to 100Hz sampling and ±2g range
    _accelerometer.writeRate(ADXL345_RATE_100HZ);
    _accelerometer.writeRange(ADXL345_RANGE_4G);

    return standby();
}

GestureRead::~GestureRead()
{
    // Stop any active sampling
    if (_isSampling)
    {
        stopSampling();
    }

    // Clean up sample buffer
    if (_sampleBuffer.samples)
    {
        delete[] _sampleBuffer.samples;
        _sampleBuffer.samples = nullptr;
    }
}

void GestureRead::clearMemory()
{
    std::lock_guard<std::mutex> lock(_bufferMutex);

    // Reset raw sample buffer
    if (_sampleBuffer.samples)
    {
        memset(_sampleBuffer.samples, 0, _maxSamples * sizeof(Sample));
        _sampleBuffer.sampleCount = 0;
        _bufferFull = false;
    }
}

bool GestureRead::calibrate(uint16_t calibrationSamples)
{
    if (calibrationSamples == 0)
        calibrationSamples = 2;

    float sumX = 0, sumY = 0, sumZ = 0;

    // Wake up accelerometer and clear buffer
    if (!wakeup())
    {
        return false;
    }

    // Disaable low power mode
    if (!disableLowPowerMode())
    {
        return false;
    }

    for (uint16_t i = 0; i < calibrationSamples; i++)
    {

        // Take reading
        if (_accelerometer.update())
        {
            sumX += getMappedX();
            sumY += getMappedY();
            sumZ += getMappedZ();
        }

        delay(10);
    }

    _calibrationOffset.x = sumX / calibrationSamples;
    _calibrationOffset.y = sumY / calibrationSamples;
    _calibrationOffset.z = sumZ / calibrationSamples;

    _isCalibrated = true;

    return standby(); // Put accelerometer in low power mode
}

bool GestureRead::startSampling()
{
    if (_isSampling)
    {
        return false; // Already sampling
    }

    // Wake up accelerometer and clear buffer
    if (!wakeup())
    {
        return false;
    }

    // Disaable low power mode
    if (!disableLowPowerMode())
    {
        return false;
    }

    clearMemory();
    _isSampling = true;
    return true;
}
bool GestureRead::isSampling()
{
    std::lock_guard<std::mutex> lock(_bufferMutex);
    return _isSampling;
}

bool GestureRead::stopSampling()
{
    if (!_isSampling)
    {
        return false; // Not currently sampling
    }

    _isSampling = false;
    if (_sampleBuffer.sampleCount >= _maxSamples)
    {
        Logger::getInstance().log("Stopped sampling - buffer full (" + String(_maxSamples) + " samples collected)");
        return true;
    }
    else
    {
        return standby(); // Put accelerometer in low power mode
    }
}

SampleBuffer &GestureRead::getCollectedSamples()
{
    std::lock_guard<std::mutex> lock(_bufferMutex);
    return _sampleBuffer;
}

// Power management implementations
bool GestureRead::enableLowPowerMode()
{
    return _accelerometer.writeRateWithLowPower(ADXL345_RATE_12_5HZ);
}

bool GestureRead::disableLowPowerMode()
{
    return _accelerometer.writeRate(ADXL345_RATE_100HZ);
}

bool GestureRead::standby()
{
    return _accelerometer.stop();
}

bool GestureRead::wakeup()
{
    return _accelerometer.start();
}


float GestureRead::getMappedX()
//TODO dont calculate if axis is in the same axis and dir 

{

    float value = 0;
    switch (_axisMap[0])
    {
    case 'x':
        value = _accelerometer.getX() * (_axisDir[0] == '-' ? -1 : 1);
        break;
    case 'y':
        value = _accelerometer.getY() * (_axisDir[0] == '-' ? -1 : 1);
        break;
    case 'z':
        value = _accelerometer.getZ() * (_axisDir[0] == '-' ? -1 : 1);
        break;
    default:
        return 0;
    }
    return value;
}

float GestureRead::getMappedY()
{
    float value = 0;
    switch (_axisMap[1])
    {
    case 'x':
        value = _accelerometer.getX() * (_axisDir[1] == '-' ? -1 : 1);
        break;
    case 'y':
        value = _accelerometer.getY() * (_axisDir[1] == '-' ? -1 : 1);
        break;
    case 'z':
        value = _accelerometer.getZ() * (_axisDir[1] == '-' ? -1 : 1);
        break;
    default:
        return 0;
    }
    return value;
}

float GestureRead::getMappedZ()
{
    float value = 0;
    switch (_axisMap[2])
    {
    case 'x':
        value = _accelerometer.getX() * (_axisDir[2] == '-' ? -1 : 1);
        break;
    case 'y':
        value = _accelerometer.getY() * (_axisDir[2] == '-' ? -1 : 1);
        break;
    case 'z':
        value = _accelerometer.getZ() * (_axisDir[2] == '-' ? -1 : 1);
        break;
    default:
        return 0;
    }
    return value;
}

void GestureRead::updateSampling()
{
    std::lock_guard<std::mutex> lock(_bufferMutex);

    unsigned long currentTime = millis();

    if (currentTime - lastSampleTime >= 10)
    { // 100Hz = 10ms
        bool sampling = _isSampling;
        uint16_t count = _sampleBuffer.sampleCount;

        if (sampling && count < _maxSamples)
        {
            if (_accelerometer.update())
            {
                _sampleBuffer.samples[count].x =
                    getMappedX() - _calibrationOffset.x;
                _sampleBuffer.samples[count].y =
                    getMappedY() - _calibrationOffset.y;
                _sampleBuffer.samples[count].z =
                    getMappedZ() - _calibrationOffset.z;
                _sampleBuffer.sampleCount = count + 1;
                lastSampleTime = currentTime;

               // Logger::getInstance().log(String(_sampleBuffer.sampleCount) + String("Sample X ") + String(_sampleBuffer.samples[count].x));
            }
        }
        else if (sampling && count >= _maxSamples)
        {
            _bufferFull = true;
            stopSampling();
            return;
        }
        else
        {
            stopSampling();
            return;
        }
    }
}
