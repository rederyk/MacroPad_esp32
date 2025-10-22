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

#include "gestureRead.h"

#include <Arduino.h>
#include <Logger.h>
#include "Led.h"

#include <ADXL345.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

constexpr float kGravity = 9.80665f;
constexpr uint16_t kDefaultSampleHz = 100;
constexpr uint16_t kLowPowerSampleHz = 12;
constexpr uint32_t kGyroReadyTimeoutMs = 300;
constexpr uint32_t kGyroReadyPollDelayMs = 5;
constexpr float kGyroReadyMinAccelSum = 0.05f;
constexpr float kGyroReadyNoiseFloor = 1e-4f;
constexpr uint8_t kGyroReadyZeroTolerance = 5;

constexpr uint8_t MPU6050_REG_SMPLRT_DIV = 0x19;
constexpr uint8_t MPU6050_REG_CONFIG = 0x1A;
constexpr uint8_t MPU6050_REG_ACCEL_CONFIG = 0x1C;
constexpr uint8_t MPU6050_REG_LP_ACCEL_ODR = 0x1E;
constexpr uint8_t MPU6050_REG_MOT_THR = 0x1F;
constexpr uint8_t MPU6050_REG_MOT_DUR = 0x20;
constexpr uint8_t MPU6050_REG_INT_PIN_CFG = 0x37;
constexpr uint8_t MPU6050_REG_INT_ENABLE = 0x38;
constexpr uint8_t MPU6050_REG_INT_STATUS = 0x3A;
constexpr uint8_t MPU6050_REG_PWR_MGMT_1 = 0x6B;
constexpr uint8_t MPU6050_REG_PWR_MGMT_2 = 0x6C;
constexpr uint8_t MPU6050_REG_MOT_DETECT_CTRL = 0x69;
constexpr uint8_t MPU6050_REG_ACCEL_XOUT_H = 0x3B;
constexpr uint8_t MPU6050_INT_MOTION_BIT = 0x40;

class AccelerometerDriver
{
public:
    virtual ~AccelerometerDriver() = default;
    virtual const char *name() const = 0;
    virtual bool begin() = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual bool update() = 0;
    virtual float readX() const = 0;
    virtual float readY() const = 0;
    virtual float readZ() const = 0;
    virtual bool hasGyroscope() const
    {
        return false;
    }
    virtual float readGyroX() const
    {
        return 0.0f;
    }
    virtual float readGyroY() const
    {
        return 0.0f;
    }
    virtual float readGyroZ() const
    {
        return 0.0f;
    }
    virtual bool hasTemperature() const
    {
        return false;
    }
    virtual float readTemperatureC() const
    {
        return 0.0f;
    }
    virtual bool setSampleRate(uint16_t hz, bool lowPower) = 0;
    virtual bool setRange(float g) = 0;
    virtual bool configureMotionWakeup(uint8_t threshold, uint8_t duration, uint8_t highPassCode, uint8_t cycleRateCode)
    {
        (void)threshold;
        (void)duration;
        (void)highPassCode;
        (void)cycleRateCode;
        return false;
    }
    virtual bool disableMotionWakeup()
    {
        return true;
    }
    virtual bool isMotionWakeupConfigured() const
    {
        return false;
    }
    virtual bool clearMotionInterrupt()
    {
        return true;
    }
    virtual bool getMotionInterruptStatus(bool clear)
    {
        (void)clear;
        return false;
    }
};

namespace
{
    uint16_t clampSampleRate(uint16_t hz)
    {
        if (hz == 0)
        {
            return kDefaultSampleHz;
        }
        return std::min<uint16_t>(hz, 1000);
    }

    uint32_t sampleIntervalMs(uint16_t hz)
    {
        hz = clampSampleRate(hz);
        if (hz == 0)
        {
            return 10;
        }
        uint32_t interval = 1000UL / hz;
        return interval == 0 ? 1 : interval;
    }

    bool probeAddress(TwoWire *wire, uint8_t address)
    {
        if (address == 0)
        {
            return false;
        }
        wire->beginTransmission(address);
        return wire->endTransmission() == 0;
    }

    bool readRegister(TwoWire *wire, uint8_t address, uint8_t reg, uint8_t &value)
    {
        if (address == 0)
        {
            return false;
        }

        wire->beginTransmission(address);
        wire->write(reg);
        if (wire->endTransmission(false) != 0)
        {
            return false;
        }

        if (wire->requestFrom(address, static_cast<uint8_t>(1)) != 1)
        {
            return false;
        }

        value = wire->read();
        return true;
    }

    bool writeRegister(TwoWire *wire, uint8_t address, uint8_t reg, uint8_t value)
    {
        if (address == 0)
        {
            return false;
        }
        wire->beginTransmission(address);
        wire->write(reg);
        wire->write(value);
        return wire->endTransmission() == 0;
    }

