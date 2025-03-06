#ifndef CONFIG_TYPES_H
#define CONFIG_TYPES_H

#include <Arduino.h>
#include <vector>

struct KeypadConfig
{
    byte rows;
    byte cols;
    std::vector<byte> rowPins;
    std::vector<byte> colPins;
    std::vector<std::vector<char>> keys;
    bool invertDirection;
};

struct EncoderConfig
{
    byte pinA;
    byte pinB;
    byte buttonPin;
    int stepValue;
};

struct AccelerometerConfig
{

    byte sdaPin;
    byte sclPin;
    float sensitivity;
    int sampleRate;
    int threshold;
    String axisMap;
};

struct WifiConfig
{
    String ap_ssid;
    String ap_password;
    String router_ssid;
    String router_password;
};

struct SystemConfig
{
    bool ap_autostart;
    bool router_autostart;
    bool enable_BLE;
    bool serial_enabled;
    int BleMacAdd;
    int combo_timeout;
    String BleName;
    // Nuovi campi per la gestione del power
    bool sleep_enabled;             // Abilitare il sleep mode
    unsigned long sleep_timeout_ms; // Timeout di inattività in millisecondi
    gpio_num_t wakeup_pin;                 // Pin GPIO per il wakeup
    gpio_num_t wakeup_pin2;                 // altro Pin GPIO per il wakeup
};

#endif
