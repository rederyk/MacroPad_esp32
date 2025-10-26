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
        resetEvent();
    }
}

void GestureDevice::clearLastGesture()
{
    lastGestureId = -1;
}

bool GestureDevice::saveGesture(uint8_t id)
{
    if (!sensorAvailable)
    {
        return false;
    }

    const bool result = analyzer.saveFeaturesWithID(id);
    if (result)
    {
        sensor.clearMemory();
    }
    return result;
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

    // Use new dual-mode recognition system (auto-selects best method)
    analyzer.setRecognitionMode(MODE_AUTO);
    analyzer.setConfidenceThreshold(0.5f);  // 50% minimum confidence

    GestureResult result = analyzer.recognizeGesture();
    lastGestureId = result.gestureID;

    if (result.gestureID < 0 || result.confidence < 0.5f)
    {
        Logger::getInstance().log("GestureDevice: gesture not recognized (low confidence)");
        sensor.clearMemory();
        return false;
    }

    // Log recognition details
    String modeName = (result.mode == MODE_SHAPE_RECOGNITION) ? "Shape" :
                     (result.mode == MODE_ORIENTATION) ? "Orientation" : "Legacy";
    Logger::getInstance().log("GestureDevice: recognized " + String(result.getName()) +
                             " (mode: " + modeName +
                             ", confidence: " + String(result.confidence * 100, 0) + "%)");

    pendingEvent.type = InputEvent::EventType::MOTION;
    pendingEvent.value1 = result.gestureID;
    pendingEvent.value2 = buffer.sampleCount;
    pendingEvent.state = true;
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