    bool readRegisters(TwoWire *wire, uint8_t address, uint8_t startReg, uint8_t *buffer, size_t length)
    {
        if (address == 0 || !buffer || length == 0)
        {
            return false;
        }

        wire->beginTransmission(address);
        wire->write(startReg);
        if (wire->endTransmission(false) != 0)
        {
            return false;
        }

        size_t received = wire->requestFrom(address, static_cast<uint8_t>(length));
        if (received != length)
        {
            return false;
        }

        for (size_t i = 0; i < length && wire->available(); ++i)
        {
            buffer[i] = wire->read();
        }
        return true;
    }

    bool modifyRegister(TwoWire *wire, uint8_t address, uint8_t reg, uint8_t mask, uint8_t value)
    {
        uint8_t current = 0;
        if (!readRegister(wire, address, reg, current))
        {
            return false;
        }
        current = (current & ~mask) | (value & mask);
        return writeRegister(wire, address, reg, current);
    }

    struct AccelerometerProbeResult
    {
        uint8_t address = 0;
        uint8_t whoAmI = 0;
        bool isClone = false;
    };

    class ADXL345Driver final : public AccelerometerDriver
    {
    public:
        ADXL345Driver(TwoWire *wire, uint8_t address)
            : _address(address ? address : ADXL345_ALT),
              _sensor(_address, wire)
        {
        }

        const char *name() const override
        {
            return "ADXL345";
        }

        bool begin() override
        {
            if (!_sensor.start())
            {
                return false;
            }
            _sensor.writeRange(ADXL345_RANGE_4G);
            _sensor.writeRate(ADXL345_RATE_100HZ);
            return _sensor.stop();
        }

        bool start() override
        {
            return _sensor.start();
        }

        bool stop() override
        {
            return _sensor.stop();
        }

        bool update() override
        {
            return _sensor.update();
        }

        float readX() const override
        {
            return _sensor.getX();
        }

        float readY() const override
        {
            return _sensor.getY();
        }

        float readZ() const override
        {
            return _sensor.getZ();
        }

        bool setSampleRate(uint16_t hz, bool lowPower) override
        {
            hz = clampSampleRate(hz);
            uint8_t rateCode = ADXL345_RATE_100HZ;

            if (hz >= 1600)
                rateCode = ADXL345_RATE_1600HZ;
            else if (hz >= 800)
                rateCode = ADXL345_RATE_800HZ;
            else if (hz >= 400)
                rateCode = ADXL345_RATE_400HZ;
            else if (hz >= 200)
                rateCode = ADXL345_RATE_200HZ;
            else if (hz >= 100)
                rateCode = ADXL345_RATE_100HZ;
            else if (hz >= 50)
                rateCode = ADXL345_RATE_50HZ;
            else if (hz >= 25)
                rateCode = ADXL345_RATE_25HZ;
            else if (hz >= 12)
                rateCode = ADXL345_RATE_12_5HZ;
            else if (hz >= 6)
                rateCode = ADXL345_RATE_6_25HZ;
            else
                rateCode = ADXL345_RATE_3_13HZ;

            if (lowPower)
            {
                return _sensor.writeRateWithLowPower(rateCode);
            }
            return _sensor.writeRate(rateCode);
        }

        bool setRange(float g) override
        {
            uint8_t rangeCode = ADXL345_RANGE_4G;
            if (g <= 2.0f)
                rangeCode = ADXL345_RANGE_2G;
            else if (g <= 4.0f)
                rangeCode = ADXL345_RANGE_4G;
            else if (g <= 8.0f)
                rangeCode = ADXL345_RANGE_8G;
            else
                rangeCode = ADXL345_RANGE_16G;

            return _sensor.writeRange(rangeCode);
        }

    private:
        uint8_t _address;
        mutable ADXL345 _sensor;
    };

    class MPU6050Driver final : public AccelerometerDriver
    {
    public:
        MPU6050Driver(TwoWire *wire, uint8_t address)
            : _wire(wire),
              _address(address ? address : MPU6050_I2CADDR_DEFAULT),
              _hasGyro(false),
              _hasTemp(false),
              _motionWakeEnabled(false),
              _motionWakeCycleCode(static_cast<uint8_t>(MPU6050_CYCLE_5_HZ))
        {
        }

        const char *name() const override
        {
            return "MPU6050";
        }

