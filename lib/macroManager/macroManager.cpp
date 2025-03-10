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
      encoderReleaseScheduled(false),
      useKeyPressOrder(false), // Di default usa il metodo originale
      processingCommandQueue(false)
{
    lastActionTime = millis();
}

// Parse a composite action string and extract commands enclosed in <>
std::vector<std::string> MacroManager::parseChainedCommands(const std::string &compositeAction)
{
    std::vector<std::string> commands;

    // Check if the action contains any <> tags
    if (compositeAction.find('<') == std::string::npos ||
        compositeAction.find('>') == std::string::npos)
    {
        // No chained commands, just return the original action
        commands.push_back(compositeAction);
        return commands;
    }

    // Parse out all commands enclosed in <> and any text outside
    size_t startPos = 0;
    size_t openBracket = compositeAction.find('<', startPos);

    // Parse through the string to find all commands
    while (startPos < compositeAction.length())
    {
        // If we have an opening bracket, look for the closing one
        if (openBracket != std::string::npos)
        {
            // If there's text before the bracket, add it as a command
            if (openBracket > startPos)
            {
                std::string plainCommand = compositeAction.substr(startPos, openBracket - startPos);
                if (!plainCommand.empty())
                {
                    commands.push_back(plainCommand);
                }
            }

            // Find the matching closing bracket
            size_t closeBracket = compositeAction.find('>', openBracket);
            if (closeBracket != std::string::npos)
            {
                // Extract the command inside the brackets (without the brackets)
                std::string bracketCommand = compositeAction.substr(openBracket + 1, closeBracket - openBracket - 1);
                if (!bracketCommand.empty())
                {
                    commands.push_back(bracketCommand);
                }

                // Move past this command
                startPos = closeBracket + 1;
                openBracket = compositeAction.find('<', startPos);
            }
            else
            {
                // No closing bracket found, treat the rest as a single command
                std::string remainder = compositeAction.substr(startPos);
                if (!remainder.empty())
                {
                    commands.push_back(remainder);
                }
                break;
            }
        }
        else
        {
            // No more opening brackets, add the rest as a command
            std::string remainder = compositeAction.substr(startPos);
            if (!remainder.empty())
            {
                commands.push_back(remainder);
            }
            break;
        }
    }

    return commands;
}

// New method to handle command queuing
void MacroManager::enqueueCommands(const std::string &compositeAction)
{
    // Parse the composite action into individual commands
    std::vector<std::string> commands = parseChainedCommands(compositeAction);

    // If there's only one command and it doesn't use <> syntax, use normal execution
    if (commands.size() == 1 && compositeAction.find('<') == std::string::npos)
    {
        pressAction(commands[0]);
        lastExecutedAction = commands[0];
        return;
    }

    // Otherwise, queue all commands for sequential execution
    // First, clear any existing queue
    commandQueue.clear();
    // Then add all new commands
    commandQueue = commands;
    processingCommandQueue = true;
    nextCommandTime = millis();

    // We set this flag to prevent other actions, but our queue commands will still run
    is_action_locked = true;

    Logger::getInstance().log("Enqueued " + String(commands.size()) + " commands for sequential execution");
}

// Process the command queue in the update method
void MacroManager::processCommandQueue()
{
    unsigned long currentTime = millis();

    // If we're not processing a queue or it's not time for the next command yet, return
    if (!processingCommandQueue || currentTime < nextCommandTime)
    {
        return;
    }

    // If the queue is empty, we're done
    if (commandQueue.empty())
    {
        processingCommandQueue = false;
        is_action_locked = false;
        return;
    }

    // Get the next command from the queue
    std::string command = commandQueue.front();
    commandQueue.erase(commandQueue.begin());

    // Handle the special marker for ending the queue
    if (command == "__RELEASE_LAST__")
    {
        // Ensure we clean up properly
        if (!lastExecutedAction.empty())
        {
            releaseAction(lastExecutedAction);
            lastExecutedAction.clear();
        }
        processingCommandQueue = false;
        is_action_locked = false;
        return;
    }

    // Execute the command
    Logger::getInstance().log("Executing queued command: " + String(command.c_str()));

    // Release any previous action first
    if (!lastExecutedAction.empty())
    {
        releaseAction(lastExecutedAction);
        lastExecutedAction.clear();
    }

    // Press the new action
    pressAction(command);
    lastExecutedAction = command;

    // If this was the last command, we'll release it after a delay
    if (commandQueue.empty())
    {
        commandQueue.push_back("__RELEASE_LAST__"); // Special marker for release only
    }

    // Schedule the next command
    nextCommandTime = currentTime + COMMAND_DELAY;
}

