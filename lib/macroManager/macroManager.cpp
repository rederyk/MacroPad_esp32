#include "macroManager.h"
#include <Arduino.h>
#include "configTypes.h"
#include "gestureRead.h"
#include "gestureAnalyze.h"
#include <WIFIManager.h>
#include <Logger.h>
#include <BLEController.h>


extern WIFIManager wifiManager;
extern BLEController bleController;

// Bitmask helper functions
inline void setKeyState(uint16_t &mask, uint8_t key, bool state)
{
    if (state)
    {
        mask |= (1 << key);
    }
    else
    {
        mask &= ~(1 << key);
    }
}

inline bool getKeyState(uint16_t mask, uint8_t key)
{
    return (mask & (1 << key)) != 0;
}

MacroManager::MacroManager(const KeypadConfig *config, const WifiConfig *wifiConfig)
    : activeKeysMask(0),
      lastCombinationTime(0),
      pendingCombination(""),
      keypadConfig(config),
      wifiConfig(wifiConfig)
{
    lastActionTime = millis();
}
void MacroManager::pressAction(const std::string &action)
{
    if (is_action_locked && !(action == "RESET_ALL"))
    {
        Logger::getInstance().log("Action locked, skipping action: " + String(action.c_str()));
        // pendingCombination.clear(); // Clear the pending status
        return;
    }

    String logMessage = "Executing action: " + String(action.c_str());

    if (action.rfind("S_B:", 0) == 0 && bleController.isBleEnabled())
    {
        bleController.BLExecutor(action.c_str(), true);
    }
    else if (action == "EXECUTE_GESTURE")
    {
        logMessage = "started " + String(action.c_str());
        specialAction.toggleSampling(true);
        is_action_locked = true;
    }
    else if (action == "RESET_ALL")
    {
        specialAction.resetDevice();
    }
    else if (action == "CALIBRATE_SENSOR")
    {
        specialAction.calibrateSensor();
    }

    else if (action == "CONVERT_JSON_BINARY")
    {
        specialAction.convertJsonToBinary();
    }

    else if (action == "TRAIN_GESTURE")
    {
        specialAction.trainGesture(true);
        is_action_locked = true;
    }
    else if (action == "MEM_INFO")
    {
        specialAction.printMemoryInfo();
    }
    else if (action == "PRINT_JSON")
    {
        specialAction.printJson();
    }
    else if (action == "HOP_BLE_DEVICE")
    {
        specialAction.hopBleDevice();
    }
    // Is useless now??
    else if (action == "AP_MODE")
    {
        if (!bleController.isBleEnabled())
        {
            wifiManager.toggleAp(wifiConfig->ap_ssid.c_str(), wifiConfig->ap_password.c_str());
        }
        else
        {
            Logger::getInstance().log("riavvia in WIFImode");
        }
    }

    Logger::getInstance().log(logMessage);
}

void MacroManager::releaseAction(const std::string &action)
{
    String logMessage = "Released action: " + String(action.c_str());

    if (is_action_locked && !(action == "TRAIN_GESTURE" || action == "EXECUTE_GESTURE"))
    {
        Logger::getInstance().log("Action locked, skipping action: insideRealeaseactionvoid " + String(action.c_str()));
        return;
    }
    if (action.rfind("S_B:", 0) == 0 && bleController.isBleEnabled())
    {
        bleController.BLExecutor(action.c_str(), false);
    }
    else if (action == "EXECUTE_GESTURE")
    {
        logMessage += " sampling stopped and Founded ";

        specialAction.toggleSampling(false);
        String G_ID = specialAction.getGestureID();
        if (G_ID != "")
        {
            pendingCombination = G_ID.c_str();
        }
        is_action_locked = false;
        gestureExecuted = true;
        gestureExecutionTime = millis();
    }

    else if (action == "TOGGLE_SAMPLING")
    {
        specialAction.toggleSampling(false);
    }
    else if (action == "TRAIN_GESTURE")
    {

        specialAction.trainGesture(false);

        is_action_locked = false;
    }
    else if (action == "CLEAR_A_GESTURE")
    {
        specialAction.clearGestureWithID();
    }
    else if (action == "TOGGLE_BLE_WIFI")
    {
        specialAction.toggleBleWifi();
    }

    Logger::getInstance().log(logMessage);
    return;
}

void MacroManager::handleInputEvent(const InputEvent &event)
{
    switch (event.type)
    {
    case InputEvent::EventType::KEY_PRESS:
        setKeyState(activeKeysMask, event.value1, event.state);
        if (event.state)
        {
            lastKeyPressTime = millis();
        }
        pendingCombination = getCurrentCombination();
        lastCombinationTime = millis();
        break;
    case InputEvent::EventType::ROTATION:
        if (event.state)
        {
            lastAction = (event.value1 > 0) ? "CW" : "CCW";
            lastRotationTime = millis();
            rotationReleaseTime = 0; // Reset in caso di nuove rotazioni
        }
        else
        {
            // Non azzerare lastAction subito, ma memorizza il tempo di rilascio
            rotationReleaseTime = millis();
        }
        pendingCombination = getCurrentCombination();
        lastCombinationTime = millis();
        break;

    case InputEvent::EventType::BUTTON:
        if (event.state)
        {
            lastAction = "BUTTON";
        }
        else
        {
            lastAction.clear();
        }
        pendingCombination = getCurrentCombination();
        lastCombinationTime = millis();
        break;

    default:
        break;
    }
}