    bool begin() override
    {
        for (uint8_t attempt = 0; attempt < 3; ++attempt)
        {
            if (_sensor.begin(_address, _wire))
            {
                _sensor.setFilterBandwidth(MPU6050_BAND_94_HZ);
                _sensor.setAccelerometerRange(MPU6050_RANGE_4_G);
                _sensor.enableCycle(false);
                _sensor.enableSleep(true);
                return true;
            }
            Logger::getInstance().log("MPU6050 Adafruit begin failed on attempt " + String(attempt + 1));
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        return false;
    }

        bool start() override
        {
            _sensor.enableCycle(false);
            return _sensor.enableSleep(false);
        }

        bool stop() override
        {
            if (_motionWakeEnabled)
            {
                _sensor.enableSleep(false);
                return true;
            }
            return _sensor.enableSleep(true);
        }

        bool update() override
        {
            sensors_event_t gyro{};
            sensors_event_t temp{};
            bool ok = _sensor.getEvent(&_accelEvent, &gyro, &temp);
            if (ok)
            {
                _gyroEvent = gyro;
                _tempEvent = temp;
            }
            _hasGyro = ok;
            _hasTemp = ok;
            return ok;
        }

        float readX() const override
        {
            return _accelEvent.acceleration.x / kGravity;
        }

        float readY() const override
        {
            return _accelEvent.acceleration.y / kGravity;
        }

        float readZ() const override
        {
            return _accelEvent.acceleration.z / kGravity;
        }

        bool hasGyroscope() const override
        {
            return _hasGyro;
        }

        float readGyroX() const override
        {
            return _gyroEvent.gyro.x;
        }

        float readGyroY() const override
        {
            return _gyroEvent.gyro.y;
        }

        float readGyroZ() const override
        {
            return _gyroEvent.gyro.z;
        }

        bool hasTemperature() const override
        {
            return _hasTemp;
        }

        float readTemperatureC() const override
        {
            return _tempEvent.temperature;
        }

        bool setSampleRate(uint16_t hz, bool lowPower) override
        {
            hz = clampSampleRate(hz);

            uint16_t effectiveHz = std::max<uint16_t>(5, hz);
            uint8_t divisor = static_cast<uint8_t>(std::max<uint16_t>(1, 1000 / effectiveHz) - 1);
            _sensor.setSampleRateDivisor(divisor);

            if (lowPower)
            {
                _sensor.enableCycle(true);
                if (_motionWakeEnabled)
                {
                    _sensor.setCycleRate(static_cast<mpu6050_cycle_rate_t>(_motionWakeCycleCode));
                }
                else if (hz <= 2)
                {
                    _sensor.setCycleRate(MPU6050_CYCLE_1_25_HZ);
                }
                else if (hz <= 7)
                {
                    _sensor.setCycleRate(MPU6050_CYCLE_5_HZ);
                }
                else if (hz <= 25)
                {
                    _sensor.setCycleRate(MPU6050_CYCLE_20_HZ);
                }
                else
                {
                    _sensor.setCycleRate(MPU6050_CYCLE_40_HZ);
                }
            }
            else
            {
                _sensor.enableCycle(false);
            }

            return true;
        }

        bool setRange(float g) override
        {
            if (g <= 2.0f)
            {
                _sensor.setAccelerometerRange(MPU6050_RANGE_2_G);
            }
            else if (g <= 4.0f)
            {
                _sensor.setAccelerometerRange(MPU6050_RANGE_4_G);
            }
            else if (g <= 8.0f)
            {
                _sensor.setAccelerometerRange(MPU6050_RANGE_8_G);
            }
            else
            {
                _sensor.setAccelerometerRange(MPU6050_RANGE_16_G);
            }
            return true;
        }
        bool configureMotionWakeup(uint8_t threshold, uint8_t duration, uint8_t highPassCode, uint8_t cycleRateCode) override
        {
            if (highPassCode > static_cast<uint8_t>(MPU6050_HIGHPASS_HOLD))
            {
                highPassCode = static_cast<uint8_t>(MPU6050_HIGHPASS_0_63_HZ);
            }

            if (cycleRateCode > static_cast<uint8_t>(MPU6050_CYCLE_40_HZ))
            {
                cycleRateCode = static_cast<uint8_t>(MPU6050_CYCLE_5_HZ);
            }

            mpu6050_highpass_t highPass = static_cast<mpu6050_highpass_t>(highPassCode);
            mpu6050_cycle_rate_t cycleRate = static_cast<mpu6050_cycle_rate_t>(cycleRateCode);

            _sensor.enableCycle(false);
            _sensor.enableSleep(false);
            _sensor.setMotionInterrupt(false);

            _sensor.setHighPassFilter(highPass);
            _sensor.setMotionDetectionThreshold(threshold);
            _sensor.setMotionDetectionDuration(duration);
            _sensor.setInterruptPinPolarity(true);
            _sensor.setInterruptPinLatch(false);
            _sensor.setMotionInterrupt(true);
            _sensor.setCycleRate(cycleRate);
            _sensor.enableCycle(true);
            _sensor.getMotionInterruptStatus(); // clear any latched state

            _motionWakeEnabled = true;
            _motionWakeCycleCode = cycleRateCode;
            return true;
        }

        bool disableMotionWakeup() override
        {
            _sensor.setMotionInterrupt(false);
            _sensor.enableCycle(false);
            _sensor.enableSleep(false);
            _motionWakeEnabled = false;
            return true;
        }

        bool isMotionWakeupConfigured() const override
        {
            return _motionWakeEnabled;
        }

        bool clearMotionInterrupt() override
        {
            _sensor.getMotionInterruptStatus();
            return true;
        }

        bool getMotionInterruptStatus(bool clear) override
        {
            (void)clear;
            return _sensor.getMotionInterruptStatus();
        }

    private:
        TwoWire *_wire;
        uint8_t _address;
        Adafruit_MPU6050 _sensor;
        sensors_event_t _accelEvent{};
        sensors_event_t _gyroEvent{};
        sensors_event_t _tempEvent{};
        bool _hasGyro;
        bool _hasTemp;
        bool _motionWakeEnabled;
        uint8_t _motionWakeCycleCode;
    };

