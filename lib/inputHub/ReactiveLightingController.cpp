#include "ReactiveLightingController.h"

#include <algorithm>
#include <cmath>

#include "Logger.h"
#include "Led.h"
#include "combinationManager.h"

void ReactiveLightingController::enable(bool enable)
{
    state.enabled = enable;

    if (enable)
    {
        Led::getInstance().getColor(
            state.savedLedColor[0],
            state.savedLedColor[1],
            state.savedLedColor[2]);

        if (state.keyColors.empty())
        {
            for (size_t i = 0; i < 9; i++)
            {
                state.keyColors.push_back(generateDefaultKeyColor(i, 9));
            }
        }

        Logger::getInstance().log("Interactive Lighting ENABLED - Use keys to show colors, encoder to adjust brightness");
        Logger::getInstance().log("Hold a key + rotate encoder to edit that key's color");
    }
    else
    {
        if (state.ledReactiveActive)
        {
            Led::getInstance().setColor(
                state.savedLedColor[0],
                state.savedLedColor[1],
                state.savedLedColor[2],
                false);
            state.ledReactiveActive = false;
        }
        state.editMode = false;
        Logger::getInstance().log("Interactive Lighting DISABLED");
    }
}

void ReactiveLightingController::handleInput(uint8_t keyIndex, bool isEncoder, int encoderDirection, uint16_t activeKeysMask)
{
    if (!state.enabled)
    {
        return;
    }

    if (!state.ledReactiveActive)
    {
        Led::getInstance().getColor(
            state.savedLedColor[0],
            state.savedLedColor[1],
            state.savedLedColor[2]);
    }

    if (isEncoder)
    {
        if (activeKeysMask == 0)
        {
            if (encoderDirection != 0)
            {
                const int step = 15;
                const int oldBrightness = state.baseBrightness;

                if (encoderDirection > 0)
                {
                    state.baseBrightness = std::min(255, state.baseBrightness + step);
                }
                else
                {
                    state.baseBrightness = std::max(0, state.baseBrightness - step);
                }

                if (oldBrightness != state.baseBrightness)
                {
                    Logger::getInstance().log("Interactive Brightness: " + String(state.baseBrightness));

                    applyColorWithBrightness(255, 255, 255);
                    state.ledReactiveActive = true;
                    state.ledReactiveTime = millis() + LED_REACTIVE_DURATION;
                }
            }
        }
        else
        {
            if (encoderDirection != 0)
            {
                if (!state.editMode)
                {
                    state.editMode = true;

                    for (uint8_t i = 0; i < 16; i++)
                    {
                        if (activeKeysMask & (1 << i))
                        {
                            state.selectedKey = i;
                            break;
                        }
                    }
                }

                const uint8_t selectedKey = state.selectedKey;
                ensureKeyColor(selectedKey);

                auto &color = state.keyColors[selectedKey];
                const uint8_t channel = state.selectedChannel;

                const int step = 10;
                const int oldValue = color[channel];

                if (encoderDirection > 0)
                {
                    color[channel] = std::min(255, color[channel] + step);
                }
                else
                {
                    color[channel] = std::max(0, color[channel] - step);
                }

                if (oldValue != color[channel])
                {
                    Logger::getInstance().log(
                        "Key " + String(selectedKey) + " " +
                        String(getChannelName(channel)) + ": " +
                        String(color[channel]));

                    applyColorWithBrightness(color[0], color[1], color[2]);
                    state.ledReactiveActive = true;
                    state.ledReactiveTime = millis() + 2000;
                }
            }
            else
            {
                const unsigned long currentTime = millis();
                if (currentTime - state.lastChannelSwitchTime > CHANNEL_SWITCH_DEBOUNCE)
                {
                    state.editMode = true;
                    state.selectedChannel = (state.selectedChannel + 1) % 3;
                    state.lastChannelSwitchTime = currentTime;

                    Logger::getInstance().log(
                        "Editing channel: " + String(getChannelName(state.selectedChannel)));

                    int flashColor[3] = {0, 0, 0};
                    flashColor[state.selectedChannel] = 255;
                    applyColorWithBrightness(flashColor[0], flashColor[1], flashColor[2]);

                    state.ledReactiveActive = true;
                    state.ledReactiveTime = millis() + 400;
                }
            }
        }
    }
    else
    {
        ensureKeyColor(keyIndex);

        const auto &color = state.keyColors[keyIndex];
        applyColorWithBrightness(color[0], color[1], color[2]);

        state.ledReactiveActive = true;
        state.ledReactiveTime = millis() + LED_REACTIVE_DURATION;

        if (activeKeysMask == 0)
        {
            state.editMode = false;
        }
    }
}

