/*
 * ESP32 MacroPad Project
 * GestureDevice implementation
 */

#include "GestureDevice.h"

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

    const int gestureId = analyzer.findKNNMatch(kDefaultKnnNeighbors);
    lastGestureId = gestureId;

    if (gestureId < 0)
    {
        Logger::getInstance().log("GestureDevice: gesture not recognized");
        sensor.clearMemory();
        return false;
    }

    pendingEvent.type = InputEvent::EventType::MOTION;
    pendingEvent.value1 = gestureId;
    pendingEvent.value2 = buffer.sampleCount;
    pendingEvent.state = true;
    eventReady = true;

    sensor.clearMemory();
    Logger::getInstance().log("GestureDevice: recognized gesture " + String(gestureId));
    return eventReady;
}
