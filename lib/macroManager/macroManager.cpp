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


#include "macroManager.h"
#include <Arduino.h>
#include "configTypes.h"
#include "gestureRead.h"
#include "gestureAnalyze.h"
#include <WIFIManager.h>
#include <Logger.h>
#include <BLEController.h>
#include "Led.h"
#include "InputHub.h"
#include "GyroMouse.h"
#include "combinationManager.h"

extern WIFIManager wifiManager;
extern BLEController bleController;
extern InputHub inputHub;
extern GyroMouse gyroMouse;
extern CombinationManager comboManager;

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
        if (inputHub.startGestureCapture())
        {
            logMessage = "started " + String(action.c_str());
            is_action_locked = true;
        }
        else
        {
            logMessage = "failed to start " + String(action.c_str());
            is_action_locked = false;
        }
    }
    else if (action == "RESET_ALL")
    {
        specialAction.resetDevice();
    }
    else if (action == "CALIBRATE_SENSOR")
    {
        specialAction.calibrateSensor();
    }
    else if (action == "MEM_INFO")
    {
        specialAction.printMemoryInfo();
    }
    else if (action == "HOP_BLE_DEVICE")
    {
        specialAction.hopBleDevice();
    }
    else if (action == "ENTER_SLEEP")

    {
        specialAction.enterSleep();
    }
    else if (action == "GYROMOUSE_START")
    {
        if (!gyroMouse.isRunning())
        {
            if (!gyroModeActive)
            {
                String currentPrefix = comboManager.getCurrentPrefix();
                savedComboPrefix = std::string(currentPrefix.c_str());
                savedComboSetNumber = comboManager.getCurrentSet();
                hasSavedCombo = true;
            }

            gyroMouse.start();
            gyroModeActive = gyroMouse.isRunning();

            if (gyroModeActive)
            {
                pendingComboSwitchFlag = true;
                pendingComboPrefix = "gyromouse_combo";
                pendingComboSetNumber = 0;
            }
            else
            {
                Logger::getInstance().log("GyroMouse: failed to start (check configuration)");
            }
        }
    }
    else if (action == "GYROMOUSE_STOP")
    {
        if (gyroMouse.isRunning())
        {
            gyroMouse.stop();
        }

        if (gyroModeActive && hasSavedCombo)
        {
            pendingComboSwitchFlag = true;
            pendingComboPrefix = savedComboPrefix;
            pendingComboSetNumber = savedComboSetNumber;
        }

        gyroModeActive = false;
    }
    else if (action == "GYROMOUSE_TOGGLE")
    {
        if (gyroMouse.isRunning())
        {
            gyroMouse.stop();

            if (gyroModeActive && hasSavedCombo)
            {
                pendingComboSwitchFlag = true;
                pendingComboPrefix = savedComboPrefix;
                pendingComboSetNumber = savedComboSetNumber;
            }
            gyroModeActive = false;
        }
        else
        {
            // Reuse start logic
            pressAction("GYROMOUSE_START");
        }
    }
    else if (action == "GYROMOUSE_CYCLE_SENSITIVITY")
    {
        if (gyroMouse.isRunning())
        {
            gyroMouse.cycleSensitivity();
            Logger::getInstance().log("GyroMouse: Sensitivity -> " + gyroMouse.getSensitivityName());
        }
        else
        {
            Logger::getInstance().log("GyroMouse: Cycle request ignored (mode inactive)");
        }
    }
    else if (action == "GYROMOUSE_RECENTER")
    {
        if (gyroMouse.isRunning())
        {
            gyroMouse.recenterNeutral();
        }
        else
        {
            Logger::getInstance().log("GyroMouse: Recenter request ignored (mode inactive)");
        }
    }
    else if (action.rfind("DELAY_", 0) == 0)
    {
        // Estrae il valore del delay in ms
        std::string delayStr = action.substr(6);
        int totalDelayMs = std::stoi(delayStr);

        specialAction.actionDelay(totalDelayMs);
    }

    // IR Remote Control Commands (toggle pattern)
    else if (action.rfind("SCAN_IR_DEV_", 0) == 0)
    {
        // Extract device ID from "SCAN_IR_DEV_X"
        std::string devStr = action.substr(12); // After "SCAN_IR_DEV_"
        int deviceId = std::stoi(devStr);

        // Pass the activation combo as exit combo
        String exitCombo = String(currentActivationCombo.c_str());
        specialAction.toggleScanIR(deviceId, exitCombo);

        // Clean up state after IR mode exits
        clearActiveKeys();
    }
    else if (action.rfind("SEND_IR_", 0) == 0)
    {
        // Hierarchical IR send command handling
        // Extract the part after "SEND_IR_" (8 characters)
        std::string remainder = action.substr(8);

        // 1. Check for interactive mode: SEND_IR_DEV_<deviceId>
        if (remainder.rfind("DEV_", 0) == 0)
        {
            // Interactive send mode
            std::string devStr = remainder.substr(4); // After "DEV_"
            try
            {
                int deviceId = std::stoi(devStr);
                String exitCombo = String(currentActivationCombo.c_str());
                specialAction.toggleSendIR(deviceId, exitCombo);
                clearActiveKeys();
            }
            catch (const std::exception &e)
            {
                Logger::getInstance().log("Invalid SEND_IR_DEV format: " + String(action.c_str()));
            }
        }
        // 2. Check for numeric direct send: SEND_IR_CMD_<deviceId>_CMD<commandId>
        else if (remainder.rfind("CMD_", 0) == 0)
        {
            std::string numericPart = remainder.substr(4); // After "CMD_"
            size_t cmdPos = numericPart.find("_CMD");

            if (cmdPos != std::string::npos)
            {
                std::string devStr = numericPart.substr(0, cmdPos);
                std::string cmdStr = numericPart.substr(cmdPos + 4);

                try
                {
                    int deviceId = std::stoi(devStr);
                    int commandId = std::stoi(cmdStr);
                    String deviceName = String("dev") + String(deviceId);
                    String commandName = String("cmd") + String(commandId);
                    specialAction.sendIRCommand(deviceName, commandName);
                }
                catch (const std::exception &e)
                {
                    Logger::getInstance().log("Invalid SEND_IR_CMD format: " + String(action.c_str()));
                }
            }
        }
        // 3. Descriptive direct send: SEND_IR_<deviceName>_<commandName>
        else
        {
            size_t underscorePos = remainder.find('_');
            if (underscorePos != std::string::npos)
            {
                String deviceName = String(remainder.substr(0, underscorePos).c_str());
                String commandName = String(remainder.substr(underscorePos + 1).c_str());
                specialAction.sendIRCommand(deviceName, commandName);
            }
            else
            {
                Logger::getInstance().log("Invalid SEND_IR format (missing underscore): " + String(action.c_str()));
            }
        }
    }
    else if (action == "IR_CHECK")
    {
        specialAction.checkIRSignal();
    }

    // LED RGB Control Commands
    else if (action.rfind("LED_RGB_", 0) == 0)
    {
        // Extract parameters after "LED_RGB_"
        std::string params = action.substr(8); // Skip "LED_RGB_"

        // Parse the three color components separated by underscores
        std::vector<std::string> components;
        size_t start = 0;
        size_t end = params.find('_');

        // Split by underscores
        while (end != std::string::npos)
        {
            components.push_back(params.substr(start, end - start));
            start = end + 1;
            end = params.find('_', start);
        }
        // Add last component
        components.push_back(params.substr(start));

        if (components.size() == 3)
        {
            int values[3] = {0, 0, 0};           // Absolute values
            int deltas[3] = {0, 0, 0};           // Relative adjustments
            bool hasRelative = false;
            bool hasAbsolute = false;

            // Parse each component
            for (int i = 0; i < 3; i++)
            {
                std::string comp = components[i];

                // Check for PLUS/MINUS modifiers
                if (comp == "PLUS_PLUS")
                {
                    deltas[i] = specialAction.ledAdjustmentStep * 2;
                    hasRelative = true;
                }
                else if (comp == "PLUS")
                {
                    deltas[i] = specialAction.ledAdjustmentStep;
                    hasRelative = true;
                }
                else if (comp == "MINUS_MINUS")
                {
                    deltas[i] = -specialAction.ledAdjustmentStep * 2;
                    hasRelative = true;
                }
                else if (comp == "MINUS")
                {
                    deltas[i] = -specialAction.ledAdjustmentStep;
                    hasRelative = true;
                }
                else
                {
                    // Try to parse as number (absolute value)
                    try
                    {
                        values[i] = std::stoi(comp);
                        hasAbsolute = true;
                    }
                    catch (const std::exception &e)
                    {
                        Logger::getInstance().log("Invalid LED component: " + String(comp.c_str()));
                        return;
                    }
                }
            }

            // Execute the command based on what we parsed
            if (hasRelative && !hasAbsolute)
            {
                // Pure relative adjustment
                specialAction.adjustLedColor(deltas[0], deltas[1], deltas[2]);
            }
            else if (hasAbsolute && !hasRelative)
            {
                // Pure absolute set
                specialAction.setLedColor(values[0], values[1], values[2], false);
            }
            else if (hasAbsolute && hasRelative)
            {
                // Mixed: first get current color, apply deltas, then set absolutes
                int currentRed, currentGreen, currentBlue;
                Led::getInstance().getColor(currentRed, currentGreen, currentBlue);

                // Start with current values
                int finalRed = currentRed;
                int finalGreen = currentGreen;
                int finalBlue = currentBlue;

                // Apply deltas where specified
                if (deltas[0] != 0) finalRed += deltas[0];
                else if (values[0] != 0 || components[0] == "0") finalRed = values[0];

                if (deltas[1] != 0) finalGreen += deltas[1];
                else if (values[1] != 0 || components[1] == "0") finalGreen = values[1];

                if (deltas[2] != 0) finalBlue += deltas[2];
                else if (values[2] != 0 || components[2] == "0") finalBlue = values[2];

                specialAction.setLedColor(finalRed, finalGreen, finalBlue, false);
            }
        }
        else
        {
            Logger::getInstance().log("Invalid LED_RGB format: expected 3 components");
        }
    }
    else if (action == "LED_OFF")
    {
        specialAction.turnOffLed();
    }
    else if (action == "LED_SAVE")
    {
        specialAction.saveLedColor();
    }
    else if (action == "LED_RESTORE")
    {
        specialAction.restoreLedColor();
    }
    else if (action == "LED_INFO")
    {
        specialAction.showLedInfo();
    }

    // LED Brightness Control Commands
    else if (action.rfind("LED_BRIGHTNESS_", 0) == 0)
    {
        // Extract parameter after "LED_BRIGHTNESS_"
        std::string param = action.substr(15); // Skip "LED_BRIGHTNESS_"

        // Check for PLUS with optional multiplier (PLUS, PLUS2, PLUS3, etc.)
        if (param.rfind("PLUS", 0) == 0)
        {
            int multiplier = 1;
            if (param.length() > 4) // Has number after PLUS
            {
                try
                {
                    std::string numStr = param.substr(4);
                    multiplier = std::stoi(numStr);
                }
                catch (const std::exception &e)
                {
                    Logger::getInstance().log("Invalid PLUS multiplier: " + String(param.c_str()));
                    multiplier = 1;
                }
            }
            specialAction.adjustBrightness(specialAction.brightnessAdjustmentStep * multiplier);
        }
        // Check for MINUS with optional multiplier (MINUS, MINUS0, MINUS1, MINUS2, etc.)
        else if (param.rfind("MINUS", 0) == 0)
        {
            int multiplier = 1;
            if (param.length() > 5) // Has number after MINUS
            {
                try
                {
                    std::string numStr = param.substr(5);
                    int num = std::stoi(numStr);
                    // MINUS0 or MINUS1 = same as MINUS (multiplier 1)
                    multiplier = (num <= 1) ? 1 : num;
                }
                catch (const std::exception &e)
                {
                    Logger::getInstance().log("Invalid MINUS multiplier: " + String(param.c_str()));
                    multiplier = 1;
                }
            }
            specialAction.adjustBrightness(-specialAction.brightnessAdjustmentStep * multiplier);
        }
        else if (param == "INFO")
        {
            specialAction.showBrightnessInfo();
        }
        else
        {
            // Try to parse as absolute value (0-255)
            try
            {
                int brightness = std::stoi(param);
                specialAction.setBrightness(brightness);
            }
            catch (const std::exception &e)
            {
                Logger::getInstance().log("Invalid LED_BRIGHTNESS format: " + String(param.c_str()));
            }
        }
    }

    // Flashlight Command - Toggle white LED full brightness
    else if (action == "FLASHLIGHT")
    {
        specialAction.toggleFlashlight();
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
    if (is_action_locked && !(action == "EXECUTE_GESTURE" || processingCommandQueue))
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
        if (inputHub.stopGestureCapture())
        {
            logMessage += " gesture capture stopped";
        }
        else
        {
            logMessage += " gesture capture already idle";
        }
        is_action_locked = false;
    }

    // IR Remote Control no longer needs release actions (now using toggle pattern)
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
    else if (action == "REACTIVE_LIGHTING")
    {
        const bool newState = !inputHub.isReactiveLightingEnabled();
        inputHub.setReactiveLightingEnabled(newState);
    }
    else if (action == "SAVE_INTERACTIVE_COLORS")
    {
        inputHub.saveReactiveLightingColors();
    }
    else if (action == "GYROMOUSE_START" ||
             action == "GYROMOUSE_STOP" ||
             action == "GYROMOUSE_TOGGLE" ||
             action == "GYROMOUSE_CYCLE_SENSITIVITY" ||
             action == "GYROMOUSE_RECENTER")
    {
        // No release action required for GyroMouse commands
    }
    else if (action.rfind("SWITCH_MY_COMBO_", 0) == 0)
    {
        // Extract combo number from "SWITCH_MY_COMBO_X"
        std::string numStr = action.substr(16); // After "SWITCH_MY_COMBO_"
        try
        {
            int comboNum = std::stoi(numStr);
            Logger::getInstance().log("Switch to my_combo_" + String(comboNum) + " requested");
            // Set pending flag instead of executing immediately to avoid stack issues
            pendingComboSwitchFlag = true;
            pendingComboPrefix = "my_combo";
            pendingComboSetNumber = comboNum;

            if (gyroModeActive)
            {
                savedComboPrefix = pendingComboPrefix;
                savedComboSetNumber = pendingComboSetNumber;
                hasSavedCombo = true;
            }
        }
        catch (const std::exception &e)
        {
            Logger::getInstance().log("Invalid SWITCH_MY_COMBO format: " + String(action.c_str()));
        }
    }
    else if (action.rfind("SWITCH_COMBO_", 0) == 0)
    {
        // Extract combo number from "SWITCH_COMBO_X"
        std::string numStr = action.substr(13); // After "SWITCH_COMBO_"
        try
        {
            int comboNum = std::stoi(numStr);
            Logger::getInstance().log("Switch to combo_" + String(comboNum) + " requested");
            // Set pending flag instead of executing immediately to avoid stack issues
            pendingComboSwitchFlag = true;
            pendingComboPrefix = "combo";
            pendingComboSetNumber = comboNum;

            if (gyroModeActive)
            {
                savedComboPrefix = pendingComboPrefix;
                savedComboSetNumber = pendingComboSetNumber;
                hasSavedCombo = true;
            }
        }
        catch (const std::exception &e)
        {
            Logger::getInstance().log("Invalid SWITCH_COMBO format: " + String(action.c_str()));
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
            // Handle reactive lighting for key press
            inputHub.handleReactiveLighting(event.value1, false, 0, activeKeysMask);

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
            pendingGestureFallback.clear();
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
            // Handle reactive lighting for encoder rotation
            inputHub.handleReactiveLighting(0, true, event.value1, activeKeysMask);

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

            // Save the activation combo for IR commands
            currentActivationCombo = fullCombo;

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
            // Handle reactive lighting for encoder button
            inputHub.handleReactiveLighting(0, true, 0, activeKeysMask);

            lastAction = "BUTTON";
            pendingCombination = getCurrentCombination();
            pendingGestureFallback.clear();
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

    case InputEvent::EventType::MOTION:
        if (event.state && event.value1 >= 0)
        {
            std::string gestureKey;
            if (event.text.length() > 0)
            {
                gestureKey = event.text.c_str();
            }

            pendingGestureFallback.clear();
            if (event.value1 >= 0)
            {
                pendingGestureFallback = "G_ID:" + std::to_string(event.value1);
            }

            if (!gestureKey.empty())
            {
                pendingCombination = gestureKey;
            }
            else
            {
                pendingCombination = pendingGestureFallback;
                pendingGestureFallback.clear();
            }

            lastCombinationTime = millis();
            newKeyPressed = true;
            gestureExecuted = true;
            gestureExecutionTime = millis();
            String logMessage = "Gesture event recognized: ";
            if (!gestureKey.empty())
            {
                logMessage += event.text;
                if (event.value1 >= 0)
                {
                    logMessage += " (G_ID:";
                    logMessage += String(event.value1);
                    logMessage += ")";
                }
            }
            else
            {
                logMessage += "G_ID:";
                logMessage += String(event.value1);
            }
            Logger::getInstance().log(logMessage);
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

bool MacroManager::executeCombinationActions(const std::string &comboKey)
{
    if (comboKey.empty())
    {
        return false;
    }

    auto it = combinations.find(comboKey);
    if (it == combinations.end())
    {
        return false;
    }

    currentActivationCombo = comboKey;

    if (!lastExecutedAction.empty())
    {
        releaseAction(lastExecutedAction);
        lastExecutedAction.clear();
    }

    bool hasChainedCommands = false;
    for (const std::string &action : it->second)
    {
        if (action.find('<') != std::string::npos && action.find('>') != std::string::npos)
        {
            hasChainedCommands = true;
            enqueueCommands(action);
            break;
        }
    }

    if (!hasChainedCommands)
    {
        for (const std::string &action : it->second)
        {
            pressAction(action);
            lastExecutedAction = action;
        }
    }

    return true;
}

void MacroManager::processKeyCombination()
{
    if (newKeyPressed && (!pendingCombination.empty() || !pendingGestureFallback.empty()))
    {
        bool executed = executeCombinationActions(pendingCombination);

        if (!executed && !pendingGestureFallback.empty())
        {
            executed = executeCombinationActions(pendingGestureFallback);
        }

        if (!executed)
        {
            if (!lastExecutedAction.empty())
            {
                releaseAction(lastExecutedAction);
                lastExecutedAction.clear();
            }

            std::string missingKey = !pendingCombination.empty() ? pendingCombination : pendingGestureFallback;
            Logger::getInstance().log(String("combinazione non impostata") + String(missingKey.c_str()));
        }

        pendingCombination.clear();
        pendingGestureFallback.clear();
        newKeyPressed = false;
    }
}

// Aggiornare clearActiveKeys per pulire anche keyPressOrder
void MacroManager::clearActiveKeys()
{
    activeKeysMask = 0;
    previousKeysMask = 0;
    pendingCombination.clear();
    pendingGestureFallback.clear();
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

bool MacroManager::reloadCombinationsFromManager(JsonObject newCombos)
{
    // Clear all active states first
    clearActiveKeys();

    // Clear existing combinations map to free memory
    combinations.clear();

    // Reload combinations from the new JsonObject
    for (JsonPair combo : newCombos)
    {
        // Skip _settings entry as it's not a key combination
        if (String(combo.key().c_str()) == "_settings")
        {
            continue;
        }

        std::vector<std::string> actions;
        for (JsonVariant v : combo.value().as<JsonArray>())
        {
            actions.push_back(std::string(v.as<const char *>()));
        }
        combinations[std::string(combo.key().c_str())] = actions;
    }

    Logger::getInstance().log("Reloaded " + String(combinations.size()) + " combinations into macroManager");
    return combinations.size() > 0;
}

bool MacroManager::hasPendingComboSwitch()
{
    return pendingComboSwitchFlag;
}

void MacroManager::getPendingComboSwitch(std::string& outPrefix, int& outSetNumber)
{
    outPrefix = pendingComboPrefix;
    outSetNumber = pendingComboSetNumber;
}

void MacroManager::clearPendingComboSwitch()
{
    pendingComboSwitchFlag = false;
    pendingComboPrefix.clear();
    pendingComboSetNumber = 0;
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
    if (newKeyPressed &&
        (!pendingCombination.empty() || !pendingGestureFallback.empty()) &&
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

    inputHub.updateReactiveLighting();

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
