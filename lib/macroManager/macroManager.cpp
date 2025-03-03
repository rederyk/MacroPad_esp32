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
      previousKeysMask(0),
      lastCombinationTime(0),
      pendingCombination(""),
      keypadConfig(config),
      wifiConfig(wifiConfig),
      newKeyPressed(false),
      encoderReleaseScheduled(false)
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
        // Salva lo stato precedente prima dell'aggiornamento
        previousKeysMask = activeKeysMask;
        wasPartOfCombo = false;

        // Se è un rilascio di tasto che fa parte di una combinazione attiva
        if (!event.state && !lastExecutedAction.empty() && getKeyState(activeKeysMask, event.value1))
        {
            wasPartOfCombo = true;
        }

        // Aggiorna lo stato del tasto
        setKeyState(activeKeysMask, event.value1, event.state);

        if (event.state)
        {
            // On key press, update the time and set pending combination
            lastKeyPressTime = millis();
            pendingCombination = getCurrentCombination();
            lastCombinationTime = millis();
            newKeyPressed = true; // Flag that a new key was pressed
        }
        else if (wasPartOfCombo)
        {
            // Se rilasciamo un tasto che faceva parte di una combo, rilasciamo l'intera combo
            if (!lastExecutedAction.empty())
            {
                releaseAction(lastExecutedAction);
                lastExecutedAction.clear();
              //  Logger::getInstance().log("Released combo on key release");
            }
        }
        break;

    case InputEvent::EventType::ROTATION:
        // Gestione completa di un impulso di encoder (atomica)
        if (event.state)
        {
            // Determina la direzione
            std::string encoderAction = (event.value1 > 0) ? "CW" : "CCW";
            std::string fullCombo = "";

            // Costruisci la combinazione con l'encoder e i tasti attuali
            if (activeKeysMask != 0)
            {
                // Ci sono tasti premuti, costruisci la combo "tasti+encoder"
                std::string keysCombo = getCurrentKeyCombination(); // Solo i tasti
                if (!keysCombo.empty())
                {
                    fullCombo = keysCombo + "," + encoderAction;
                }
                else
                {
                    fullCombo = encoderAction;
                }
            }
            else
            {
                // Solo encoder
                fullCombo = encoderAction;
            }

            Logger::getInstance().log("Encoder pulse: " + String(encoderAction.c_str()) + " combo: " + String(fullCombo.c_str()));

            // Rilascia l'azione precedente se esiste
            if (!lastExecutedAction.empty())
            {
                releaseAction(lastExecutedAction);
                lastExecutedAction.clear();
            }

            // Controlla se questa combo esiste
            if (combinations.find(fullCombo) != combinations.end())
            {
                // Esegui l'azione
                for (const std::string &action : combinations[fullCombo])
                {
                    pressAction(action);
                    lastExecutedAction = action;

                    // Rilascia immediatamente dopo per garantire un impulso completo
                    // Ritardiamo leggermente il rilascio per dare tempo all'azione di essere eseguita
                    encoderReleaseScheduled = true;
                    encoderReleaseTime = millis() + encoder_pulse_duration;
                    encoderPendingAction = action;
                }
            }
        }
        break;

    case InputEvent::EventType::BUTTON:
        if (event.state)
        {
            lastAction = "BUTTON";
            pendingCombination = getCurrentCombination();
            lastCombinationTime = millis();
            newKeyPressed = true; // Flag that a new input was registered
        }
        else
        {
            lastAction.clear();

            // Se rilasciamo un pulsante che faceva parte di una combo, rilascia l'azione
            if (!lastExecutedAction.empty())
            {
                releaseAction(lastExecutedAction);
                lastExecutedAction.clear();
                Logger::getInstance().log("Released combo on button release");
            }
        }
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

std::string MacroManager::getCurrentKeyCombination()
{
    char buffer[64]; // Pre-allocate buffer for combination
    char *ptr = buffer;
    bool first = true;

    // Add active keys using bitmask (WITHOUT lastAction)
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
            }
            first = false;
        }
    }

    *ptr = '\0'; // Null-terminate the string
    return std::string(buffer);
}

void MacroManager::processKeyCombination()
{
    // Only execute if a new key was pressed and the combo is valid
    if (newKeyPressed && !pendingCombination.empty())
    {
        // Check if there is a valid combination
        if (combinations.find(pendingCombination) != combinations.end())
        {
            // Release previous action if exists
            if (!lastExecutedAction.empty())
            {
                releaseAction(lastExecutedAction);
                lastExecutedAction.clear();
            }

            // Execute the new combination
            for (const std::string &action : combinations[pendingCombination])
            {
                pressAction(action);
                lastExecutedAction = action;
            }
        }

        pendingCombination.clear();
        newKeyPressed = false; // Reset the flag
    }
}

void MacroManager::clearActiveKeys()
{
    activeKeysMask = 0;
    previousKeysMask = 0;
    pendingCombination.clear();
    newKeyPressed = false;

    // Se c'è un'azione attiva, rilasciala
    if (!lastExecutedAction.empty())
    {
        releaseAction(lastExecutedAction);
        lastExecutedAction.clear();
    }

    lastAction.clear();
}

void MacroManager::update()
{
    unsigned long currentTime = millis();

    // Process pending combination if the combo_delay has passed and a new key was pressed
    if (!pendingCombination.empty() && newKeyPressed &&
        (currentTime - lastCombinationTime >= combo_delay))
    {
        processKeyCombination();
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

    // Gestione del rilascio programmato dell'encoder
    if (encoderReleaseScheduled && currentTime >= encoderReleaseTime)
    {
        if (!encoderPendingAction.empty())
        {
            releaseAction(encoderPendingAction);
           // Logger::getInstance().log("Auto-releasing encoder action: " + String(encoderPendingAction.c_str()));
            encoderPendingAction.clear();
            lastExecutedAction.clear();
        }
        encoderReleaseScheduled = false;
    }

   // Logger::getInstance().processBuffer();
}

void MacroManager::releaseGestureActions()
{
    if (!lastExecutedAction.empty())
    {
        releaseAction(lastExecutedAction);
        lastExecutedAction.clear();
    }
}