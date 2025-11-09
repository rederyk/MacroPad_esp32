#include "CommandFactory.h"
#include "Logger.h"

#include "ResetCommand.h"
#include "HopBleDeviceCommand.h"
#include "CalibrateSensorCommand.h"
#include "MemInfoCommand.h"
#include "EnterSleepCommand.h"
#include "IrCheckCommand.h"
#include "GyroMouseStartCommand.h"
#include "GyroMouseStopCommand.h"
#include "GyroMouseToggleCommand.h"
#include "GyroMouseCycleSensitivityCommand.h"
#include "GyroMouseRecenterCommand.h"
#include "DelayCommand.h"
#include "FlashlightCommand.h"
#include "ApModeCommand.h"
#include "BleCommand.h"
#include "ExecuteGestureCommand.h"
#include "ScanIrDevCommand.h"
#include "SendIrCommand.h"
#include "LedCommand.h"
#include "LedBrightnessCommand.h"
#include "ToggleBleWifiCommand.h"
#include "ToggleKeyOrderCommand.h"
#include "ToggleReactiveLightingCommand.h"
#include "SaveInteractiveColorsCommand.h"
#include "SwitchComboCommand.h"
// Include other command headers as they are created

// Dependencies for the commands
#include "specialAction.h"
#include "BLEController.h"
#include "GyroMouse.h"
#include "InputHub.h"
#include "WIFIManager.h"
#include "combinationManager.h"
#include "macroManager.h"

CommandFactory::CommandFactory(
    SpecialAction* specialAction,
    BLEController* bleController,
    GyroMouse* gyroMouse,
    InputHub* inputHub,
    WIFIManager* wifiManager,
    CombinationManager* comboManager
) : _specialAction(specialAction),
    _bleController(bleController),
    _gyroMouse(gyroMouse),
    _inputHub(inputHub),
    _wifiManager(wifiManager),
    _comboManager(comboManager),
    _macroManager(nullptr)
{}

void CommandFactory::setMacroManager(MacroManager* macroManager) {
    _macroManager = macroManager;
}