    class MPU6050CloneDriver final : public AccelerometerDriver
    {
    public:
        MPU6050CloneDriver(TwoWire *wire, uint8_t address)
            : _wire(wire),
              _address(address),
              _scale(16384.0f),
              _sampleRate(kDefaultSampleHz),
              _accelX(0.0f),
              _accelY(0.0f),
              _accelZ(0.0f),
              _gyroX(0.0f),
              _gyroY(0.0f),
              _gyroZ(0.0f),
              _temperatureC(0.0f),
              _hasGyro(false),
              _hasTemperature(false),
              _motionWakeEnabled(false),
              _motionWakeThreshold(0),
              _motionWakeDuration(0),
              _motionWakeCycleCode(static_cast<uint8_t>(MPU6050_CYCLE_5_HZ)),
              _motionWakeHighPassCode(static_cast<uint8_t>(MPU6050_HIGHPASS_0_63_HZ))
        {
        }

        const char *name() const override
        {
            return "MPU6050-Clone";
        }

        bool begin() override
        {
            if (!resetDevice())
            {
                return false;
            }

            if (!writeRegister(_wire, _address, MPU6050_REG_CONFIG, 0x02)) // 94 Hz bandwidth
            {
                return false;
            }

            if (!setRange(4.0f))
            {
                return false;
            }

            if (!setSampleRate(kDefaultSampleHz, false))
            {
                return false;
            }

            return stop(); // enter standby by default
        }

        bool start() override
        {
            if (!modifyRegister(_wire, _address, MPU6050_REG_PWR_MGMT_1, 0x40, 0x00))
            {
                return false;
            }
            // Ensure all axes are enabled when entering active mode
            if (!writeRegister(_wire, _address, MPU6050_REG_PWR_MGMT_2, 0x00))
            {
                Logger::getInstance().log("MPU6050-Clone start: failed to enable gyro/accel axes");
                return false;
            }
            return true;
        }

        bool stop() override
        {
            if (_motionWakeEnabled)
            {
                return modifyRegister(_wire, _address, MPU6050_REG_PWR_MGMT_1, 0x40, 0x00);
            }
            return modifyRegister(_wire, _address, MPU6050_REG_PWR_MGMT_1, 0x40, 0x40);
        }

        bool update() override
        {
            uint8_t data[14] = {0};
            if (!readRegisters(_wire, _address, MPU6050_REG_ACCEL_XOUT_H, data, sizeof(data)))
            {
                return false;
            }

            int16_t rawX = static_cast<int16_t>((data[0] << 8) | data[1]);
            int16_t rawY = static_cast<int16_t>((data[2] << 8) | data[3]);
            int16_t rawZ = static_cast<int16_t>((data[4] << 8) | data[5]);
            int16_t rawTemp = static_cast<int16_t>((data[6] << 8) | data[7]);
            int16_t rawGyroX = static_cast<int16_t>((data[8] << 8) | data[9]);
            int16_t rawGyroY = static_cast<int16_t>((data[10] << 8) | data[11]);
            int16_t rawGyroZ = static_cast<int16_t>((data[12] << 8) | data[13]);

            _accelX = static_cast<float>(rawX) / _scale;
            _accelY = static_cast<float>(rawY) / _scale;
            _accelZ = static_cast<float>(rawZ) / _scale;

            constexpr float gyroScale = 131.0f; // LSB/deg/s at +/-250 deg/s
            _gyroX = (static_cast<float>(rawGyroX) / gyroScale) * DEG_TO_RAD;
            _gyroY = (static_cast<float>(rawGyroY) / gyroScale) * DEG_TO_RAD;
            _gyroZ = (static_cast<float>(rawGyroZ) / gyroScale) * DEG_TO_RAD;
            _hasGyro = true;

            _temperatureC = (static_cast<float>(rawTemp) / 340.0f) + 36.53f;
            _hasTemperature = true;

            return true;
        }

        float readX() const override
        {
            return _accelX;
        }

        float readY() const override
        {
            return _accelY;
        }

        float readZ() const override
        {
            return _accelZ;
        }

        bool hasGyroscope() const override
        {
            return _hasGyro;
        }

        float readGyroX() const override
        {
            return _gyroX;
        }

        float readGyroY() const override
        {

            return _gyroY;

        }

        float readGyroZ() const override
        {
            return _gyroZ;
        }

        bool hasTemperature() const override
        {
            return _hasTemperature;
        }

        float readTemperatureC() const override
        {
            return _temperatureC;
        }

        bool setSampleRate(uint16_t hz, bool lowPower) override
        {
            hz = clampSampleRate(hz);
            _sampleRate = hz;

            uint16_t effectiveHz = std::max<uint16_t>(5, hz);
            uint16_t divisorBase = 1000;
            uint8_t divisor = static_cast<uint8_t>(std::max<uint16_t>(1, divisorBase / effectiveHz) - 1);

            if (!writeRegister(_wire, _address, MPU6050_REG_SMPLRT_DIV, divisor))
            {
                return false;
            }

            uint8_t cycleValue = lowPower ? 0x20 : 0x00;
            return modifyRegister(_wire, _address, MPU6050_REG_PWR_MGMT_1, 0x20, cycleValue);
        }

