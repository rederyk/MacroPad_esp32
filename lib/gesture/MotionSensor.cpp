#include "MotionSensor.h"

#include <Arduino.h>
#include <Logger.h>

#include <ADXL345.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

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
    virtual bool hasGyroscope() const { return false; }
    virtual float readGyroX() const { return 0.0f; }
    virtual float readGyroY() const { return 0.0f; }
    virtual float readGyroZ() const { return 0.0f; }
    virtual bool hasTemperature() const { return false; }
    virtual float readTemperatureC() const { return 0.0f; }
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
    virtual bool disableMotionWakeup() { return true; }
    virtual bool isMotionWakeupConfigured() const { return false; }
    virtual bool clearMotionInterrupt() { return true; }
    virtual bool getMotionInterruptStatus(bool clear)
    {
        (void)clear;
        return false;
    }
};

namespace
{
    uint16_t clampSampleRateImpl(uint16_t hz)
    {
        if (hz == 0)
        {
            return MotionSensor::kDefaultSampleHz;
        }
        return std::min<uint16_t>(hz, 1000);
    }

    uint32_t sampleIntervalMsImpl(uint16_t hz)
    {
        hz = clampSampleRateImpl(hz);
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
    constexpr uint8_t MPU6050_INT_DATA_RDY_BIT = 0x01;

    class ADXL345Driver final : public AccelerometerDriver
    {
    public:
        ADXL345Driver(TwoWire *wire, uint8_t address, const String &axisMap, const String &axisDir)
            : _address(address ? address : ADXL345_ALT),
              _sensor(_address, wire),
              _axisMap("xyz"),
              _axisDir("+++")
        {
            // Parse and apply axis mapping (ADXL345 needs this, MPU6050 doesn't)
            applyAxisMapping(axisMap, axisDir);
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
            // NOTE: setRange will be called by MotionSensor::begin() with config.sensitivity
            // Don't set it here to avoid conflicts
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
            return getAxisValue(0);
        }

        float readY() const override
        {
            return getAxisValue(1);
        }

        float readZ() const override
        {
            return getAxisValue(2);
        }

        bool setSampleRate(uint16_t hz, bool lowPower) override
        {
            hz = clampSampleRateImpl(hz);
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
        void applyAxisMapping(const String &axisMap, const String &axisDir)
        {
            char axes[4] = {'x', 'y', 'z', '\0'};
            char dirs[4] = {'+', '+', '+', '\0'};
            char currentSign = '+';
            uint8_t axisCount = 0;

            // Parse axisMap string (e.g., "xzy", "-x+y-z")
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

            // Override with explicit axisDir if provided
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
                _axisDir = String(dirs);
            }
        }

        float getAxisValue(uint8_t index) const
        {
            if (_axisMap.length() < 3 || _axisDir.length() < 3)
            {
                return 0.0f;
            }

            // Get raw sensor value based on axis mapping
            char axis = tolower(_axisMap[index]);
            float value = 0.0f;

            switch (axis)
            {
            case 'x':
                value = _sensor.getX();
                break;
            case 'y':
                value = _sensor.getY();
                break;
            case 'z':
                value = _sensor.getZ();
                break;
            default:
                return 0.0f;
            }

            // Apply direction inversion if needed
            if (_axisDir[index] == '-')
            {
                value = -value;
            }

            return value;
        }

        uint8_t _address;
        mutable ADXL345 _sensor;
        String _axisMap;
        String _axisDir;
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
                    // NOTE: setRange will be called by MotionSensor::begin() with config.sensitivity
                    // Don't set it here to avoid conflicts
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
            return _accelEvent.acceleration.x / MotionSensor::kGravity;
        }

        float readY() const override
        {
            return _accelEvent.acceleration.y / MotionSensor::kGravity;
        }

        float readZ() const override
        {
            return _accelEvent.acceleration.z / MotionSensor::kGravity;
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
            hz = clampSampleRateImpl(hz);

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
              _sampleRate(MotionSensor::kDefaultSampleHz),
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

            // NOTE: setRange will be called by MotionSensor::begin() with config.sensitivity
            // Don't set it here to avoid conflicts

            if (!setSampleRate(MotionSensor::kDefaultSampleHz, false))
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

            // DEBUG: Log first NON-ZERO update to verify sensor is reading correctly
            static bool firstUpdate = true;
            if (firstUpdate && (rawX != 0 || rawY != 0 || rawZ != 0)) {
                Logger::getInstance().log("[MPU6050Clone] First update - RAW values:");
                Logger::getInstance().log("  rawX=" + String(rawX) + " rawY=" + String(rawY) + " rawZ=" + String(rawZ));
                Logger::getInstance().log("  _scale=" + String(_scale, 2) + " (should be 8192.00 for ±4g)");
                Logger::getInstance().log("  Expected: accelX=" + String(rawX / 8192.0f, 4) + "g");
                Logger::getInstance().log("  Actual:   accelX=" + String(rawX / _scale, 4) + "g");
                firstUpdate = false;
            }

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
            hz = clampSampleRateImpl(hz);
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

            Logger::getInstance().log("[MPU6050Clone] setRange(" + String(g, 1) + "g) -> configValue=0x" +
                                     String(configValue, HEX) + ", scale=" + String(scale, 2));

            if (!writeRegister(_wire, _address, MPU6050_REG_ACCEL_CONFIG, configValue))
            {
                Logger::getInstance().log("[MPU6050Clone] ERROR: Failed to write ACCEL_CONFIG register!");
                return false;
            }

            // Verify the register was written correctly
            uint8_t readback = 0;
            if (readRegister(_wire, _address, MPU6050_REG_ACCEL_CONFIG, readback))
            {
                Logger::getInstance().log("[MPU6050Clone] ACCEL_CONFIG readback: 0x" + String(readback, HEX) +
                                         " (expected: 0x" + String(configValue, HEX) + ")");
                if (readback != configValue)
                {
                    Logger::getInstance().log("[MPU6050Clone] WARNING: Readback mismatch! Register may not be set correctly.");
                }
            }
            else
            {
                Logger::getInstance().log("[MPU6050Clone] WARNING: Failed to read back ACCEL_CONFIG register!");
            }

            _scale = scale;
            Logger::getInstance().log("[MPU6050Clone] _scale updated to " + String(_scale, 2));
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

            // Clear any pending interrupt (read twice for some clones)
            uint8_t status = 0;
            readRegister(_wire, _address, MPU6050_REG_INT_STATUS, status);
            vTaskDelay(pdMS_TO_TICKS(10));
            readRegister(_wire, _address, MPU6050_REG_INT_STATUS, status);
            vTaskDelay(pdMS_TO_TICKS(50));

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
            // Read interrupt status register twice to ensure it's cleared
            // Some clones need multiple reads to properly clear latched interrupts
            if (!readRegister(_wire, _address, MPU6050_REG_INT_STATUS, status))
            {
                return false;
            }
            vTaskDelay(pdMS_TO_TICKS(5));
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
            // MPU6050 has gyro - no axis mapping needed (uses sensor fusion)
            if (useCloneDriver)
            {
                return std::unique_ptr<AccelerometerDriver>(new MPU6050CloneDriver(wire, config.address));
            }
            return std::unique_ptr<AccelerometerDriver>(new MPU6050Driver(wire, config.address));
        }

        // ADXL345 needs axis mapping (no gyro for auto-orientation)
        return std::unique_ptr<AccelerometerDriver>(new ADXL345Driver(wire, config.address, config.axisMap, config.axisDir));
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

MotionSensor::MotionSensor(TwoWire *wire)
    : _wire(wire),
      _configLoaded(false),
      _expectGyro(false),
      _motionWakeEnabled(false),
      _sampleHz(kDefaultSampleHz)
{
}

MotionSensor::~MotionSensor() = default;

bool MotionSensor::begin(const AccelerometerConfig &config)
{
    _config = config;
    _configLoaded = true;
    String normalizedType = _config.type;
    normalizedType.toLowerCase();
    _expectGyro = (normalizedType == "mpu6050");

    _sampleHz = clampSampleRate(config.sampleRate > 0 ? config.sampleRate : kDefaultSampleHz);

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
    _driver->setSampleRate(_sampleHz, false);

    Logger::getInstance().log("Accelerometer initialised: " + String(_driver->name()));

    _motionWakeEnabled = false;
    return true;
}

bool MotionSensor::isReady() const
{
    return _driver != nullptr;
}

bool MotionSensor::start()
{
    if (!_driver)
    {
        return false;
    }
    return _driver->start();
}

bool MotionSensor::stop()
{
    if (!_driver)
    {
        return false;
    }
    return _driver->stop();
}

bool MotionSensor::setSampleRate(uint16_t hz, bool lowPower)
{
    if (!_driver)
    {
        return false;
    }
    _sampleHz = clampSampleRate(hz);
    return _driver->setSampleRate(_sampleHz, lowPower);
}

bool MotionSensor::setRange(float g)
{
    if (!_driver)
    {
        return false;
    }
    return _driver->setRange(g);
}

uint16_t MotionSensor::sampleRateHz() const
{
    return _sampleHz;
}

bool MotionSensor::update()
{
    if (!_driver)
    {
        return false;
    }
    return _driver->update();
}

float MotionSensor::getMappedX() const
{
    if (!_driver)
    {
        return 0.0f;
    }
    return _driver->readX();
}

float MotionSensor::getMappedY() const
{
    if (!_driver)
    {
        return 0.0f;
    }
    return _driver->readY();
}

float MotionSensor::getMappedZ() const
{
    if (!_driver)
    {
        return 0.0f;
    }
    return _driver->readZ();
}

void MotionSensor::getMappedAcceleration(float &x, float &y, float &z) const
{
    x = getMappedX();
    y = getMappedY();
    z = getMappedZ();
}

void MotionSensor::getMappedGyro(float &x, float &y, float &z) const
{
    if (!_driver)
    {
        x = y = z = 0.0f;
        return;
    }
    // Gyro values are NOT mapped - gyroscope measures angular velocity
    // in the sensor's physical reference frame
    x = _driver->readGyroX();
    y = _driver->readGyroY();
    z = _driver->readGyroZ();
}


bool MotionSensor::hasGyro() const
{
    return _driver && _driver->hasGyroscope();
}

bool MotionSensor::hasTemperature() const
{
    return _driver && _driver->hasTemperature();
}

float MotionSensor::readTemperatureC() const
{
    if (!_driver)
    {
        return 0.0f;
    }
    return _driver->readTemperatureC();
}

bool MotionSensor::expectsGyro() const
{
    return _expectGyro;
}

bool MotionSensor::configureMotionWakeup(uint8_t threshold, uint8_t duration, uint8_t highPassCode, uint8_t cycleRateCode)
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

bool MotionSensor::disableMotionWakeup()
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

bool MotionSensor::isMotionWakeConfigured() const
{
    return _driver && _driver->isMotionWakeupConfigured();
}

bool MotionSensor::clearMotionInterrupt()
{
    if (!_driver || !_motionWakeEnabled)
    {
        return false;
    }
    return _driver->clearMotionInterrupt();
}

bool MotionSensor::getMotionInterruptStatus(bool clear)
{
    if (!_driver || !_motionWakeEnabled)
    {
        return false;
    }
    return _driver->getMotionInterruptStatus(clear);
}

const AccelerometerConfig &MotionSensor::config() const
{
    return _config;
}

const char *MotionSensor::driverName() const
{
    return _driver ? _driver->name() : "uninitialised";
}

uint16_t MotionSensor::clampSampleRate(uint16_t hz)
{
    return clampSampleRateImpl(hz);
}

uint32_t MotionSensor::sampleIntervalMs(uint16_t hz)
{
    return sampleIntervalMsImpl(hz);
}
