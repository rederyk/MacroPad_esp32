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
constexpr uint8_t kMinSamplingWindowSamples = 10;       // Aim to collect at least 10 samples per gesture
constexpr uint32_t kMinSamplingWindowFloorMs = 50;      // Never wait less than 50ms
constexpr float kSensorFlushStabilityEpsilon = 0.01f;   // Threshold to treat readings as stable
constexpr uint32_t kSensorFlushTimeoutCeilingMs = 300;  // Upper bound for flush wait
constexpr uint8_t kSensorFlushStableReadsMin = 3;
constexpr uint8_t kSensorFlushStableReadsMax = 6;
constexpr float kMinValidAccelMagnitude = 0.05f;
constexpr float kStaleSampleEpsilon = 0.0025f;

namespace
{
    struct FlushTiming
    {
        uint32_t timeoutMs;
        uint32_t waitMs;
        uint8_t stableReads;
    };

    FlushTiming computeFlushTiming(uint16_t sampleHz)
    {
        const uint32_t intervalMs = std::max<uint32_t>(1, MotionSensor::sampleIntervalMs(sampleHz));
        const uint32_t waitMs = std::max<uint32_t>(1, intervalMs / 2);
        const uint32_t timeoutMs = std::min<uint32_t>(
            kSensorFlushTimeoutCeilingMs,
            std::max<uint32_t>(intervalMs * 3, waitMs * 4));
        const uint32_t stableTarget = std::max<uint32_t>(
            kSensorFlushStableReadsMin,
            std::min<uint32_t>(kSensorFlushStableReadsMax, timeoutMs / intervalMs));

        return {timeoutMs, waitMs, static_cast<uint8_t>(stableTarget)};
    }

    uint32_t computeMinimumSamplingDurationMs(uint16_t sampleHz)
    {
        const uint32_t intervalMs = std::max<uint32_t>(1, MotionSensor::sampleIntervalMs(sampleHz));
        return std::max<uint32_t>(kMinSamplingWindowFloorMs, intervalMs * kMinSamplingWindowSamples);
    }
} // namespace

GestureRead::GestureRead(TwoWire *wire)
    : _sensor(new MotionSensor(wire)),
      _configLoaded(false),
      _wire(wire),
      _isCalibrated(false),
      _isSampling(false),
      _bufferFull(false),
      lastSampleTime(0),
      samplingStartTime(0),
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

    // Auto-calibration disabled - using manual calibration only

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

    Logger::getInstance().log("Auto-calibration is DISABLED. Use manual calibration command.");

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

        const bool wakeConfigured = _sensor->configureMotionWakeup(threshold, duration, highPass, cycle);
        _motionWakeEnabled = wakeConfigured;

        if (!wakeConfigured)
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
        _motionWakeEnabled = false;
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
        lastSampleTime = 0;
    }
}

void GestureRead::flushSensorBuffer()
{
    if (!_sensor || !_sensor->isReady())
    {
        return;
    }

    if (isSampling())
    {
        // Avoid touching the hardware FIFO while a capture is in progress
        return;
    }

    // Temporarily wake the sensor so we can drain any residual samples that
    // may still be buffered by the device after the previous capture.
    if (!wakeup())
    {
        Logger::getInstance().log("GestureRead: failed to wake sensor for flush");
        return;
    }

    if (!disableLowPowerMode())
    {
        Logger::getInstance().log("GestureRead: failed to disable low power mode for flush");
        standby();
        return;
    }

    const FlushTiming timing = computeFlushTiming(_sampleHZ);
    drainSensorBuffer(timing.timeoutMs, timing.waitMs, timing.stableReads);

    // Ensure the software buffer starts clean as well
    clearMemory();

    if (!standby())
    {
        Logger::getInstance().log("GestureRead: failed to return sensor to standby after flush");
    }
}

