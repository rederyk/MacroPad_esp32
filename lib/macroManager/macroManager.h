/*
 * ESP32 MacroPad Project
 * Copyright (C) [2025] [Enrico Mori]
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


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
#define COMMAND_DELAY 200 // Delay between chained commands in ms

class MacroManager
{
public:
    MacroManager(const KeypadConfig *config, const WifiConfig *wifiConfig);
    void handleInputEvent(const InputEvent &event);
    void update();
    void clearActiveKeys();
    void setUseKeyPressOrder(bool useOrder);


    // Configurazione delle combinazioni
    std::map<std::string, std::vector<std::string>> combinations;
    unsigned long combo_delay = 50; // Default delay in ms
    unsigned long encoder_pulse_duration = 150; // Durata dell'impulso dell'encoder in ms

private:
    // Struttura per tenere traccia dell'ordine di pressione dei tasti
    struct KeyPressInfo {
        uint8_t keyIndex;     // Indice del tasto
        unsigned long timestamp;  // Timestamp di quando è stato premuto
    };
    std::vector<KeyPressInfo> keyPressOrder;  // Vettore per memorizzare l'ordine di pressione
    bool useKeyPressOrder;    // Flag per scegliere il metodo di ordinamento

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
    std::string currentActivationCombo; // Combo che ha attivato l'azione corrente
    std::string encoderPendingAction; // Azione dell'encoder in attesa di rilascio
    const KeypadConfig *keypadConfig;
    const WifiConfig *wifiConfig;
    bool is_action_locked = false;
    bool gestureExecuted = false;
    bool wasPartOfCombo = false;
    bool newKeyPressed = false; // Flag to track when a new key is pressed
    bool encoderReleaseScheduled = false; // Flag per indicare il rilascio programmato dell'encoder
    unsigned long gestureExecutionTime = 0;
    
    // Command chaining functionality
    std::vector<std::string> commandQueue;
    unsigned long nextCommandTime;
    bool processingCommandQueue = false;
    std::vector<std::string> parseChainedCommands(const std::string &compositeAction);
    void processCommandQueue();
    void enqueueCommands(const std::string &compositeAction);

    void pressAction(const std::string &action);
    void releaseAction(const std::string &action);
    std::string getCurrentCombination();
    std::string getOrderAwareCombination();
    std::string getCurrentKeyCombination(); // Get only key combination without encoder/button actions
    void processKeyCombination();
    void releaseGestureActions();
};

#endif // MACRO_MANAGER_H