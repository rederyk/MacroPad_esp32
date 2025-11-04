#ifndef GYROMOUSE_H
#define GYROMOUSE_H

#include <Arduino.h>
#include "gestureRead.h"
#include "configTypes.h"

class GyroMouse {
public:
    GyroMouse();
    ~GyroMouse();

    // Inizializzazione
    bool begin(GestureRead* sensor, const GyroMouseConfig& config);

    // Controllo stato
    void start();
    void stop();
    bool isRunning() const { return active; }

    // Update loop (chiamato da main)
    void update();

    // Gestione sensibilità
    void cycleSensitivity();
    uint8_t getCurrentSensitivity() const { return currentSensitivityIndex; }
    String getSensitivityName() const;

    // Getters
    const GyroMouseConfig& getConfig() const { return config; }

private:
    // Riferimenti esterni
    GestureRead* gestureSensor;

    // Stato
    bool active;
    uint8_t currentSensitivityIndex;
    GyroMouseConfig config;
    bool ownsSampling;
    bool gestureCaptureSuspended;

    // Variabili movimento
    float smoothedMouseX;
    float smoothedMouseY;
    unsigned long lastUpdateTime;

    // Helper
    void calculateMouseMovement(float gyroX, float gyroY, float deltaTime,
                               int8_t& mouseX, int8_t& mouseY);
    float applyDeadzone(float value, float threshold);
    int8_t clampMouseValue(float value);
};

#endif // GYROMOUSE_H
