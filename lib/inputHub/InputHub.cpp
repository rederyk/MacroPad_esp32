#include "InputHub.h"

#include "configManager.h"
#include "keypad.h"
#include "rotaryEncoder.h"
#include "IRSensor.h"
#include "IRSender.h"
#include "IRStorage.h"
#include "Logger.h"
#include "combinationManager.h"
#include "GestureDevice.h"

extern GestureRead gestureSensor;
extern GestureAnalyze gestureAnalyzer;

InputHub::InputHub() = default;

InputHub::~InputHub() = default;

bool InputHub::begin(ConfigurationManager &configManager)
{
    const KeypadConfig &keypadConfig = configManager.getKeypadConfig();
    keypad.reset(new Keypad(&keypadConfig));
    keypad->setup();

    const EncoderConfig &encoderConfig = configManager.getEncoderConfig();
    rotaryEncoder.reset(new RotaryEncoder(&encoderConfig));
   rotaryEncoder->setup();

    const SystemConfig &systemConfig = configManager.getSystemConfig();
    const AccelerometerConfig &accelerometerConfig = configManager.getAccelerometerConfig();

    if (accelerometerConfig.active)
    {
        if (!gestureDevice)
        {
            gestureDevice.reset(new GestureDevice(gestureSensor, gestureAnalyzer));
        }
        gestureDevice->setSensorAvailable(true);
        gestureDevice->setup();
        Logger::getInstance().log("Gesture device registered");
    }
    else
    {
        if (gestureDevice)
        {
            gestureDevice.reset();
        }
        Logger::getInstance().log("Gesture device disabled (accelerometer inactive)");
    }

    const IRSensorConfig &irSensorConfig = configManager.getIrSensorConfig();
    const IRLedConfig &irLedConfig = configManager.getIrLedConfig();

    // IR Sensor: initialise only when BLE is disabled (shared RMT hardware)
    if (!systemConfig.enable_BLE)
    {
        Logger::getInstance().log("IR Sensor Config: pin=" + String(irSensorConfig.pin) +
                                  ", active=" + String(irSensorConfig.active ? "true" : "false"));
        if (irSensorConfig.active && irSensorConfig.pin >= 0)
        {
            Logger::getInstance().log("Initializing IR Sensor on pin " + String(irSensorConfig.pin));
            irSensor.reset(new IRSensor(irSensorConfig.pin));
            if (!irSensor->begin())
            {
                Logger::getInstance().log("Failed to initialize IR Sensor");
                irSensor.reset();
            }
            else
            {
                Logger::getInstance().log("IR Sensor initialized successfully");
            }
        }
        else
        {
            Logger::getInstance().log("IR Sensor NOT initialized (disabled or invalid pin)");
        }
    }
    else
    {
        Logger::getInstance().log("IR Sensor DISABLED (BLE enabled - RMT conflict)");
    }

    // IR Sender: can be active even with BLE enabled
    Logger::getInstance().log("IR LED Config: pin=" + String(irLedConfig.pin) +
                              ", active=" + String(irLedConfig.active ? "true" : "false") +
                              ", anodeGpio=" + String(irLedConfig.anodeGpio ? "true" : "false"));
    if (irLedConfig.active && irLedConfig.pin >= 0)
    {
        Logger::getInstance().log("Initializing IR Sender on pin " + String(irLedConfig.pin) +
                                  (systemConfig.enable_BLE ? " (BLE mode - IR receive disabled)" : ""));
        irSender.reset(new IRSender(irLedConfig.pin, irLedConfig.anodeGpio));
        if (!irSender->begin())
        {
            Logger::getInstance().log("Failed to initialize IR Sender");
            irSender.reset();
        }
        else
        {
            Logger::getInstance().log("IR Sender initialized successfully");
        }
    }
    else
    {
        Logger::getInstance().log("IR Sender NOT initialized (disabled or invalid pin)");
    }

    if (irSensor || irSender)
    {
        Logger::getInstance().log("Initializing IR Storage");
        irStorage.reset(new IRStorage());
        if (!irStorage->begin())
        {
            Logger::getInstance().log("Failed to initialize IR Storage (LittleFS error?)");
            irStorage.reset();
        }
        else if (irStorage->loadIRData())
        {
            Logger::getInstance().log("IR data loaded from file");
        }
        else
        {
            Logger::getInstance().log("No existing IR data file (this is normal on first run)");
        }
    }
    else
    {
        Logger::getInstance().log("IR Storage NOT initialized (no IR sensor or sender available)");
    }

    Logger::getInstance().log("Free heap after IR initialization: " + String(ESP.getFreeHeap()) + " bytes");
    return true;
}