        bool setRange(float g) override
        {
            uint8_t configValue = 0x00;
            float scale = 16384.0f; // 2g

            if (g <= 2.0f)
            {
                configValue = 0x00;
                scale = 16384.0f;
            }
            else if (g <= 4.0f)
            {
                configValue = 0x08;
                scale = 8192.0f;
            }
            else if (g <= 8.0f)
            {
                configValue = 0x10;
                scale = 4096.0f;
            }
            else
            {
                configValue = 0x18;
                scale = 2048.0f;
            }

            if (!writeRegister(_wire, _address, MPU6050_REG_ACCEL_CONFIG, configValue))
            {
                return false;
            }

            _scale = scale;
            return true;
        }

        bool configureMotionWakeup(uint8_t threshold, uint8_t duration, uint8_t highPassCode, uint8_t cycleRateCode) override
        {
            (void)cycleRateCode; // clone driver ignores cycle rate hints

            _motionWakeEnabled = false;
            threshold = std::max<uint8_t>(threshold, static_cast<uint8_t>(5));
            duration = std::max<uint8_t>(duration, static_cast<uint8_t>(5));

            highPassCode &= 0x07;
            _motionWakeThreshold = threshold;
            _motionWakeDuration = duration;
            _motionWakeCycleCode = cycleRateCode & 0x0F;
            _motionWakeHighPassCode = highPassCode;

            if (!modifyRegister(_wire, _address, MPU6050_REG_PWR_MGMT_1, 0x40, 0x00)) // ensure awake
            {
                return false;
            }

            if (!modifyRegister(_wire, _address, MPU6050_REG_PWR_MGMT_1, 0x20, 0x00)) // disable cycle during config
            {
                return false;
            }

            if (!modifyRegister(_wire, _address, MPU6050_REG_ACCEL_CONFIG, 0x07, highPassCode))
            {
                return false;
            }

            if (!writeRegister(_wire, _address, MPU6050_REG_MOT_THR, threshold))
            {
                return false;
            }

            if (!writeRegister(_wire, _address, MPU6050_REG_MOT_DUR, duration))
            {
                return false;
            }

            if (!modifyRegister(_wire, _address, MPU6050_REG_INT_PIN_CFG, 0x80, 0x80)) // INT active low
            {
                return false;
            }

            if (!modifyRegister(_wire, _address, MPU6050_REG_INT_PIN_CFG, 0x40, 0x40)) // open drain
            {
                return false;
            }

            if (!modifyRegister(_wire, _address, MPU6050_REG_INT_PIN_CFG, 0x20, 0x20)) // latch until cleared
            {
                return false;
            }

            if (!modifyRegister(_wire, _address, MPU6050_REG_INT_ENABLE, MPU6050_INT_MOTION_BIT, MPU6050_INT_MOTION_BIT))
            {
                return false;
            }

            if (!writeRegister(_wire, _address, MPU6050_REG_MOT_DETECT_CTRL, 0x15))
            {
                return false;
            }

            if (!writeRegister(_wire, _address, MPU6050_REG_PWR_MGMT_2, 0x07)) // disable gyros
            {
                return false;
            }

            uint8_t lpRate = cycleRateCode & 0x0F;
            if (!writeRegister(_wire, _address, MPU6050_REG_LP_ACCEL_ODR, lpRate))
            {
                return false;
            }

            if (!modifyRegister(_wire, _address, MPU6050_REG_PWR_MGMT_1, 0x20, 0x20)) // enable cycle
            {
                return false;
            }

            // Clear any pending interrupt
            uint8_t status = 0;
            readRegister(_wire, _address, MPU6050_REG_INT_STATUS, status);
            vTaskDelay(pdMS_TO_TICKS(20));

            _motionWakeEnabled = true;
            return true;
        }

        bool disableMotionWakeup() override
        {
            if (!modifyRegister(_wire, _address, MPU6050_REG_INT_ENABLE, MPU6050_INT_MOTION_BIT, 0x00))
            {
                return false;
            }
            if (!modifyRegister(_wire, _address, MPU6050_REG_PWR_MGMT_1, 0x20, 0x00))
            {
                return false;
            }
            if (!writeRegister(_wire, _address, MPU6050_REG_MOT_DETECT_CTRL, 0x00))
            {
                return false;
            }
            if (!writeRegister(_wire, _address, MPU6050_REG_PWR_MGMT_2, 0x00))
            {
                return false;
            }
            if (!modifyRegister(_wire, _address, MPU6050_REG_INT_PIN_CFG, 0xE0, 0x00))
            {
                return false;
            }
            _motionWakeEnabled = false;
            return true;
        }

        bool isMotionWakeupConfigured() const override
        {
            return _motionWakeEnabled;
        }

        bool clearMotionInterrupt() override
        {
            uint8_t status = 0;
            return readRegister(_wire, _address, MPU6050_REG_INT_STATUS, status);
        }