void GestureRead::drainSensorBuffer(uint32_t timeoutMs, uint32_t waitMs, uint8_t stableReads)
{
    if (!_sensor || !_sensor->isReady())
    {
        return;
    }

    const uint32_t deadline = millis() + timeoutMs;
    TickType_t waitTicks = pdMS_TO_TICKS(waitMs);
    if (waitTicks == 0)
    {
        waitTicks = 1;
    }

    float prevX = 0.0f;
    float prevY = 0.0f;
    float prevZ = 0.0f;
    bool havePrev = false;
    uint8_t stableCount = 0;

    while (millis() < deadline)
    {
        if (!_sensor->update())
        {
            vTaskDelay(waitTicks);
            continue;
        }

        const float x = getMappedX();
        const float y = getMappedY();
        const float z = getMappedZ();

        if (havePrev)
        {
            const bool stable =
                fabsf(x - prevX) < kSensorFlushStabilityEpsilon &&
                fabsf(y - prevY) < kSensorFlushStabilityEpsilon &&
                fabsf(z - prevZ) < kSensorFlushStabilityEpsilon;

            if (stable)
            {
                if (++stableCount >= stableReads)
                {
                    break;
                }
            }
            else
            {
                stableCount = 0;
            }
        }
        else
        {
            havePrev = true;
        }

        prevX = x;
        prevY = y;
        prevZ = z;

        vTaskDelay(waitTicks);
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
        calibrationSamples = 10;  // Increased from 2 to 10
    }

    if (!wakeup())
    {
        return false;
    }

    if (!disableLowPowerMode())
    {
        return false;
    }

    Logger::getInstance().log("=== CALIBRATION STARTED ===");
    Logger::getInstance().log("IMPORTANT: Position the device as you normally use it");
    Logger::getInstance().log("  (e.g., flat on desk, or vertical/tilted if that's your normal usage)");
    Logger::getInstance().log("Keep the device VERY STILL");
    Logger::getInstance().log("Collecting " + String(calibrationSamples) + " samples...");

    float sumX = 0;
    float sumY = 0;
    float sumZ = 0;
    float sumGyroX = 0;
    float sumGyroY = 0;
    float sumGyroZ = 0;
    uint16_t validGyroSamples = 0;

    // Wait for sensor to stabilize
    vTaskDelay(pdMS_TO_TICKS(50));

    uint16_t validSamples = 0;

    for (uint16_t i = 0; i < calibrationSamples; i++)
    {
        // Wait for new data to be ready
        vTaskDelay(pdMS_TO_TICKS(15));  // At 100Hz, samples come every 10ms

        if (_sensor->update())
        {
            // Get sensor values (already oriented correctly by driver)
            float accelX = getMappedX();
            float accelY = getMappedY();
            float accelZ = getMappedZ();

            // Check if data is valid (not zero)
            float accelMag = sqrtf(accelX*accelX + accelY*accelY + accelZ*accelZ);
            if (accelMag < 0.1f) {
                Logger::getInstance().log("Sample " + String(i) + ": INVALID (magnitude=" + String(accelMag, 4) + "g) - SKIPPING");
                continue;  // Skip invalid samples
            }

            sumX += accelX;
            sumY += accelY;
            sumZ += accelZ;

            // Collect gyro data for diagnostics
            if (_sensor->hasGyro())
            {
                float gyroX, gyroY, gyroZ;
                getMappedGyro(gyroX, gyroY, gyroZ);
                sumGyroX += gyroX;
                sumGyroY += gyroY;
                sumGyroZ += gyroZ;
                validGyroSamples++;
            }

            // Log ALL samples for detailed diagnostic
            Logger::getInstance().log("Sample " + String(validSamples) +
                ": accel=[" + String(accelX, 4) + "," + String(accelY, 4) + "," + String(accelZ, 4) +
                "] mag=" + String(accelMag, 4) + "g");

            validSamples++;
        }
        else
        {
            Logger::getInstance().log("Sample " + String(i) + ": sensor.update() FAILED");
        }
    }

    if (validSamples == 0) {
        Logger::getInstance().log("ERROR: No valid calibration samples collected!");
        return false;
    }

    Logger::getInstance().log("Collected " + String(validSamples) + "/" + String(calibrationSamples) + " valid samples");

    _calibrationOffset.x = sumX / validSamples;
    _calibrationOffset.y = sumY / validSamples;
    _calibrationOffset.z = sumZ / validSamples;

    _isCalibrated = true;

    // Diagnostic logs
    Logger::getInstance().log("=== CALIBRATION COMPLETE ===");
    Logger::getInstance().log("Calibration offset: [" + String(_calibrationOffset.x, 4) + "," +
                             String(_calibrationOffset.y, 4) + "," + String(_calibrationOffset.z, 4) + "]");

    // Calculate magnitude to verify sensor is working correctly
    float magnitude = sqrtf(_calibrationOffset.x * _calibrationOffset.x +
                           _calibrationOffset.y * _calibrationOffset.y +
                           _calibrationOffset.z * _calibrationOffset.z);
    Logger::getInstance().log("Magnitude: " + String(magnitude, 4) + "g (should be ~1.0g)");

    if (magnitude < 0.8f || magnitude > 1.2f) {
        Logger::getInstance().log("WARNING: Magnitude outside expected range!");
        Logger::getInstance().log("  - Check sensor orientation and mounting");
        Logger::getInstance().log("  - For ADXL345: verify axisMap/axisDir in config.json");
        Logger::getInstance().log("  - For MPU6050: sensor fusion will auto-correct orientation");
    }

    Logger::getInstance().log("Sensor type: " + String(_sensor->driverName()));
    if (_sensor->expectsGyro()) {
        Logger::getInstance().log("NOTE: MPU6050 uses gyroscope for orientation - axis mapping ignored");
    } else {
        Logger::getInstance().log("Current axisMap: \"" + _sensor->config().axisMap + "\"");
        Logger::getInstance().log("Current axisDir: \"" + _sensor->config().axisDir + "\"");
    }
    Logger::getInstance().log("================================");

    if (validGyroSamples > 0)
    {
        float avgGyroX = sumGyroX / validGyroSamples;
        float avgGyroY = sumGyroY / validGyroSamples;
        float avgGyroZ = sumGyroZ / validGyroSamples;
        float gyroMag = sqrtf(avgGyroX * avgGyroX + avgGyroY * avgGyroY + avgGyroZ * avgGyroZ);

        Logger::getInstance().log("Gyro average: [" + String(avgGyroX, 4) + "," +
                                 String(avgGyroY, 4) + "," + String(avgGyroZ, 4) + "]");
        Logger::getInstance().log("Gyro magnitude: " + String(gyroMag, 4) + " rad/s (should be ~0.00 if still)");

        if (gyroMag > 0.1f)
        {
            Logger::getInstance().log("WARNING: Device is MOVING during calibration! Keep it STILL.");
        }
    }

    // Log axis breakdown to help identify orientation
    Logger::getInstance().log("Axis breakdown:");
    Logger::getInstance().log("  X: " + String(fabsf(_calibrationOffset.x), 4) + "g " +
                             (fabsf(_calibrationOffset.x) > 0.8f ? "<-- VERTICAL AXIS" : ""));
    Logger::getInstance().log("  Y: " + String(fabsf(_calibrationOffset.y), 4) + "g " +
                             (fabsf(_calibrationOffset.y) > 0.8f ? "<-- VERTICAL AXIS" : ""));
    Logger::getInstance().log("  Z: " + String(fabsf(_calibrationOffset.z), 4) + "g " +
                             (fabsf(_calibrationOffset.z) > 0.8f ? "<-- VERTICAL AXIS" : ""));

    return standby();
}

