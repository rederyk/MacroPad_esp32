/*
 * ESP32 MacroPad Project
 * GestureDevice implementation
 */

#include "GestureDevice.h"
#include "gestureAxisCalibration.h"
#include "Logger.h"

GestureDevice::GestureDevice(GestureRead &sensorRef, GestureAnalyze &analyzerRef)
    : sensor(sensorRef),
      analyzer(analyzerRef),
      state(State::Idle),
      sensorAvailable(false),
      eventReady(false),
      lastGestureId(-1),
      lastGestureName(""),
      recognitionEnabled(true)
{
    resetEvent();
}

void GestureDevice::setup()
{
    clearLastGesture();
    resetEvent();
    state = State::Idle;
    recognitionEnabled = true;
}

bool GestureDevice::processInput()
{
    if (!sensorAvailable)
    {
        return false;
    }

    sensor.updateSampling();

    if (state == State::Capturing && !sensor.isSampling())
    {
        state = State::PendingRecognition;
    }

    if (state == State::PendingRecognition)
    {
        if (!recognitionEnabled)
        {
            state = State::Idle;
            return false;
        }

        if (performRecognition())
        {
            state = State::Idle;
            return true;
        }

        state = State::Idle;
    }

    return false;
}

InputEvent GestureDevice::getEvent()
{
    InputEvent evt = pendingEvent;
    resetEvent();
    return evt;
}

bool GestureDevice::startCapture()
{
    if (!sensorAvailable)
    {
        Logger::getInstance().log("GestureDevice: sensor not available");
        return false;
    }

    if (sensor.isSampling())
    {
        state = State::Capturing;
        return true;
    }

    if (!sensor.startSampling())
    {
        Logger::getInstance().log("GestureDevice: failed to start capture");
        return false;
    }

    state = State::Capturing;
    clearLastGesture();
    resetEvent();
    return true;
}

bool GestureDevice::stopCapture()
{
    if (!sensorAvailable)
    {
        return false;
    }

    const bool wasSampling = sensor.isSampling();

    if (wasSampling)
    {
        sensor.stopSampling();
    }

    if (state == State::Capturing || wasSampling)
    {
        state = State::PendingRecognition;
        return true;
    }

    return false;
}

bool GestureDevice::isCapturing() const
{
    return sensorAvailable && sensor.isSampling();
}

void GestureDevice::setSensorAvailable(bool available)
{
    sensorAvailable = available;
    if (!sensorAvailable)
    {
        state = State::Idle;
        clearLastGesture();
        resetEvent();
    }
}

void GestureDevice::clearLastGesture()
{
    lastGestureId = -1;
    lastGestureName = "";
}

void GestureDevice::setRecognitionEnabled(bool enable)
{
    recognitionEnabled = enable;
}

void GestureDevice::resetEvent()
{
    pendingEvent.type = InputEvent::EventType::MOTION;
    pendingEvent.value1 = -1;
    pendingEvent.value2 = 0;
    pendingEvent.state = false;
    pendingEvent.text = "";
    eventReady = false;
}

bool GestureDevice::performRecognition()
{
    resetEvent();

    SampleBuffer &buffer = sensor.getCollectedSamples();
    if (buffer.sampleCount == 0 || buffer.samples == nullptr)
    {
        Logger::getInstance().log("GestureDevice: no samples collected");
        sensor.clearMemory();
        return false;
    }

    if (!analyzer.hasRecognizer())
    {
        Logger::getInstance().log("GestureDevice: no recognizer available");
        sensor.clearMemory();
        return false;
    }

    GestureRecognitionResult result = analyzer.recognizeWithRecognizer();
    lastGestureId = result.gestureID;
    lastGestureName = (result.gestureID >= 0) ? result.gestureName : "";

    if (result.gestureID < 0 || result.confidence < analyzer.getConfidenceThreshold())
    {
        Logger::getInstance().log("GestureDevice: gesture not recognized (low confidence)");
        lastGestureName = "";
        sensor.clearMemory();
        return false;
    }

    Logger::getInstance().log("GestureDevice: recognized " + result.gestureName +
                             " (mode: " + analyzer.getRecognizerModeName() +
                             ", confidence: " + String(result.confidence * 100, 0) + "%)");

    pendingEvent.type = InputEvent::EventType::MOTION;
    pendingEvent.value1 = result.gestureID;
    pendingEvent.value2 = buffer.sampleCount;
    pendingEvent.state = true;
    pendingEvent.text = result.gestureName;
    eventReady = true;

    sensor.clearMemory();
    return eventReady;
}

bool GestureDevice::calibrateAxes(bool saveToFile)
{
    if (!sensorAvailable) {
        Logger::getInstance().log("[GestureDevice] Sensor not available for calibration");
        return false;
    }

    Logger::getInstance().log("[GestureDevice] Starting axis calibration...");
    Logger::getInstance().log("[GestureDevice] HOLD DEVICE IN NORMAL POSITION:");
    Logger::getInstance().log("[GestureDevice] - Buttons facing you");
    Logger::getInstance().log("[GestureDevice] - Device vertical");
    Logger::getInstance().log("[GestureDevice] - Keep VERY STILL for 2 seconds...");

    // Give user time to position device
    delay(1000);

    // Get motion sensor from gesture reader
    MotionSensor* motionSensor = sensor.getMotionSensor();
    if (!motionSensor) {
        Logger::getInstance().log("[GestureDevice] Cannot access motion sensor");
        return false;
    }

    // Perform calibration
    AxisCalibration calibrator;
    AxisCalibrationResult result = calibrator.calibrate(motionSensor, 2000);

    if (!result.success) {
        Logger::getInstance().log("[GestureDevice] Calibration FAILED");
        Logger::getInstance().log("[GestureDevice] Make sure device is held still and vertical");
        return false;
    }

    Logger::getInstance().log("[GestureDevice] Calibration SUCCESS!");
    Logger::getInstance().log("[GestureDevice] Detected configuration:");
    Logger::getInstance().log("[GestureDevice]   axisMap: \"" + result.axisMap + "\"");
    Logger::getInstance().log("[GestureDevice]   axisDir: \"" + result.axisDir + "\"");
    Logger::getInstance().log("[GestureDevice]   confidence: " + String(result.confidence * 100, 0) + "%");

    // Save to config file if requested
    if (saveToFile) {
        Logger::getInstance().log("[GestureDevice] Saving to config.json...");
        if (calibrator.saveToConfig(result)) {
            Logger::getInstance().log("[GestureDevice] Saved! Restart device to apply.");
            return true;
        } else {
            Logger::getInstance().log("[GestureDevice] Failed to save config");
            return false;
        }
    }

    return true;
}
