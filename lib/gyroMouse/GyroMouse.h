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
    void recenterNeutral();
    uint8_t getCurrentSensitivity() const { return currentSensitivityIndex; }
    String getSensitivityName() const;

    // Getters
    const GyroMouseConfig& getConfig() const { return config; }

private:
    enum class PointingMode : uint8_t
    {
        GyroRate,
        TiltVelocity,
        Hybrid
    };

    struct SensorFrame
    {
        float gyroX;
        float gyroY;
        float gyroZ;
        float accelX;
        float accelY;
        float accelZ;
        float accelMagnitude;
    };

    // Riferimenti esterni
    GestureRead* gestureSensor;

    // Stato
    bool active;
    uint8_t currentSensitivityIndex;
    GyroMouseConfig config;
    bool ownsSampling;
    bool gestureCaptureSuspended;
    bool gyroAvailable;

    // Variabili movimento
    float smoothedMouseX;
    float smoothedMouseY;
    unsigned long lastUpdateTime;
    float fusedPitchRad;
    float fusedRollRad;
    float neutralPitchRad;
    float neutralRollRad;
    bool neutralCaptured;

    // Helper
    void calculateMouseMovement(const SensorFrame& frame, float deltaTime,
                               int8_t& mouseX, int8_t& mouseY);
    void updateOrientation(const SensorFrame& frame, float deltaTime);
    void updateNeutralBaseline(float deltaTime, const SensorFrame& frame);
    PointingMode resolvePointingMode(const SensitivitySettings& settings) const;
    void applyNeutralOrientationRotation(float& x, float& y, float& z) const;
    float applyDeadzone(float value, float threshold);
    int8_t clampMouseValue(float value);
    void performAbsoluteCentering();
    void dispatchRelativeMove(int deltaX, int deltaY);
};

#endif // GYROMOUSE_H
