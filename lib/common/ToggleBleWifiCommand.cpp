#include "ToggleBleWifiCommand.h"
#include "Logger.h"

ToggleBleWifiCommand::ToggleBleWifiCommand(SpecialAction* specialAction)
    : _specialAction(specialAction) {}

void ToggleBleWifiCommand::press() {
    // No action on press for this command, as it's typically a toggle on release
    Logger::getInstance().log("ToggleBleWifiCommand: Press (no action)");
}

void ToggleBleWifiCommand::release() {
    if (_specialAction) {
        _specialAction->toggleBleWifi();
        Logger::getInstance().log("ToggleBleWifiCommand: Toggled BLE/WiFi");
    } else {
        Logger::getInstance().log("ToggleBleWifiCommand: SpecialAction is null");
    }
}