void InputHub::scanDevices()
{
    scanKeypad();
    scanRotaryEncoder();
    scanGestures();
}

bool InputHub::poll(InputEvent &outEvent)
{
    if (eventQueue.empty())
    {
        return false;
    }
    outEvent = eventQueue.front().event;
    eventQueue.pop_front();
    return true;
}

bool InputHub::poll(TimedEvent &outEvent)
{
    if (eventQueue.empty())
    {
        return false;
    }
    outEvent = eventQueue.front();
    eventQueue.pop_front();
    return true;
}

bool InputHub::pollFiltered(const std::function<bool(const InputEvent &)> &predicate, TimedEvent &outEvent)
{
    const size_t initialSize = eventQueue.size();
    for (size_t i = 0; i < initialSize; ++i)
    {
        TimedEvent current = eventQueue.front();
        eventQueue.pop_front();
        if (predicate(current.event))
        {
            outEvent = current;
            return true;
        }
        eventQueue.push_back(current);
    }
    return false;
}

void InputHub::clearQueue()
{
    eventQueue.clear();
}

Keypad *InputHub::getKeypad()
{
    return keypad.get();
}

RotaryEncoder *InputHub::getRotaryEncoder()
{
    return rotaryEncoder.get();
}

IRSensor *InputHub::getIrSensor()
{
    return irSensor.get();
}

IRSender *InputHub::getIrSender()
{
    return irSender.get();
}

IRStorage *InputHub::getIrStorage()
{
    return irStorage.get();
}

bool InputHub::hasGestureSensor() const
{
    return gestureDevice && gestureDevice->hasSensor();
}

bool InputHub::startGestureCapture(bool enableRecognition)
{
    if (!gestureDevice)
    {
        Logger::getInstance().log("InputHub: gesture device not available");
        return false;
    }
    gestureDevice->setRecognitionEnabled(enableRecognition);
    gestureDevice->clearLastGesture();
    return gestureDevice->startCapture();
}

bool InputHub::stopGestureCapture()
{
    if (!gestureDevice)
    {
        return false;
    }
    return gestureDevice->stopCapture();
}

bool InputHub::isGestureCapturing() const
{
    return gestureDevice && gestureDevice->isCapturing();
}

int InputHub::getLastGestureId() const
{
    if (!gestureDevice)
    {
        return -1;
    }
    return gestureDevice->getLastGestureId();
}

void InputHub::clearLastGesture()
{
    if (gestureDevice)
    {
        gestureDevice->clearLastGesture();
    }
}

bool InputHub::saveGestureSample(uint8_t id)
{
    if (!gestureDevice)
    {
        return false;
    }
    return gestureDevice->saveGesture(id);
}

void InputHub::enqueue(const InputEvent &event)
{
    if (eventQueue.size() >= MAX_QUEUE_SIZE)
    {
        Logger::getInstance().log("InputHub queue full, dropping event");
        return;
    }

    TimedEvent timedEvent{event, millis()};
    eventQueue.push_back(timedEvent);
}

void InputHub::scanKeypad()
{
    if (!keypad)
    {
        return;
    }

    while (keypad->processInput())
    {
        enqueue(keypad->getEvent());
    }
}

void InputHub::scanRotaryEncoder()
{
    if (!rotaryEncoder)
    {
        return;
    }

    while (rotaryEncoder->processInput())
    {
        enqueue(rotaryEncoder->getEvent());
    }
}

void InputHub::scanGestures()
{
    if (!gestureDevice)
    {
        return;
    }

    if (gestureDevice->processInput())
    {
        enqueue(gestureDevice->getEvent());
    }
}

void InputHub::setReactiveLightingEnabled(bool enable)
{
    reactiveLighting.enable(enable);
}

bool InputHub::isReactiveLightingEnabled() const
{
    return reactiveLighting.isEnabled();
}

void InputHub::handleReactiveLighting(uint8_t keyIndex, bool isEncoder, int encoderDirection, uint16_t activeKeysMask)
{
    reactiveLighting.handleInput(keyIndex, isEncoder, encoderDirection, activeKeysMask);
}

void InputHub::updateReactiveLighting()
{
    reactiveLighting.update();
}

void InputHub::updateReactiveLightingColors(const ComboSettings &settings)
{
    reactiveLighting.updateColors(settings);
}

void InputHub::saveReactiveLightingColors() const
{
    reactiveLighting.saveColors();
}
