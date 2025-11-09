#ifndef AP_MODE_COMMAND_H
#define AP_MODE_COMMAND_H

#include "Command.h"
#include "configTypes.h"

class WIFIManager;
class BLEController;

class ApModeCommand : public Command {
public:
    ApModeCommand(WIFIManager* wifiManager, BLEController* bleController, const WifiConfig* wifiConfig);
    void press() override;
    void release() override;

private:
    WIFIManager* _wifiManager;
    BLEController* _bleController;
    const WifiConfig* _wifiConfig;
};

#endif // AP_MODE_COMMAND_H
