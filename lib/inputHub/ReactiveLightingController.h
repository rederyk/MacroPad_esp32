#ifndef REACTIVE_LIGHTING_CONTROLLER_H
#define REACTIVE_LIGHTING_CONTROLLER_H

#include <Arduino.h>
#include <array>
#include <vector>

struct ComboSettings;

class ReactiveLightingController
{
public:
    void enable(bool enable);
    bool isEnabled() const { return state.enabled; }

    void handleInput(uint8_t keyIndex, bool isEncoder, int encoderDirection, uint16_t activeKeysMask);
    void update();

    void updateColors(const ComboSettings &settings);
    void saveColors() const;

private:
    struct State
    {
        bool enabled = false;
        std::vector<std::array<int, 3>> keyColors;
        int savedLedColor[3] = {0, 0, 0};
        unsigned long ledReactiveTime = 0;
        bool ledReactiveActive = false;

        bool editMode = false;
        uint8_t selectedKey = 0;
        uint8_t selectedChannel = 0;
        unsigned long lastChannelSwitchTime = 0;

        int baseBrightness = 255;
    } state;

    static constexpr unsigned long LED_REACTIVE_DURATION = 300;
    static constexpr unsigned long CHANNEL_SWITCH_DEBOUNCE = 200;

    void ensureKeyColor(size_t keyIndex);
    std::array<int, 3> generateDefaultKeyColor(size_t keyIndex, size_t totalKeys) const;
    void applyColorWithBrightness(int r, int g, int b) const;
    const char *getChannelName(uint8_t channel) const;
};

#endif // REACTIVE_LIGHTING_CONTROLLER_H
