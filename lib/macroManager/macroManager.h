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

#define GESTURE_HOLD_TIME 200 // Tempo di mantenimento della gesture in ms

class MacroManager
{
public:
    MacroManager(const KeypadConfig *config, const WifiConfig *wifiConfig);
    void handleInputEvent(const InputEvent &event);
    void update();
    void clearActiveKeys();

    // Configurazione delle combinazioni
    std::map<std::string, std::vector<std::string>> combinations;
    unsigned long combo_delay = 50; // Default delay in ms
    unsigned long encoder_pulse_duration = 150; // Durata dell'impulso dell'encoder in ms

private:
    uint16_t activeKeysMask;    // Current key state
    uint16_t previousKeysMask;  // Previous key state to track changes
    unsigned long lastCombinationTime;
    unsigned long lastKeyPressTime;
    unsigned long lastRotationTime;
    unsigned long rotationReleaseTime;
    unsigned long lastActionTime;
    unsigned long encoderReleaseTime; // Tempo per il rilascio dell'encoder
    std::string pendingCombination;
    std::string lastAction;
    std::string lastExecutedAction;
    std::string encoderPendingAction; // Azione dell'encoder in attesa di rilascio
    const KeypadConfig *keypadConfig;
    const WifiConfig *wifiConfig;
    bool is_action_locked = false;
    bool gestureExecuted = false;
    bool wasPartOfCombo = false;
    bool newKeyPressed = false; // Flag to track when a new key is pressed
    bool encoderReleaseScheduled = false; // Flag per indicare il rilascio programmato dell'encoder
    unsigned long gestureExecutionTime = 0;

    void pressAction(const std::string &action);
    void releaseAction(const std::string &action);
    std::string getCurrentCombination();
    std::string getCurrentKeyCombination(); // Get only key combination without encoder/button actions
    //void processCombination(const InputEvent &event);
    void processKeyCombination();
    void releaseGestureActions();


};

#endif // MACRO_MANAGER_H