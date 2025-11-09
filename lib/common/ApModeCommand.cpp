#include "ApModeCommand.h"
#include "WIFIManager.h"
#include "BLEController.h"
#include "Logger.h"

ApModeCommand::ApModeCommand(WIFIManager* wifiManager, BLEController* bleController, const WifiConfig* wifiConfig)
    : _wifiManager(wifiManager), _bleController(bleController), _wifiConfig(wifiConfig) {}

void ApModeCommand::press() {
    if (!_wifiManager || !_bleController || !_wifiConfig) return;

    if (!_bleController->isBleEnabled()) {
        _wifiManager->toggleAp(_wifiConfig->ap_ssid.c_str(), _wifiConfig->ap_password.c_str());
    } else {
        Logger::getInstance().log("riavvia in WIFImode");
    }
}

void ApModeCommand::release() {
    // No action on release
}
