#ifndef MACRO_MANAGER_H
#define MACRO_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <map>
#include <functional>
#include <string>
#include <memory>
#include <ArduinoJson.h>
#include "inputDevice.h"
#include "configTypes.h"

// Forward declarations for dependency injection
class WIFIManager;
class BLEController;
class InputHub;
class GyroMouse;
class CombinationManager;
class SpecialAction;
class CommandFactory;
class Command;

#define GESTURE_HOLD_TIME 200 // Tempo di mantenimento della gesture in ms
#define COMMAND_DELAY 200 // Delay between chained commands in ms

class MacroManager
{
public:
    MacroManager();
    void begin(
        WIFIManager* wifiManager,
        BLEController* bleController,
        InputHub* inputHub,
        GyroMouse* gyroMouse,
        CombinationManager* comboManager,
        SpecialAction* specialAction,
        CommandFactory* commandFactory,
        const KeypadConfig* keypadConfig,
        const WifiConfig* wifiConfig);
    void handleInputEvent(const InputEvent &event);
    void update();
    void clearActiveKeys();
    void setUseKeyPressOrder(bool useOrder);
    bool getUseKeyPressOrder() const { return useKeyPressOrder; }
    bool reloadCombinationsFromManager(JsonObject newCombos);

    // Combo switch request system
    bool hasPendingComboSwitch();
    void getPendingComboSwitch(std::string& outPrefix, int& outSetNumber);
    void clearPendingComboSwitch();
    void setPendingComboSwitch(const std::string& prefix, int setNumber);

    // GyroMouse mode state management
    void setGyroModeActive(bool active);
    bool isGyroModeActive() const;
    void saveCurrentComboForGyro();
    void restoreSavedGyroCombo();
    bool hasSavedGyroCombo() const;

    // Action lock management
    void setActionLocked(bool locked);

    // Config getters
    const WifiConfig* getWifiConfig() const;

    // State getters for commands
    const std::string& getCurrentActivationCombo() const;

    // Configurazione delle combinazioni
    std::map<std::string, std::vector<std::string>> combinations;
    unsigned long combo_delay = 50; // Default delay in ms
    unsigned long encoder_pulse_duration = 150; // Durata dell'impulso dell'encoder in ms

private:
    // Dependencies
    WIFIManager* wifiManager;
    BLEController* bleController;
    InputHub* inputHub;
    GyroMouse* gyroMouse;
    CombinationManager* comboManager;
    SpecialAction* specialAction;
    CommandFactory* commandFactory;
    const KeypadConfig* keypadConfig;
    const WifiConfig* wifiConfig;

    // Command execution state
    std::unique_ptr<Command> _lastExecutedCommand;

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
    std::string pendingGestureFallback;
    std::string lastAction;
    std::string lastExecutedAction;
    std::string currentActivationCombo; // Combo che ha attivato l'azione corrente
    std::string encoderPendingAction; // Azione dell'encoder in attesa di rilascio
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
    bool executeCombinationActions(const std::string &comboKey);
    void releaseGestureActions();

    // Pending combo switch request
    bool pendingComboSwitchFlag = false;
    std::string pendingComboPrefix;
    int pendingComboSetNumber = 0;

    // Gyro mouse mode tracking
    bool gyroModeActive = false;
    bool hasSavedCombo = false;
    std::string savedComboPrefix;
    int savedComboSetNumber = 0;
};

#endif // MACRO_MANAGER_H
