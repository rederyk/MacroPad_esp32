#ifndef MACRO_MANAGER_H
#define MACRO_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <map>

// #define COMBO_DELAY 100 // Adjust this value as needed (milliseconds)
#include <string>
#include "inputDevice.h"
#include "configTypes.h"
#include "specialAction.h"
#include "configManager.h"
extern SpecialAction specialAction;

class MacroManager
{
private:
    // Track active keys using bitmask (bits 0-15 represent keys 1-16)
    uint16_t activeKeysMask;
    // Keypad configuration reference
    const KeypadConfig *keypadConfig;
    // Wifi configuration reference
    const WifiConfig *wifiConfig;
    // Last input action
    std::string lastAction;
    // Timeout for key combinations
    static constexpr unsigned long combo_delay = 50; // 100ms to complete combination
    // SE rilascio il tasto in meno di questo tempo non viene recepito il rilascio,,,,,,TODO indagare 
    // Last input action timestamp
    unsigned long lastActionTime;
    unsigned long lastRotationTime;
    // Pending combination and its timestamp
    std::string pendingCombination;
    std::string lastExecutedCombination;
    unsigned long lastCombinationTime;
    unsigned long rotationReleaseTime = 0;  // Variabile globale o membro di classe

    int expectedGestureId; // ID del gesto atteso
    std::string lastExecutedAction;
    // static bool is_action_locked;
    bool is_action_locked;
    bool gestureExecuted = false;
    bool rotationNeedsRelease = false;
    bool pendingRotationPress = false;
    bool rotationActive = false;
    unsigned long lastKeyPressTime = 0; // Timestamp of the last key press
    static constexpr unsigned long debounceTime = 50; // Minimum time between key presses (milliseconds)
    unsigned long gestureExecutionTime;
    static constexpr unsigned long GESTURE_HOLD_TIME = 200; // Time to hold gesture actions (milliseconds)

    // Helper methods
    std::string getCurrentCombination();
    void clearActiveKeys();
    void releaseGestureActions(); // Declare the releaseGestureActions function
    void pressAction(const std::string &action);
    void processCombination(const InputEvent &event);
    void releaseAction(const std::string &action);

public:
    void update();
    std::map<std::string, std::vector<std::string>> combinations;

public:
    MacroManager(const KeypadConfig *config, const WifiConfig *wifiConfig);

    // Process events
    void handleInputEvent(const InputEvent &event);
};

#endif
