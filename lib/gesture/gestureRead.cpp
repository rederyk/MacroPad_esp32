#include "gestureRead.h"

#include <Arduino.h>
#include <Logger.h>
#include "Led.h"

#include <algorithm>
#include <cmath>
#include <cstring>

constexpr uint32_t kGyroReadyTimeoutMs = 300;
constexpr uint32_t kGyroReadyPollDelayMs = 5;
constexpr float kGyroReadyMinAccelSum = 0.05f;
constexpr float kGyroReadyNoiseFloor = 1e-4f;
constexpr uint8_t kGyroReadyZeroTolerance = 5;

GestureRead::GestureRead(TwoWire *wire)
    : _sensor(new MotionSensor(wire)),
      _configLoaded(false),
      _wire(wire),
      _isCalibrated(false),
      _isSampling(false),
      _bufferFull(false),
      lastSampleTime(0),
      _motionWakeEnabled(false),
      _expectGyro(false)
{
    _calibrationOffset = {0, 0, 0};
    _sampleHZ = MotionSensor::kDefaultSampleHz;
    _maxSamples = 300;

    _sampleBuffer.samples = new Sample[_maxSamples];
    if (!_sampleBuffer.samples)
    {
        Logger::getInstance().log("FATAL: Failed to allocate sample buffer!");
        while (true)
            ;
    }
    _sampleBuffer.sampleCount = 0;
    _sampleBuffer.maxSamples = _maxSamples;
    _sampleBuffer.sampleHZ = _sampleHZ;

    _autoCalib.enabled = true;
    _autoCalib.gyroStillThreshold = 0.12f; // ~7 deg/s
    _autoCalib.minStableSamples = 15;       // ~150 ms at 100 Hz
    _autoCalib.smoothingFactor = 0.05f;
    resetAutoCalibrationState();
}

GestureRead::~GestureRead()
{
    if (_isSampling)
    {
        stopSampling();
    }

    if (_sampleBuffer.samples)
    {
        delete[] _sampleBuffer.samples;
        _sampleBuffer.samples = nullptr;
    }
}

bool GestureRead::begin(const AccelerometerConfig &config)
{
    _config = config;
    _configLoaded = true;

    if (!_sensor)
    {
        _sensor.reset(new MotionSensor(_wire));
    }

    String normalizedType = _config.type;
    normalizedType.toLowerCase();
    _expectGyro = (normalizedType == "mpu6050");

    _sampleHZ = MotionSensor::clampSampleRate(_config.sampleRate > 0 ? _config.sampleRate : MotionSensor::kDefaultSampleHz);
    _sampleBuffer.sampleHZ = _sampleHZ;

    const uint16_t defaultStableSamples = std::max<uint16_t>(static_cast<uint16_t>(_sampleHZ / 6), static_cast<uint16_t>(5));
    const uint16_t stableSamples = _config.autoCalibrateStableSamples > 0 ? _config.autoCalibrateStableSamples : defaultStableSamples;
    const float gyroThreshold = _config.autoCalibrateGyroThreshold > 0.0f ? _config.autoCalibrateGyroThreshold : _autoCalib.gyroStillThreshold;
    const float smoothing = (_config.autoCalibrateSmoothing > 0.0f && _config.autoCalibrateSmoothing <= 1.0f) ? _config.autoCalibrateSmoothing : _autoCalib.smoothingFactor;
    setAutoCalibrationParameters(gyroThreshold, stableSamples, smoothing);
    setAutoCalibrationEnabled(_config.autoCalibrateEnabled);

    float desiredSeconds = 3.0f;
    uint16_t desiredSamples = static_cast<uint16_t>(_sampleHZ * desiredSeconds);
    desiredSamples = std::max<uint16_t>(desiredSamples, 200);

    if (desiredSamples != _maxSamples)
    {
        if (_sampleBuffer.samples)
        {
            delete[] _sampleBuffer.samples;
        }
        _sampleBuffer.samples = new Sample[desiredSamples];
        if (!_sampleBuffer.samples)
        {
            Logger::getInstance().log("FATAL: Failed to allocate new sample buffer!");
            return false;
        }
        _maxSamples = desiredSamples;
        _sampleBuffer.maxSamples = _maxSamples;
        clearMemory();
    }

    if (!_sensor->begin(_config))
    {
        return false;
    }

    _config = _sensor->config(); // sync resolved address

    _motionWakeEnabled = false;
    if (_config.motionWakeEnabled)
    {
        uint8_t threshold = _config.motionWakeThreshold > 0 ? _config.motionWakeThreshold : 1;
        uint8_t duration = _config.motionWakeDuration > 0 ? _config.motionWakeDuration : 1;
        uint8_t highPass = _config.motionWakeHighPass;
        uint8_t cycle = _config.motionWakeCycleRate;

        if (_expectGyro && _config.motionWakeThreshold < 5 && _config.motionWakeDuration < 5 && normalizedType == "mpu6050")
        {
            Logger::getInstance().log("Motion wake threshold/duration adjusted for clone-safe defaults.");
            threshold = std::max<uint8_t>(threshold, static_cast<uint8_t>(5));
            duration = std::max<uint8_t>(duration, static_cast<uint8_t>(5));
        }

        if (!_sensor->configureMotionWakeup(threshold, duration, highPass, cycle))
        {
            Logger::getInstance().log("Motion wakeup not supported by accelerometer driver");
        }
        else
        {
            Logger::getInstance().log("Motion wakeup armed (thr=" + String(threshold) + ", dur=" + String(duration) + ")");
        }
    }
    else
    {
        _sensor->disableMotionWakeup();
    }

    return standby();
}