std::unique_ptr<Command> CommandFactory::create(const std::string& actionString) {
    if (actionString == "RESET_ALL") {
        Logger::getInstance().log("CommandFactory: Creating ResetCommand");
        return std::unique_ptr<ResetCommand>(new ResetCommand(_specialAction));
    }
    if (actionString == "HOP_BLE_DEVICE") {
        Logger::getInstance().log("CommandFactory: Creating HopBleDeviceCommand");
        return std::unique_ptr<HopBleDeviceCommand>(new HopBleDeviceCommand(_specialAction));
    }
    if (actionString == "CALIBRATE_SENSOR") {
        Logger::getInstance().log("CommandFactory: Creating CalibrateSensorCommand");
        return std::unique_ptr<CalibrateSensorCommand>(new CalibrateSensorCommand(_specialAction));
    }
    if (actionString == "MEM_INFO") {
        Logger::getInstance().log("CommandFactory: Creating MemInfoCommand");
        return std::unique_ptr<MemInfoCommand>(new MemInfoCommand(_specialAction));
    }
    if (actionString == "ENTER_SLEEP") {
        Logger::getInstance().log("CommandFactory: Creating EnterSleepCommand");
        return std::unique_ptr<EnterSleepCommand>(new EnterSleepCommand(_specialAction));
    }
    if (actionString == "IR_CHECK") {
        Logger::getInstance().log("CommandFactory: Creating IrCheckCommand");
        return std::unique_ptr<IrCheckCommand>(new IrCheckCommand(_specialAction));
    }
    if (actionString == "GYROMOUSE_START") {
        Logger::getInstance().log("CommandFactory: Creating GyroMouseStartCommand");
        return std::unique_ptr<GyroMouseStartCommand>(new GyroMouseStartCommand(_gyroMouse, _macroManager));
    }
    if (actionString == "GYROMOUSE_STOP") {
        Logger::getInstance().log("CommandFactory: Creating GyroMouseStopCommand");
        return std::unique_ptr<GyroMouseStopCommand>(new GyroMouseStopCommand(_gyroMouse, _macroManager));
    }
    if (actionString == "GYROMOUSE_TOGGLE") {
        Logger::getInstance().log("CommandFactory: Creating GyroMouseToggleCommand");
        return std::unique_ptr<GyroMouseToggleCommand>(new GyroMouseToggleCommand(_gyroMouse, _macroManager));
    }
    if (actionString == "GYROMOUSE_CYCLE_SENSITIVITY") {
        Logger::getInstance().log("CommandFactory: Creating GyroMouseCycleSensitivityCommand");
        return std::unique_ptr<GyroMouseCycleSensitivityCommand>(new GyroMouseCycleSensitivityCommand(_gyroMouse));
    }
    if (actionString == "GYROMOUSE_RECENTER") {
        Logger::getInstance().log("CommandFactory: Creating GyroMouseRecenterCommand");
        return std::unique_ptr<GyroMouseRecenterCommand>(new GyroMouseRecenterCommand(_gyroMouse));
    }
    if (actionString.rfind("DELAY_", 0) == 0) {
        std::string delayStr = actionString.substr(6);
        try {
            int totalDelayMs = std::stoi(delayStr);
            Logger::getInstance().log("CommandFactory: Creating DelayCommand with delay " + String(totalDelayMs));
            return std::unique_ptr<DelayCommand>(new DelayCommand(_specialAction, totalDelayMs));
        } catch (const std::exception& e) {
            Logger::getInstance().log("CommandFactory: Error parsing DELAY_ command: " + String(e.what()));
            return nullptr;
        }
    }
    if (actionString == "FLASHLIGHT") {
        Logger::getInstance().log("CommandFactory: Creating FlashlightCommand");
        return std::unique_ptr<FlashlightCommand>(new FlashlightCommand(_specialAction));
    }
    if (actionString == "AP_MODE") {
        Logger::getInstance().log("CommandFactory: Creating ApModeCommand");
        return std::unique_ptr<ApModeCommand>(new ApModeCommand(_wifiManager, _bleController, _macroManager->getWifiConfig()));
    }
    if (actionString == "TOGGLE_BLE_WIFI") {
        Logger::getInstance().log("CommandFactory: Creating ToggleBleWifiCommand");
        return std::unique_ptr<ToggleBleWifiCommand>(new ToggleBleWifiCommand(_specialAction));
    }
    if (actionString == "TOGGLE_KEY_ORDER") {
        Logger::getInstance().log("CommandFactory: Creating ToggleKeyOrderCommand");
        return std::unique_ptr<ToggleKeyOrderCommand>(new ToggleKeyOrderCommand(_macroManager));
    }
    if (actionString == "REACTIVE_LIGHTING") {
        Logger::getInstance().log("CommandFactory: Creating ToggleReactiveLightingCommand");
        return std::unique_ptr<ToggleReactiveLightingCommand>(new ToggleReactiveLightingCommand(_inputHub));
    }
    if (actionString == "SAVE_INTERACTIVE_COLORS") {
        Logger::getInstance().log("CommandFactory: Creating SaveInteractiveColorsCommand");
        return std::unique_ptr<SaveInteractiveColorsCommand>(new SaveInteractiveColorsCommand(_inputHub));
    }
    if (actionString.rfind("SWITCH_MY_COMBO_", 0) == 0 || actionString.rfind("SWITCH_COMBO_", 0) == 0) {
        Logger::getInstance().log("CommandFactory: Creating SwitchComboCommand for " + String(actionString.c_str()));
        return std::unique_ptr<SwitchComboCommand>(new SwitchComboCommand(_macroManager, actionString));
    }
    if (actionString.rfind("S_B:", 0) == 0) {
        Logger::getInstance().log("CommandFactory: Creating BleCommand for " + String(actionString.c_str()));
        return std::unique_ptr<BleCommand>(new BleCommand(_bleController, actionString));
    }
    if (actionString.rfind("LED_BRIGHTNESS_", 0) == 0) {
        Logger::getInstance().log("CommandFactory: Creating LedBrightnessCommand for " + String(actionString.c_str()));
        return std::unique_ptr<LedBrightnessCommand>(new LedBrightnessCommand(_specialAction, actionString));
    }
    if (actionString.rfind("LED_RGB_", 0) == 0 ||
        actionString == "LED_OFF" ||
        actionString == "LED_SAVE" ||
        actionString == "LED_RESTORE" ||
        actionString == "LED_INFO") {
        Logger::getInstance().log("CommandFactory: Creating LedCommand for " + String(actionString.c_str()));
        return std::unique_ptr<LedCommand>(new LedCommand(_specialAction, actionString));
    }
    if (actionString.rfind("SEND_IR_", 0) == 0) {
        Logger::getInstance().log("CommandFactory: Creating SendIrCommand for " + String(actionString.c_str()));
        return std::unique_ptr<SendIrCommand>(new SendIrCommand(_specialAction, _macroManager, actionString));
    }
    if (actionString.rfind("SCAN_IR_DEV_", 0) == 0) {
        std::string devStr = actionString.substr(12); // After "SCAN_IR_DEV_"
        try {
            int deviceId = std::stoi(devStr);
            Logger::getInstance().log("CommandFactory: Creating ScanIrDevCommand for device ID " + String(deviceId));
            return std::unique_ptr<ScanIrDevCommand>(new ScanIrDevCommand(_specialAction, _macroManager, deviceId));
        } catch (const std::exception& e) {
            Logger::getInstance().log("CommandFactory: Error parsing SCAN_IR_DEV_ command: " + String(e.what()));
            return nullptr;
        }
    }
    if (actionString == "EXECUTE_GESTURE") {
        Logger::getInstance().log("CommandFactory: Creating ExecuteGestureCommand");
        return std::unique_ptr<ExecuteGestureCommand>(new ExecuteGestureCommand(_inputHub, _macroManager));
    }
    
    // If no command matches, return nullptr.
    // The caller will be responsible for handling this case.
    Logger::getInstance().log("CommandFactory: No command matched for action: " + String(actionString.c_str()));
    return nullptr;
}