        bool getMotionInterruptStatus(bool clear) override
        {
            (void)clear;
            uint8_t status = 0;
            if (!readRegister(_wire, _address, MPU6050_REG_INT_STATUS, status))
            {
                return false;
            }
            return (status & MPU6050_INT_MOTION_BIT) != 0;
        }

    private:
        bool resetDevice()
        {
            if (!writeRegister(_wire, _address, MPU6050_REG_PWR_MGMT_1, 0x80)) // reset
            {
                return false;
            }
            vTaskDelay(pdMS_TO_TICKS(50));

            if (!writeRegister(_wire, _address, MPU6050_REG_PWR_MGMT_1, 0x01)) // clock source PLL
            {
                return false;
            }
            vTaskDelay(pdMS_TO_TICKS(30));
            return true;
        }

        TwoWire *_wire;
        uint8_t _address;
        float _scale;
        uint16_t _sampleRate;
        float _accelX;
        float _accelY;
        float _accelZ;
        float _gyroX;
        float _gyroY;
        float _gyroZ;
        float _temperatureC;
        bool _hasGyro;
        bool _hasTemperature;
        bool _motionWakeEnabled;
        uint8_t _motionWakeThreshold;
        uint8_t _motionWakeDuration;
        uint8_t _motionWakeCycleCode;
        uint8_t _motionWakeHighPassCode;
    };

    std::unique_ptr<AccelerometerDriver> createDriver(const AccelerometerConfig &config, TwoWire *wire, bool useCloneDriver)
    {
        String normalizedType = config.type;
        normalizedType.toLowerCase();

        if (normalizedType == "mpu6050")
        {
            if (useCloneDriver)
            {
                return std::unique_ptr<AccelerometerDriver>(new MPU6050CloneDriver(wire, config.address));
            }
            return std::unique_ptr<AccelerometerDriver>(new MPU6050Driver(wire, config.address));
        }

        return std::unique_ptr<AccelerometerDriver>(new ADXL345Driver(wire, config.address));
    }