bool GestureRead::startSampling()
{
    {
        std::lock_guard<std::mutex> lock(_bufferMutex);
        if (_isSampling)
        {
            return false;
        }
    }

    if (!_sensor || !_sensor->isReady())
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

    // Drain any stale samples that may still be buffered by the hardware
    const FlushTiming timing = computeFlushTiming(_sampleHZ);
    drainSensorBuffer(timing.timeoutMs, timing.waitMs, timing.stableReads);

    bool sensorReady = false;
    if (_expectGyro)
    {
        sensorReady = waitForGyroReady(kGyroReadyTimeoutMs);
    }
    else
    {
        sensorReady = waitForFreshAccelerometer(kGyroReadyTimeoutMs);
    }

    if (!sensorReady)
    {
        Logger::getInstance().log("GestureRead: proceeding without confirmed fresh sample (warmup timeout)");
    }

    // Prime driver with the most recent frame so the first stored sample is current
    _sensor->update();

    clearMemory();

    {
        std::lock_guard<std::mutex> lock(_bufferMutex);
        _isSampling = true;
        _bufferFull = false;
    }

    samplingStartTime = millis(); // Record when sampling started
    return true;
}

bool GestureRead::isSampling()
{
    std::lock_guard<std::mutex> lock(_bufferMutex);
    return _isSampling;
}

