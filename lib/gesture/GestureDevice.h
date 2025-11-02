/*
 * ESP32 MacroPad Project
 * Copyright (C) [2025]
 *
 * Licensed under the GNU GPL v3 or later.
 */

#ifndef GESTURE_DEVICE_H
#define GESTURE_DEVICE_H

#include <Arduino.h>
#include "inputDevice.h"
#include "gestureRead.h"
#include "gestureAnalyze.h"

/**
 * @brief Wrapper that exposes gesture capture and recognition
 *        as a standard InputDevice for the InputHub pipeline.
 */
class GestureDevice : public InputDevice
{
public:
    enum class State : uint8_t
    {
        Idle,
        Capturing,
        PendingRecognition
    };

    GestureDevice(GestureRead &sensor, GestureAnalyze &analyzer);

    void setup() override;
    bool processInput() override;
    InputEvent getEvent() override;

    bool startCapture();
    bool stopCapture();
    bool isCapturing() const;

    State getState() const { return state; }

    bool hasSensor() const { return sensorAvailable; }
    void setSensorAvailable(bool available);

    int getLastGestureId() const { return lastGestureId; }
    const String& getLastGestureName() const { return lastGestureName; }
    void clearLastGesture();
    void setRecognitionEnabled(bool enable);
    bool isRecognitionEnabled() const { return recognitionEnabled; }

    /**
     * Calibrate accelerometer axes automatically
     * User should hold device in normal position (buttons facing them, vertical)
     * and keep it still for ~2 seconds
     *
     * @param saveToFile If true, saves calibration to config.json
     * @return true if calibration successful
     */
    bool calibrateAxes(bool saveToFile = true);

private:
    void resetEvent();
    bool performRecognition();

    GestureRead &sensor;
    GestureAnalyze &analyzer;

    State state;
    bool sensorAvailable;

    InputEvent pendingEvent;
    bool eventReady;

    int lastGestureId;
    String lastGestureName;
    bool recognitionEnabled;
};

#endif // GESTURE_DEVICE_H