void GestureRead::clearMemory()
{
    std::lock_guard<std::mutex> lock(_bufferMutex);

    if (_sampleBuffer.samples)
    {
        memset(_sampleBuffer.samples, 0, _maxSamples * sizeof(Sample));
        _sampleBuffer.sampleCount = 0;
        _bufferFull = false;
    }
}

bool GestureRead::calibrate(uint16_t calibrationSamples)
{
    if (!_sensor || !_sensor->isReady())
    {
        return false;
    }

    if (calibrationSamples == 0)
    {
        calibrationSamples = 2;
    }

    if (!wakeup())
    {
        return false;
    }

    if (!disableLowPowerMode())
    {
        return false;
    }

    float sumX = 0;
    float sumY = 0;
    float sumZ = 0;

    for (uint16_t i = 0; i < calibrationSamples; i++)
    {
        if (_sensor->update())
        {
            sumX += getMappedX();
            sumY += getMappedY();
            sumZ += getMappedZ();
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    _calibrationOffset.x = sumX / calibrationSamples;
    _calibrationOffset.y = sumY / calibrationSamples;
    _calibrationOffset.z = sumZ / calibrationSamples;

    _isCalibrated = true;

    return standby();
}

bool GestureRead::startSampling()
{
    if (_isSampling || !_sensor || !_sensor->isReady())
    {
        return false;
    }

    if (!wakeup())
    {
        return false;
    }

    if (!disableLowPowerMode())
    {
        return false;
    }

    if (!waitForGyroReady(kGyroReadyTimeoutMs))
    {
        Logger::getInstance().log("Aborting sampling start: gyro not ready.");
        standby();
        return false;
    }

    resetAutoCalibrationState();
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
        return false;
    }

    _isSampling = false;
    resetAutoCalibrationState();

    const bool bufferWasFull = _sampleBuffer.sampleCount >= _maxSamples;
    if (bufferWasFull)
    {
        Logger::getInstance().log("Stopped sampling - buffer full (" + String(_maxSamples) + " samples collected)");
    }

    if (!standby())
    {
        Logger::getInstance().log("Failed to enter accelerometer standby after sampling stop");
        return false;
    }

    return true;
}

SampleBuffer &GestureRead::getCollectedSamples()
{
    std::lock_guard<std::mutex> lock(_bufferMutex);
    return _sampleBuffer;
}

bool GestureRead::enableLowPowerMode()
{
    if (!_sensor || !_sensor->isReady())
    {
        return false;
    }
    return _sensor->setSampleRate(MotionSensor::kLowPowerSampleHz, true);
}

bool GestureRead::disableLowPowerMode()
{
    if (!_sensor || !_sensor->isReady())
    {
        return false;
    }
    return _sensor->setSampleRate(_sampleHZ, false);
}

bool GestureRead::standby()
{
    if (!_sensor || !_sensor->isReady())
    {
        return false;
    }

    const bool motionWakeActive = isMotionWakeEnabled();

    if (!enableLowPowerMode())
    {
        Logger::getInstance().log("Failed to configure accelerometer low power mode");
    }

    if (!_sensor->stop())
    {
        return false;
    }

    _motionWakeEnabled = motionWakeActive && _sensor->isMotionWakeConfigured();
    lastSampleTime = 0;
    return true;
}

bool GestureRead::wakeup()
{
    if (!_sensor || !_sensor->isReady())
    {
        return false;
    }
    return _sensor->start();
}

bool GestureRead::configureMotionWakeup(uint8_t threshold, uint8_t duration, uint8_t highPassCode, uint8_t cycleRateCode)
{
    if (!_sensor || !_sensor->isReady())
    {
        return false;
    }

    if (!_sensor->configureMotionWakeup(threshold, duration, highPassCode, cycleRateCode))
    {
        return false;
    }

    _motionWakeEnabled = _sensor->isMotionWakeConfigured();
    return _motionWakeEnabled;
}

bool GestureRead::disableMotionWakeup()
{
    if (!_sensor || !_sensor->isReady())
    {
        return false;
    }

    if (!_sensor->disableMotionWakeup())
    {
        return false;
    }

    _motionWakeEnabled = false;
    return true;
}

bool GestureRead::isMotionWakeEnabled() const
{
    return _motionWakeEnabled && _sensor && _sensor->isMotionWakeConfigured();
}

bool GestureRead::clearMotionWakeInterrupt()
{
    if (!_sensor || !_motionWakeEnabled)
    {
        return false;
    }
    return _sensor->clearMotionInterrupt();
}

bool GestureRead::isMotionWakeTriggered()
{
    if (!_sensor || !_motionWakeEnabled)
    {
        return false;
    }
    return _sensor->getMotionInterruptStatus(true);
}

float GestureRead::getMappedX()
{
    if (!_sensor)
    {
        return 0.0f;
    }
    return _sensor->getMappedX();
}

float GestureRead::getMappedY()
{
    if (!_sensor)
    {
        return 0.0f;
    }
    return _sensor->getMappedY();
}

float GestureRead::getMappedZ()
{
    if (!_sensor)
    {
        return 0.0f;
    }
    return _sensor->getMappedZ();
}

void GestureRead::getMappedGyro(float &x, float &y, float &z)
{
    if (!_sensor)
    {
        x = y = z = 0.0f;
        return;
    }
    _sensor->getMappedGyro(x, y, z);
}

void GestureRead::resetAutoCalibrationState()
{
    _autoCalib.stableCount = 0;
}

void GestureRead::updateAutoCalibration(const float rawAccel[3], const float mappedGyro[3], bool gyroValid)
{
    if (!_autoCalib.enabled || !gyroValid)
    {
        _autoCalib.stableCount = 0;
        return;
    }

    const float gyroMagnitudeSq = (mappedGyro[0] * mappedGyro[0]) +
                                  (mappedGyro[1] * mappedGyro[1]) +
                                  (mappedGyro[2] * mappedGyro[2]);
    const float gyroMagnitude = sqrtf(gyroMagnitudeSq);

    if (!isfinite(gyroMagnitude) || gyroMagnitude > _autoCalib.gyroStillThreshold)
    {
        _autoCalib.stableCount = 0;
        return;
    }

    if (_autoCalib.stableCount < 0xFFFF)
    {
        _autoCalib.stableCount++;
    }

    if (_autoCalib.stableCount < _autoCalib.minStableSamples)
    {
        return;
    }

    const float alpha = _autoCalib.smoothingFactor;
    _calibrationOffset.x = (1.0f - alpha) * _calibrationOffset.x + alpha * rawAccel[0];
    _calibrationOffset.y = (1.0f - alpha) * _calibrationOffset.y + alpha * rawAccel[1];
    _calibrationOffset.z = (1.0f - alpha) * _calibrationOffset.z + alpha * rawAccel[2];
}

bool GestureRead::waitForGyroReady(uint32_t timeoutMs)
{
    if (!_sensor || !_sensor->isReady())
    {
        return false;
    }

    if (!_sensor->expectsGyro())
    {
        return true;
    }

    const unsigned long start = millis();
    bool seenGyroData = false;
    uint8_t zeroGyroSamples = 0;

    while (millis() - start < timeoutMs)
    {
        if (_sensor->update() && _sensor->hasGyro())
        {
            seenGyroData = true;

            const float rawX = getMappedX();
            const float rawY = getMappedY();
            const float rawZ = getMappedZ();

            float mappedGyroX = 0.0f;
            float mappedGyroY = 0.0f;
            float mappedGyroZ = 0.0f;
            getMappedGyro(mappedGyroX, mappedGyroY, mappedGyroZ);

            const float accelSum = fabsf(rawX) + fabsf(rawY) + fabsf(rawZ);
            const float gyroSum = fabsf(mappedGyroX) + fabsf(mappedGyroY) + fabsf(mappedGyroZ);

            if (!isfinite(accelSum) || !isfinite(gyroSum))
            {
                zeroGyroSamples = 0;
            }
            else if (accelSum >= kGyroReadyMinAccelSum && gyroSum > kGyroReadyNoiseFloor)
            {
                return true;
            }
            else if (accelSum >= kGyroReadyMinAccelSum)
            {
                if (++zeroGyroSamples >= kGyroReadyZeroTolerance)
                {
                    Logger::getInstance().log("Gyro warmup: readings remain near zero, proceeding after " + String(zeroGyroSamples) + " attempts.");
                    return true;
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(kGyroReadyPollDelayMs));
    }

    if (!seenGyroData)
    {
        Logger::getInstance().log("Gyro failed to report data within " + String(timeoutMs) + " ms");
    }
    else
    {
        Logger::getInstance().log("Gyro data stayed at zero for " + String(timeoutMs) + " ms");
    }

    return false;
}

void GestureRead::updateSampling()
{
    if (!_sensor || !_sensor->isReady())
    {
        return;
    }

    std::lock_guard<std::mutex> lock(_bufferMutex);

    const unsigned long currentTime = millis();
    const unsigned long interval = MotionSensor::sampleIntervalMs(_sampleHZ);

    if (currentTime - lastSampleTime < interval)
    {
        return;
    }

    const bool sampling = _isSampling;
    const uint16_t count = _sampleBuffer.sampleCount;

    if (sampling && count < _maxSamples)
    {
        if (_sensor->update())
        {
            Sample &sample = _sampleBuffer.samples[count];
            const float rawX = getMappedX();
            const float rawY = getMappedY();
            const float rawZ = getMappedZ();

            float mappedGyroX = 0.0f;
            float mappedGyroY = 0.0f;
            float mappedGyroZ = 0.0f;
            const bool gyroAvailable = _sensor->hasGyro();
            if (gyroAvailable)
            {
                getMappedGyro(mappedGyroX, mappedGyroY, mappedGyroZ);
            }

            const float rawAccel[3] = {rawX, rawY, rawZ};
            const float mappedGyro[3] = {mappedGyroX, mappedGyroY, mappedGyroZ};
            updateAutoCalibration(rawAccel, mappedGyro, gyroAvailable);

            sample.x = rawX - _calibrationOffset.x;
            sample.y = rawY - _calibrationOffset.y;
            sample.z = rawZ - _calibrationOffset.z;

            sample.gyroValid = gyroAvailable;
            if (sample.gyroValid)
            {
                sample.gyroX = mappedGyroX;
                sample.gyroY = mappedGyroY;
                sample.gyroZ = mappedGyroZ;
            }
            else
            {
                sample.gyroX = 0.0f;
                sample.gyroY = 0.0f;
                sample.gyroZ = 0.0f;
            }

            sample.temperatureValid = _sensor->hasTemperature();
            sample.temperature = sample.temperatureValid ? _sensor->readTemperatureC() : 0.0f;

            _sampleBuffer.sampleCount = count + 1;
            lastSampleTime = currentTime;

            static uint16_t debugLogEmitted = 0;
            if (count == 0)
            {
                debugLogEmitted = 0;
            }

            if (debugLogEmitted < 5)
            {
                String logMsg = "gesture_sample idx=" + String(count) +
                                " raw=[" + String(rawX, 4) + "," + String(rawY, 4) + "," + String(rawZ, 4) + "]" +
                                " offset=[" + String(_calibrationOffset.x, 4) + "," + String(_calibrationOffset.y, 4) + "," + String(_calibrationOffset.z, 4) + "]" +
                                " accel=[" + String(sample.x, 4) + "," + String(sample.y, 4) + "," + String(sample.z, 4) + "]";

                if (sample.gyroValid)
                {
                    logMsg += " gyro=[" + String(sample.gyroX, 4) + "," + String(sample.gyroY, 4) + "," + String(sample.gyroZ, 4) + "]";
                }
                else
                {
                    logMsg += " gyro=NA";
                }

                if (sample.temperatureValid)
                {
                    logMsg += " temp=" + String(sample.temperature, 2) + "C";
                }

                Logger::getInstance().log(logMsg);
                debugLogEmitted++;
            }

            float x = fabsf(sample.x);
            float y = fabsf(sample.y);
            float z = fabsf(sample.z);

            x = x > 4.0f ? 4.0f : x;
            y = y > 4.0f ? 4.0f : y;
            z = z > 4.0f ? 4.0f : z;

            const int r = static_cast<int>(x * 255.0f / 4.0f);
            const int g = static_cast<int>(y * 255.0f / 4.0f);
            const int b = static_cast<int>(z * 255.0f / 4.0f);

            Led::getInstance().setColor(r, g, b, false);
        }
    }
    else if (sampling && count >= _maxSamples)
    {
        _bufferFull = true;
        Led::getInstance().setColor(true);
        stopSampling();
    }
    else
    {
        Led::getInstance().setColor(true);
        stopSampling();
    }
}

void GestureRead::setAutoCalibrationEnabled(bool enable)
{
    _autoCalib.enabled = enable;
    if (!enable)
    {
        resetAutoCalibrationState();
    }
}

void GestureRead::setAutoCalibrationParameters(float gyroStillThresholdRad, uint16_t minStableSamples, float smoothingFactor)
{
    if (gyroStillThresholdRad > 0.0f)
    {
        _autoCalib.gyroStillThreshold = gyroStillThresholdRad;
    }
    if (minStableSamples > 0)
    {
        _autoCalib.minStableSamples = minStableSamples;
    }
    if (smoothingFactor > 0.0f && smoothingFactor <= 1.0f)
    {
        _autoCalib.smoothingFactor = smoothingFactor;
    }
    resetAutoCalibrationState();
}

bool GestureRead::isAutoCalibrationEnabled() const
{
    return _autoCalib.enabled;
}