std::string MacroManager::getCurrentCombination()
{
    char buffer[64]; // Pre-allocate buffer for combination
    char *ptr = buffer;
    bool first = true;

    // Add active keys using bitmask
    uint8_t totalKeys = keypadConfig->rows * keypadConfig->cols;
    for (uint8_t key = 0; key < totalKeys; key++)
    {
        if (getKeyState(activeKeysMask, key))
        {
            if (!first)
            {
                *ptr++ = '+';
            }
            // Get key label from keypad configuration
            uint8_t row = key / keypadConfig->cols;
            uint8_t col = key % keypadConfig->cols;
            char keyLabel = this->keypadConfig->keys[row][col];
            if (keyLabel != '\0')
            {
                *ptr++ = keyLabel;
                // Logger::getInstance().log("Key label: " + String(keyLabel));
            }
            first = false;
        }
    }

    // Add last action if present
    if (!lastAction.empty())
    {
        if (!first)
        {
            *ptr++ = ',';
        }
        const char *actionPtr = lastAction.c_str();
        while (*actionPtr)
        {
            *ptr++ = *actionPtr++;
        }
    }

    *ptr = '\0'; // Null-terminate the string
    return std::string(buffer);
}

void MacroManager::clearActiveKeys()
{
    activeKeysMask = 0;
}

void MacroManager::processCombination(const InputEvent &event)
{
    std::string fullCombination = getCurrentCombination();

    // Regular combination processing
    pendingCombination = fullCombination;
    lastCombinationTime = millis();
}

void MacroManager::update()
{
    unsigned long currentTime = millis();

    //    // Se il rilascio della rotazione è in sospeso e combo_delay+1 è trascorso
    //    // Ce sicuramente un modo migliore
    //    if (rotationReleaseTime != 0 && (currentTime - rotationReleaseTime >= combo_delay + 1))
    //    {
    //        lastAction.clear();
    //        rotationReleaseTime = 0;
    //        releaseAction(lastExecutedAction);
    //        lastExecutedAction.clear();
    //    }
    //    // Se non ci sono chiavi attive e nessuna azione attuale, rilascia eventuali azioni pendenti
    //    if (activeKeysMask == 0 && lastAction.empty() && !lastExecutedAction.empty())
    //    {
    //        releaseAction(lastExecutedAction);
    //        lastExecutedAction.clear();
    //    }
    //
    // Controlla se il combo_delay è scaduto e se c'è una combinazione in sospeso
    if (!pendingCombination.empty() && (currentTime - lastCombinationTime >= combo_delay))
    {
        // Rilascia l'azione precedente se esiste (può essere utile per evitare doppie esecuzioni)
        if (!lastExecutedAction.empty())
        {
            releaseAction(lastExecutedAction);
            lastExecutedAction.clear();
        }
        // Esegui la nuova combinazione
        if (combinations.find(pendingCombination) != combinations.end())
        {
            for (const std::string &action : combinations[pendingCombination])
            {
                pressAction(action);
                lastExecutedAction = action;
            }
        }
        else
        {
            pressAction(pendingCombination);
            lastExecutedAction = pendingCombination;
        }
        pendingCombination.clear();
    }

    // Gestione del timeout per eventuali gesture, debounce, ecc.
    if (gestureExecuted)
    {
        if (currentTime - gestureExecutionTime > GESTURE_HOLD_TIME)
        {
            releaseGestureActions();
            gestureExecuted = false;
        }
    }

    // Se il rilascio della rotazione è in sospeso e combo_delay+1 è trascorso
    // Ce sicuramente un modo migliore
    if (rotationReleaseTime != 0 && (currentTime - rotationReleaseTime >= combo_delay + 1))
    {
        lastAction.clear();
        rotationReleaseTime = 0;
        releaseAction(lastExecutedAction);
        lastExecutedAction.clear();
    }
    // Se non ci sono chiavi attive e nessuna azione attuale, rilascia eventuali azioni pendenti
    if (activeKeysMask == 0 && lastAction.empty() && !lastExecutedAction.empty())
    {
        releaseAction(lastExecutedAction);
        lastExecutedAction.clear();
    }
    Logger::getInstance().processBuffer();
}

void MacroManager::releaseGestureActions()
{
    if (!lastExecutedAction.empty())
    {
        releaseAction(lastExecutedAction);
        lastExecutedAction.clear();
    }
}