    AccelerometerProbeResult resolveAccelerometerAddress(const AccelerometerConfig &config, TwoWire *wire)
    {
        String normalizedType = config.type;
        normalizedType.toLowerCase();

        std::vector<uint8_t> candidates;
        candidates.reserve(4);

        auto addCandidate = [&candidates](uint8_t addr) {
            if (addr == 0)
                return;
            for (uint8_t existing : candidates)
            {
                if (existing == addr)
                    return;
            }
            candidates.push_back(addr);
        };

        if (config.address)
        {
            addCandidate(config.address);
        }

        if (normalizedType == "mpu6050")
        {
            addCandidate(MPU6050_I2CADDR_DEFAULT);
            addCandidate(MPU6050_I2CADDR_DEFAULT + 1); // 0x69
        }
        else
        {
            addCandidate(ADXL345_ALT);
            addCandidate(ADXL345_STD);
        }

        if (candidates.empty())
        {
            Logger::getInstance().log("Accelerometer address list empty, no addresses to probe.");
            return {};
        }

        String probeLog = "I2C probe ";
        probeLog += normalizedType.length() ? normalizedType : String("accelerometer");
        probeLog += " @ ";

        AccelerometerProbeResult result;

        for (size_t i = 0; i < candidates.size(); ++i)
        {
            uint8_t address = candidates[i];
            bool ok = probeAddress(wire, address);
            probeLog += "0x" + String(address, HEX);
            probeLog += ok ? "(ACK)" : "(--)";
            if (i < candidates.size() - 1)
            {
                probeLog += ", ";
            }

            if (ok && result.address == 0)
            {
                result.address = address;
            }
        }

        Logger::getInstance().log(probeLog);

        if (result.address == 0)
        {
            Logger::getInstance().log("No accelerometer responded on I2C bus. Check wiring, power, pullups, and address in config.");
        }
        else
        {
            Logger::getInstance().log("Detected accelerometer at 0x" + String(result.address, HEX));

            uint8_t whoAmI = 0;
            bool whoAmIOk = false;
            String whoLog = "WHO_AM_I read failed";

            if (normalizedType == "mpu6050")
            {
                whoAmIOk = readRegister(wire, result.address, 0x75, whoAmI);
                if (whoAmIOk)
                {
                    whoLog = "MPU6050 WHO_AM_I = 0x" + String(whoAmI, HEX);
                    if (whoAmI == 0x70)
                    {
                        whoLog += " (MPU clone detected)";
                    }
                    else if (whoAmI != 0x68)
                    {
                        whoLog += " (unexpected, expected 0x68)";
                    }
                }
            }
            else
            {
                whoAmIOk = readRegister(wire, result.address, 0x00, whoAmI);
                if (whoAmIOk)
                {
                    whoLog = "ADXL345 DEVID = 0x" + String(whoAmI, HEX);
                    if (whoAmI != 0xE5)
                    {
                        whoLog += " (unexpected, expected 0xE5)";
                    }
                }
            }

            Logger::getInstance().log(whoLog);

            if (normalizedType == "mpu6050" && whoAmIOk)
            {
                result.whoAmI = whoAmI;
                result.isClone = (whoAmI == 0x70);
            }
        }

        return result;
    }
} // namespace

GestureRead::GestureRead(TwoWire *wire)
    : _driver(nullptr),
      _configLoaded(false),
      _wire(wire),
      _isCalibrated(false),
      _axisMap("xyz"),
      _axisDir("+++"),
      // TODO questa parte e decisamente complessa ,includere le foto dei sonsori supportati e tutte le combinazioni possibili delle loro posizioni o un piccolo scrpit che ruota in 3d un rettangolo con la texture del sensore e indica le axismap e axisdir 
      _isSampling(false),
      _bufferFull(false),
      lastSampleTime(0),
      _motionWakeEnabled(false),
      _expectGyro(false)
{
    _calibrationOffset = {0, 0, 0};
    _sampleHZ = kDefaultSampleHz;
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
    String normalizedType = _config.type;
    normalizedType.toLowerCase();
    _expectGyro = (normalizedType == "mpu6050");

    applyAxisMap(config.axisMap, config.axisDir);

    _sampleHZ = clampSampleRate(config.sampleRate > 0 ? config.sampleRate : kDefaultSampleHz);
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

    AccelerometerProbeResult probeResult = resolveAccelerometerAddress(_config, _wire);
    if (probeResult.address == 0)
    {
        return false;
    }

    _config.address = probeResult.address;

    if (probeResult.isClone)
    {
        Logger::getInstance().log("Using MPU6050 clone driver for WHO_AM_I 0x" + String(probeResult.whoAmI, HEX));
    }

    _driver = createDriver(_config, _wire, probeResult.isClone);
    if (!_driver)
    {
        Logger::getInstance().log("Failed to instantiate accelerometer driver");
        return false;
    }

    if (!_driver->begin())
    {
        Logger::getInstance().log("Failed to initialise accelerometer driver");
        _driver.reset();
        return false;
    }

    float range = config.sensitivity > 0.0f ? config.sensitivity : 4.0f;
    _driver->setRange(range);
    _driver->setSampleRate(_sampleHZ, false);

    Logger::getInstance().log("Accelerometer initialised: " + String(_driver->name()));

    _motionWakeEnabled = false;
    if (_config.motionWakeEnabled)
    {
        uint8_t threshold = _config.motionWakeThreshold > 0 ? _config.motionWakeThreshold : 1;
        uint8_t duration = _config.motionWakeDuration > 0 ? _config.motionWakeDuration : 1;
        uint8_t highPass = _config.motionWakeHighPass <= static_cast<uint8_t>(MPU6050_HIGHPASS_HOLD) ? _config.motionWakeHighPass : static_cast<uint8_t>(MPU6050_HIGHPASS_0_63_HZ);
        uint8_t cycle = _config.motionWakeCycleRate <= static_cast<uint8_t>(MPU6050_CYCLE_40_HZ) ? _config.motionWakeCycleRate : static_cast<uint8_t>(MPU6050_CYCLE_5_HZ);

        if (probeResult.isClone)
        {
            if (threshold < 5)
            {
                Logger::getInstance().log("Motion threshold raised for clone from " + String(threshold) + " to 5 to avoid false wakeups");
                threshold = 5;
            }
            if (duration < 5)
            {
                Logger::getInstance().log("Motion duration raised for clone from " + String(duration) + " to 5 to avoid false wakeups");
                duration = 5;
            }
        }

        if (configureMotionWakeup(threshold, duration, highPass, cycle))
        {
            Logger::getInstance().log("Motion wakeup armed (thr=" + String(threshold) + ", dur=" + String(duration) + ")");
        }
        else
        {
            Logger::getInstance().log("Motion wakeup not supported by accelerometer driver");
        }
    }
    else
    {
        disableMotionWakeup();
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
    if (!_driver)
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
        if (_driver->update())
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
    if (_isSampling || !_driver)
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
    if (!_driver)
    {
        return false;
    }
    return _driver->setSampleRate(kLowPowerSampleHz, true);
}

bool GestureRead::disableLowPowerMode()
{
    if (!_driver)
    {
        return false;
    }
    return _driver->setSampleRate(_sampleHZ, false);
}

bool GestureRead::standby()
{
    if (!_driver)
    {
        return false;
    }

    const bool motionWakeActive = isMotionWakeEnabled();

    if (!enableLowPowerMode())
    {
        Logger::getInstance().log("Failed to configure accelerometer low power mode");
    }

    if (!_driver->stop())
    {
        return false;
    }

    _motionWakeEnabled = motionWakeActive && _driver->isMotionWakeupConfigured();

    lastSampleTime = 0;
    return true;
}

bool GestureRead::wakeup()
{
    if (!_driver)
    {
        return false;
    }
    return _driver->start();
}

bool GestureRead::configureMotionWakeup(uint8_t threshold, uint8_t duration, uint8_t highPassCode, uint8_t cycleRateCode)
{
    if (!_driver)
    {
        return false;
    }

    if (!_driver->configureMotionWakeup(threshold, duration, highPassCode, cycleRateCode))
    {
        return false;
    }

    _motionWakeEnabled = _driver->isMotionWakeupConfigured();
    return _motionWakeEnabled;
}

bool GestureRead::disableMotionWakeup()
{
    if (!_driver)
    {
        return false;
    }

    if (!_driver->disableMotionWakeup())
    {
        return false;
    }

    _motionWakeEnabled = false;
    return true;
}

bool GestureRead::isMotionWakeEnabled() const
{
    return _motionWakeEnabled && _driver && _driver->isMotionWakeupConfigured();
}

bool GestureRead::clearMotionWakeInterrupt()
{
    if (!_driver || !_motionWakeEnabled)
    {
        return false;
    }
    return _driver->clearMotionInterrupt();
}

bool GestureRead::isMotionWakeTriggered()
{
    if (!_driver || !_motionWakeEnabled)
    {
        return false;
    }
    return _driver->getMotionInterruptStatus(true);
}

float GestureRead::getAxisValue(uint8_t index)
{
    if (!_driver || _axisMap.length() < 3 || _axisDir.length() < 3)
    {
        return 0.0f;
    }

    char axis = tolower(_axisMap[index]);
    float value = 0.0f;

    switch (axis)
    {
    case 'x':
        value = _driver->readX();
        break;
    case 'y':
        value = _driver->readY();
        break;
    case 'z':
        value = _driver->readZ();
        break;
    default:
        return 0.0f;
    }

    if (_axisDir[index] == '-')
    {
        value = -value;
    }

    return value;
}

float GestureRead::getGyroAxisValue(uint8_t index)
{
    if (!_driver || _axisMap.length() < 3 || _axisDir.length() < 3)
    {
        return 0.0f;
    }

    char axis = tolower(_axisMap[index]);
    float value = 0.0f;

    switch (axis)
    {
    case 'x':
        value = _driver->readGyroX();
        break;
    case 'y':
        value = _driver->readGyroY();
        break;
    case 'z':
        value = _driver->readGyroZ();
        break;
    default:
        return 0.0f;
    }

    if (_axisDir[index] == '-')
    {
        value = -value;
    }

    return value;
}

void GestureRead::getMappedGyro(float &x, float &y, float &z)
{
    x = getGyroAxisValue(0);
    y = getGyroAxisValue(1);
    z = getGyroAxisValue(2);
}

float GestureRead::getMappedX()
{
    return getAxisValue(0);
}

float GestureRead::getMappedY()
{
    return getAxisValue(1);
}

float GestureRead::getMappedZ()
{
    return getAxisValue(2);
}

void GestureRead::applyAxisMap(const String &axisMap, const String &axisDir)
{
    char axes[4] = {'z', 'y', 'x', '\0'};
    char dirs[4] = {'+', '+', '-', '\0'};
    char currentSign = '+';
    uint8_t axisCount = 0;

    for (uint16_t i = 0; i < axisMap.length() && axisCount < 3; ++i)
    {
        char c = axisMap.charAt(i);
        if (c == '+' || c == '-')
        {
            currentSign = c;
            continue;
        }

        c = tolower(c);
        if (c == 'x' || c == 'y' || c == 'z')
        {
            axes[axisCount] = c;
            dirs[axisCount] = currentSign;
            currentSign = '+';
            axisCount++;
        }
    }

    if (axisDir.length() >= 3)
    {
        for (uint8_t i = 0; i < 3; ++i)
        {
            char dirChar = axisDir.charAt(i);
            if (dirChar == '+' || dirChar == '-')
            {
                dirs[i] = dirChar;
            }
        }
    }

    if (axisCount == 3)
    {
        _axisMap = String(axes);
    }
    else
    {
        _axisMap = "zyx";
        dirs[0] = '+';
        dirs[1] = '+';
        dirs[2] = '-';
    }

    _axisDir = String(dirs);
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
    if (!_driver)
    {
        return false;
    }

    if (!_expectGyro)
    {
        return true;
    }

    const unsigned long start = millis();
    bool seenGyroData = false;
    uint8_t zeroGyroSamples = 0;

    while (millis() - start < timeoutMs)
    {
        if (_driver->update() && _driver->hasGyroscope())
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
    if (!_driver)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(_bufferMutex);

    const unsigned long currentTime = millis();
    const unsigned long interval = sampleIntervalMs(_sampleHZ);

    if (currentTime - lastSampleTime < interval)
    {
        return;
    }

    const bool sampling = _isSampling;
    const uint16_t count = _sampleBuffer.sampleCount;

    if (sampling && count < _maxSamples)
    {
        if (_driver->update())
        {
            Sample &sample = _sampleBuffer.samples[count];
            const float rawX = getMappedX();
            const float rawY = getMappedY();
            const float rawZ = getMappedZ();

            float mappedGyroX = 0.0f;
            float mappedGyroY = 0.0f;
            float mappedGyroZ = 0.0f;
            const bool gyroAvailable = _driver->hasGyroscope();
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

            sample.temperatureValid = _driver->hasTemperature();
            sample.temperature = sample.temperatureValid ? _driver->readTemperatureC() : 0.0f;

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
