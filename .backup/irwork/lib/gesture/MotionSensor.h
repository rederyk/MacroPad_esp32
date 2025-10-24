#ifndef MOTION_SENSOR_H
#define MOTION_SENSOR_H

#include <Arduino.h>
#include <Wire.h>
#include <memory>

#include "configTypes.h"

class AccelerometerDriver;

class MotionSensor
{
public:
    static constexpr float kGravity = 9.80665f;
    static constexpr uint16_t kDefaultSampleHz = 100;
    static constexpr uint16_t kLowPowerSampleHz = 12;

    explicit MotionSensor(TwoWire *wire = &Wire);
    ~MotionSensor();

    bool begin(const AccelerometerConfig &config);
    bool isReady() const;

    bool start();
    bool stop();

    bool setSampleRate(uint16_t hz, bool lowPower);
    bool setRange(float g);
    uint16_t sampleRateHz() const;

    bool update();

    float getMappedX() const;
    float getMappedY() const;
    float getMappedZ() const;
    void getMappedAcceleration(float &x, float &y, float &z) const;
    void getMappedGyro(float &x, float &y, float &z) const;

    bool hasGyro() const;
    bool hasTemperature() const;
    float readTemperatureC() const;
    bool expectsGyro() const;

    bool configureMotionWakeup(uint8_t threshold, uint8_t duration, uint8_t highPassCode, uint8_t cycleRateCode);
    bool disableMotionWakeup();
    bool isMotionWakeConfigured() const;
    bool clearMotionInterrupt();
    bool getMotionInterruptStatus(bool clear);

    const AccelerometerConfig &config() const;
    const char *driverName() const;

    static uint16_t clampSampleRate(uint16_t hz);
    static uint32_t sampleIntervalMs(uint16_t hz);

private:
    void applyAxisMap(const String &axisMap, const String &axisDir);
    float getAxisValue(uint8_t axisIndex) const;
    float getGyroAxisValue(uint8_t axisIndex) const;

    TwoWire *_wire;
    AccelerometerConfig _config;
    bool _configLoaded;
    bool _expectGyro;
    bool _motionWakeEnabled;
    String _axisMap;
    String _axisDir;
    uint16_t _sampleHz;
    std::unique_ptr<AccelerometerDriver> _driver;
};

#endif // MOTION_SENSOR_H
