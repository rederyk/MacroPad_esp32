#ifndef INPUT_HUB_H
#define INPUT_HUB_H

#include <Arduino.h>
#include <deque>
#include <memory>
#include <functional>

#include "inputDevice.h"
#include "ReactiveLightingController.h"

class ConfigurationManager;
class Keypad;
class RotaryEncoder;
class IRSensor;
class IRSender;
class IRStorage;
class GestureDevice;
struct ComboSettings;

/**
 * @brief Central coordinator for input devices.
 *
 * The InputHub owns the configured input peripherals, takes care of
 * their lifecycle, and exposes a simple event queue that can be
 * polled from the main loop. Events are collected in the order they
 * are generated to guarantee a deterministic delivery to the
 * MacroManager.
 */
class InputHub
{
public:
    struct TimedEvent
    {
        InputEvent event;
        unsigned long timestamp;
    };

    InputHub();
    ~InputHub();

    /**
     * @brief Initialise all configured input devices.
     *
     * @param configManager Configuration source for the hardware.
     * @return true when setup completed successfully.
     */
    bool begin(ConfigurationManager &configManager);

    /**
     * @brief Scan connected devices and enqueue newly generated events.
     */
    void scanDevices();

    /**
     * @brief Retrieve the next queued event.
     *
     * @param outEvent The retrieved event.
     * @return true when an event was available.
     */
    bool poll(InputEvent &outEvent);

    /**
     * @brief Retrieve the next queued event including timestamp.
     *
     * @param outEvent The retrieved timed event.
     * @return true when an event was available.
     */
    bool poll(TimedEvent &outEvent);

    /**
     * @brief Retrieve the next event that matches the predicate.
     *
     * Non-matching events are rotated to the back of the queue,
     * preserving their relative order.
     */
    bool pollFiltered(const std::function<bool(const InputEvent &)> &predicate, TimedEvent &outEvent);

    /**
     * @brief Remove any queued events.
     */
    void clearQueue();

    // Direct device accessors are provided for modules that need
    // exclusive control (e.g. interactive special actions).
    Keypad *getKeypad();
    RotaryEncoder *getRotaryEncoder();
    IRSensor *getIrSensor();
    IRSender *getIrSender();
    IRStorage *getIrStorage();

    // Gesture controls
    bool hasGestureSensor() const;
    bool startGestureCapture(bool enableRecognition = true);
    bool stopGestureCapture();
    bool isGestureCapturing() const;
    int getLastGestureId() const;
    void clearLastGesture();
    bool saveGestureSample(uint8_t id);

    // Reactive lighting controls
    void setReactiveLightingEnabled(bool enable);
    bool isReactiveLightingEnabled() const;
    void handleReactiveLighting(uint8_t keyIndex, bool isEncoder, int encoderDirection, uint16_t activeKeysMask);
    void updateReactiveLighting();
    void updateReactiveLightingColors(const ComboSettings &settings);
    void saveReactiveLightingColors() const;

private:
    static constexpr size_t MAX_QUEUE_SIZE = 32;

    void enqueue(const InputEvent &event);
    void scanKeypad();
    void scanRotaryEncoder();
    void scanGestures();

    std::deque<TimedEvent> eventQueue;
    std::unique_ptr<Keypad> keypad;
    std::unique_ptr<RotaryEncoder> rotaryEncoder;
    std::unique_ptr<IRSensor> irSensor;
    std::unique_ptr<IRSender> irSender;
    std::unique_ptr<IRStorage> irStorage;
    std::unique_ptr<GestureDevice> gestureDevice;
    ReactiveLightingController reactiveLighting;
};

#endif // INPUT_HUB_H