void ReactiveLightingController::update()
{
    if (!state.ledReactiveActive)
    {
        return;
    }

    const unsigned long currentTime = millis();
    if (currentTime >= state.ledReactiveTime)
    {
        Led::getInstance().setColor(
            state.savedLedColor[0],
            state.savedLedColor[1],
            state.savedLedColor[2],
            false);
        state.ledReactiveActive = false;
        state.editMode = false;
    }
}

void ReactiveLightingController::updateColors(const ComboSettings &settings)
{
    if (settings.hasInteractiveColors())
    {
        state.keyColors = settings.interactiveColors;
        Logger::getInstance().log("Loaded " + String(state.keyColors.size()) +
                                  " interactive colors from combo settings");
    }
    else
    {
        state.keyColors.clear();
        for (size_t i = 0; i < 9; i++)
        {
            state.keyColors.push_back(generateDefaultKeyColor(i, 9));
        }
        Logger::getInstance().log("Using default interactive colors (9 keys)");
    }
}

void ReactiveLightingController::saveColors() const
{
    Logger::getInstance().log("SAVE_INTERACTIVE_COLORS command received");
    Logger::getInstance().log("Note: Auto-save to JSON not yet implemented");
    Logger::getInstance().log("Current colors:");

    for (size_t i = 0; i < state.keyColors.size(); i++)
    {
        const auto &color = state.keyColors[i];
        Logger::getInstance().log(
            "  Key " + String(i) + ": RGB(" +
            String(color[0]) + "," +
            String(color[1]) + "," +
            String(color[2]) + ")");
    }
}

void ReactiveLightingController::ensureKeyColor(size_t keyIndex)
{
    while (state.keyColors.size() <= keyIndex)
    {
        state.keyColors.push_back(generateDefaultKeyColor(state.keyColors.size(), 9));
    }
}

std::array<int, 3> ReactiveLightingController::generateDefaultKeyColor(size_t keyIndex, size_t totalKeys) const
{
    if (totalKeys == 0)
    {
        totalKeys = 1;
    }

    const float hue = static_cast<float>(keyIndex * 360) / static_cast<float>(totalKeys);

    float saturation = 1.0f;
    float value = 1.0f;

    if (totalKeys > 12)
    {
        saturation = (keyIndex % 2 == 0) ? 1.0f : 0.85f;
        value = 0.8f + (0.2f * (keyIndex % 3) / 2.0f);
    }

    const float c = value * saturation;
    const float x = c * (1.0f - fabs(fmod(hue / 60.0f, 2.0f) - 1.0f));
    const float m = value - c;

    float r, g, b;
    if (hue < 60)
    {
        r = c;
        g = x;
        b = 0;
    }
    else if (hue < 120)
    {
        r = x;
        g = c;
        b = 0;
    }
    else if (hue < 180)
    {
        r = 0;
        g = c;
        b = x;
    }
    else if (hue < 240)
    {
        r = 0;
        g = x;
        b = c;
    }
    else if (hue < 300)
    {
        r = x;
        g = 0;
        b = c;
    }
    else
    {
        r = c;
        g = 0;
        b = x;
    }

    return {
        static_cast<int>((r + m) * 255),
        static_cast<int>((g + m) * 255),
        static_cast<int>((b + m) * 255)};
}

void ReactiveLightingController::applyColorWithBrightness(int r, int g, int b) const
{
    const float brightnessFactor = state.baseBrightness / 255.0f;
    const int adjustedR = static_cast<int>(r * brightnessFactor);
    const int adjustedG = static_cast<int>(g * brightnessFactor);
    const int adjustedB = static_cast<int>(b * brightnessFactor);

    Led::getInstance().setColor(adjustedR, adjustedG, adjustedB, false);
}

const char *ReactiveLightingController::getChannelName(uint8_t channel) const
{
    switch (channel)
    {
    case 0:
        return "RED";
    case 1:
        return "GREEN";
    case 2:
        return "BLUE";
    default:
        return "UNKNOWN";
    }
}