void MacroManager::pressAction(const std::string &action)
{
    // Allow commands from the queue to execute even when actions are locked
    if (is_action_locked && !(action == "RESET_ALL") && !processingCommandQueue)
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
    else if (action == "ENTER_SLEEP")
    {
        specialAction.enterSleep();
    }
    else if (action.rfind("DELAY_", 0) == 0)
    {
        // Estrae il valore del delay in ms
        std::string delayStr = action.substr(6);
        int totalDelayMs = std::stoi(delayStr);

        specialAction.actionDelay(totalDelayMs);
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
    // Skip if it's our special marker
    if (action == "__RELEASE_LAST__")
    {
        processingCommandQueue = false;
        is_action_locked = false;
        return;
    }

    String logMessage = "Released action: " + String(action.c_str());

    // If action is locked but this is part of our command queue, let it proceed
    if (is_action_locked && !(action == "TRAIN_GESTURE" || action == "EXECUTE_GESTURE" || processingCommandQueue))
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
        newKeyPressed = true;
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
    else if (action == "TOGGLE_KEY_ORDER")
    {
        if (useKeyPressOrder)
        {
            setUseKeyPressOrder(false);
        }
        else
        {
            setUseKeyPressOrder(true);
        }
    }

    Logger::getInstance().log(logMessage);
    return;
}

// Aggiornare handleInputEvent per registrare l'ordine di pressione
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
            // Quando un tasto viene premuto, aggiungilo alla lista dell'ordine di pressione
            if (useKeyPressOrder)
            {
                // Rimuovi prima il tasto se già presente (in caso di ripetizione)
                for (auto it = keyPressOrder.begin(); it != keyPressOrder.end();)
                {
                    if (it->keyIndex == event.value1)
                    {
                        it = keyPressOrder.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }

                // Aggiungi il nuovo tasto premuto
                KeyPressInfo newPress;
                newPress.keyIndex = static_cast<uint8_t>(event.value1);
                newPress.timestamp = millis();
                keyPressOrder.push_back(newPress);
            }

            // Resto del codice originale
            lastKeyPressTime = millis();
            pendingCombination = getCurrentCombination();
            lastCombinationTime = millis();
            newKeyPressed = true;
        }
        else
        {
            // Quando un tasto viene rilasciato, rimuovilo dalla lista
            if (useKeyPressOrder)
            {
                for (auto it = keyPressOrder.begin(); it != keyPressOrder.end();)
                {
                    if (it->keyIndex == event.value1)
                    {
                        it = keyPressOrder.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
            }

            if (wasPartOfCombo)
            {
                // Codice originale per il rilascio di combo
                if (!lastExecutedAction.empty())
                {
                    releaseAction(lastExecutedAction);
                    lastExecutedAction.clear();
                }
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
                // Check if any action contains <> syntax
                bool hasChainedCommands = false;
                for (const std::string &action : combinations[fullCombo])
                {
                    if (action.find('<') != std::string::npos && action.find('>') != std::string::npos)
                    {
                        hasChainedCommands = true;
                        enqueueCommands(action);
                        break;
                    }
                }

                // If no chained commands, execute normally
                if (!hasChainedCommands)
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

// Metodo per ottenere la combinazione basata sull'ordine di pressione
std::string MacroManager::getOrderAwareCombination()
{
    char buffer[64]; // Pre-allocate buffer
    char *ptr = buffer;
    bool first = true;

    // Itera attraverso i tasti nell'ordine in cui sono stati premuti
    for (const auto &keyInfo : keyPressOrder)
    {
        uint8_t key = keyInfo.keyIndex;
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

// Modifica getCurrentCombination per scegliere il metodo appropriato
std::string MacroManager::getCurrentCombination()
{
    if (useKeyPressOrder)
    {
        return getOrderAwareCombination();
    }
    else
    {
        // Implementazione originale
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
}

// Metodo simile per getCurrentKeyCombination
std::string MacroManager::getCurrentKeyCombination()
{
    if (useKeyPressOrder)
    {
        char buffer[64];
        char *ptr = buffer;
        bool first = true;

        for (const auto &keyInfo : keyPressOrder)
        {
            uint8_t key = keyInfo.keyIndex;
            if (!first)
            {
                *ptr++ = '+';
            }

            uint8_t row = key / keypadConfig->cols;
            uint8_t col = key % keypadConfig->cols;
            char keyLabel = this->keypadConfig->keys[row][col];
            if (keyLabel != '\0')
            {
                *ptr++ = keyLabel;
            }
            first = false;
        }

        *ptr = '\0';
        return std::string(buffer);
    }
    else
    {
        // Implementazione originale
        char buffer[64];
        char *ptr = buffer;
        bool first = true;

        uint8_t totalKeys = keypadConfig->rows * keypadConfig->cols;
        for (uint8_t key = 0; key < totalKeys; key++)
        {
            if (getKeyState(activeKeysMask, key))
            {
                if (!first)
                {
                    *ptr++ = '+';
                }
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

        *ptr = '\0';
        return std::string(buffer);
    }
}

// Aggiungi metodo per modificare l'impostazione di ordinamento
void MacroManager::setUseKeyPressOrder(bool useOrder)
{
    useKeyPressOrder = useOrder;
    if (useKeyPressOrder)
    {
        // Se attiviamo l'ordine, pulisci la lista corrente
        keyPressOrder.clear();

        // Popola la lista con i tasti attualmente premuti (in ordine di indice)
        uint8_t totalKeys = keypadConfig->rows * keypadConfig->cols;
        for (uint8_t key = 0; key < totalKeys; key++)
        {
            if (getKeyState(activeKeysMask, key))
            {
                KeyPressInfo info = {key, millis()};
                keyPressOrder.push_back(info);
            }
        }
    }
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

            // Check if any action contains <> syntax
            bool hasChainedCommands = false;
            for (const std::string &action : combinations[pendingCombination])
            {
                if (action.find('<') != std::string::npos && action.find('>') != std::string::npos)
                {
                    hasChainedCommands = true;
                    enqueueCommands(action);
                    break;
                }
            }

            // If no chained commands, execute normally
            if (!hasChainedCommands)
            {
                for (const std::string &action : combinations[pendingCombination])
                {
                    pressAction(action);
                    lastExecutedAction = action;
                }
            }
        }
        else
        {
            // Release previous action if exists
            if (!lastExecutedAction.empty())
            {
                releaseAction(lastExecutedAction);
                lastExecutedAction.clear();
            }

            Logger::getInstance().log(String("combinazione non impostata") + String(pendingCombination.c_str()));
        }

        pendingCombination.clear();
        newKeyPressed = false; // Reset the flag
    }
}

// Aggiornare clearActiveKeys per pulire anche keyPressOrder
void MacroManager::clearActiveKeys()
{
    activeKeysMask = 0;
    previousKeysMask = 0;
    pendingCombination.clear();
    newKeyPressed = false;
    keyPressOrder.clear(); // Pulisci anche l'ordine di pressione

    // Reset command queue if it was in progress
    if (processingCommandQueue)
    {
        commandQueue.clear();
        processingCommandQueue = false;
        is_action_locked = false;
    }

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

    // Process command queue if active
    if (processingCommandQueue)
    {
        processCommandQueue();
        return; // Skip other processing while queue is active
    }

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