void GestureRead::ensureMinimumSamplingTime()
{
    // Lock the buffer to check sampling state atomically
    std::unique_lock<std::mutex> lock(_bufferMutex);

    if (!_isSampling)
    {
        return;
    }

    const unsigned long elapsed = millis() - samplingStartTime;
    const uint32_t minSamplingDuration = computeMinimumSamplingDurationMs(_sampleHZ);

    if (elapsed < minSamplingDuration)
    {
        const uint32_t remainingTime = minSamplingDuration - elapsed;
        Logger::getInstance().log("Waiting " + String(remainingTime) + "ms more for minimum sampling window (" +
                                  String(minSamplingDuration) + "ms target)...");

        const unsigned long waitEnd = millis() + remainingTime;

        // Release lock before delay to allow updateSampling() to continue
        lock.unlock();

        const uint32_t pollMs = std::max<uint32_t>(1, MotionSensor::sampleIntervalMs(_sampleHZ));
        TickType_t pollTicks = pdMS_TO_TICKS(pollMs);
        if (pollTicks == 0)
        {
            pollTicks = 1;
        }

        while (millis() < waitEnd)
        {
            vTaskDelay(pollTicks);
        }
    }
}

bool GestureRead::stopSampling()
{
    std::unique_lock<std::mutex> lock(_bufferMutex);
    if (!_isSampling)
    {
        return false;
    }

    const bool bufferWasFull = _sampleBuffer.sampleCount >= _maxSamples;
    _isSampling = false;
    lock.unlock();

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

bool GestureRead::waitForFreshAccelerometer(uint32_t timeoutMs)
{
    if (!_sensor || !_sensor->isReady())
    {
        return false;
    }

    const unsigned long start = millis();

    while (millis() - start < timeoutMs)
    {
        if (_sensor->update())
        {
            const float rawX = getMappedX();
            const float rawY = getMappedY();
            const float rawZ = getMappedZ();

            if (std::isfinite(rawX) && std::isfinite(rawY) && std::isfinite(rawZ))
            {
                const float accelSum = fabsf(rawX) + fabsf(rawY) + fabsf(rawZ);
                if (accelSum > kMinValidAccelMagnitude)
                {
                    return true;
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(kGyroReadyPollDelayMs));
    }

    Logger::getInstance().log("GestureRead: accelerometer failed to produce fresh data within " + String(timeoutMs) + " ms");
    return false;
}

void GestureRead::updateSampling()
{
    if (!_sensor || !_sensor->isReady())
    {
        return;
    }

    bool requestStop = false;
    bool ledSetIdle = false;
    bool ledSetFull = false;

    std::unique_lock<std::mutex> lock(_bufferMutex);
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
        // Try to update sensor - this may fail if no new data is ready yet
        if (_sensor->update())
        {
            const float mappedX = getMappedX();
            const float mappedY = getMappedY();
            const float mappedZ = getMappedZ();

            if (!std::isfinite(mappedX) || !std::isfinite(mappedY) || !std::isfinite(mappedZ))
            {
                return;
            }

            const float accelSum = fabsf(mappedX) + fabsf(mappedY) + fabsf(mappedZ);
            if (accelSum < kMinValidAccelMagnitude)
            {
                // Skip clearly stale readings (all zeros)
                return;
            }

            const float calibratedX = mappedX - _calibrationOffset.x;
            const float calibratedY = mappedY - _calibrationOffset.y;
            const float calibratedZ = mappedZ - _calibrationOffset.z;

            float mappedGyroX = 0.0f;
            float mappedGyroY = 0.0f;
            float mappedGyroZ = 0.0f;
            const bool gyroAvailable = _sensor->hasGyro();
            if (gyroAvailable)
            {
                getMappedGyro(mappedGyroX, mappedGyroY, mappedGyroZ);
            }

            if (count > 0)
            {
                const Sample &prev = _sampleBuffer.samples[count - 1];
                const bool accelDuplicate =
                    fabsf(prev.x - calibratedX) < kStaleSampleEpsilon &&
                    fabsf(prev.y - calibratedY) < kStaleSampleEpsilon &&
                    fabsf(prev.z - calibratedZ) < kStaleSampleEpsilon;

                bool gyroDuplicate = true;
                if (gyroAvailable && prev.gyroValid)
                {
                    gyroDuplicate =
                        fabsf(prev.gyroX - mappedGyroX) < kStaleSampleEpsilon &&
                        fabsf(prev.gyroY - mappedGyroY) < kStaleSampleEpsilon &&
                        fabsf(prev.gyroZ - mappedGyroZ) < kStaleSampleEpsilon;
                }

                if (accelDuplicate && gyroDuplicate)
                {
                    return;
                }
            }

            Sample &sample = _sampleBuffer.samples[count];

            // Apply calibration offset (set during manual calibration)
            sample.x = calibratedX;
            sample.y = calibratedY;
            sample.z = calibratedZ;

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
            // Only update lastSampleTime when we successfully got new data
            lastSampleTime = currentTime;

            static uint16_t debugLogEmitted = 0;
            if (count == 0)
            {
                debugLogEmitted = 0;
            }

            if (debugLogEmitted < 5)
            {
                String logMsg = "gesture_sample idx=" + String(count) +
                                " mapped=[" + String(mappedX, 4) + "," + String(mappedY, 4) + "," + String(mappedZ, 4) + "]" +
                                " offset=[" + String(_calibrationOffset.x, 4) + "," + String(_calibrationOffset.y, 4) + "," + String(_calibrationOffset.z, 4) + "]" +
                                " calibrated=[" + String(sample.x, 4) + "," + String(sample.y, 4) + "," + String(sample.z, 4) + "]";

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

            const float maxRange = _config.sensitivity;
            x = x > maxRange ? maxRange : x;
            y = y > maxRange ? maxRange : y;
            z = z > maxRange ? maxRange : z;

            const int r = static_cast<int>(x * 255.0f / maxRange);
            const int g = static_cast<int>(y * 255.0f / maxRange);
            const int b = static_cast<int>(z * 255.0f / maxRange);

            Led::getInstance().setColor(r, g, b, false);
        }
    }
    else if (sampling && count >= _maxSamples)
    {
        _bufferFull = true;
        ledSetFull = true;
        requestStop = true;
    }
    else
    {
        if (!sampling)
        {
            ledSetIdle = true;
        }
    }

    lock.unlock();

    if (ledSetFull || ledSetIdle)
    {
        Led::getInstance().setColor(true);
    }

    if (requestStop)
    {
        stopSampling();
    }
}
