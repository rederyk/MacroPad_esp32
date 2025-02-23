#ifndef GESTUREREAD_H
#define GESTUREREAD_H

#include <Arduino.h>
#include <Wire.h>
#include <ADXL345.h>
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

class GestureRead
{
public:
    GestureRead(uint8_t i2cAddress = ADXL345_ALT, TwoWire *wire = &Wire);
    ~GestureRead(); // Destructor declaration

    bool begin();
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
    ADXL345 _accelerometer;
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
