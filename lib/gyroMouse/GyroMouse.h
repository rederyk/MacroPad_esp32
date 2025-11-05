#ifndef GYROMOUSE_H
#define GYROMOUSE_H

#include <Arduino.h>
#include "gestureRead.h"
#include "configTypes.h"
#include "SensorFusion.h"

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
    float residualMouseX;
    float residualMouseY;
    unsigned long lastUpdateTime;

    // Click stabilization
    float clickSlowdownFactor;
    unsigned long lastClickCheckTime;

    // Sensor Fusion
    SensorFusion fusion;

    // Helper
    void calculateMouseMovement(const SensorFrame& frame, float deltaTime,
                               int8_t& mouseX, int8_t& mouseY);
    void updateNeutralBaseline(float deltaTime, const SensorFrame& frame);
    void rotateVectorByNeutral(float& x, float& y, float& z) const;
    float applyDeadzone(float value, float threshold);
    float applyDynamicDeadzone(float value, float baseThreshold, float noiseFactor);
    float applySmoothCurve(float value, float deadzone, float maxValue);
    int8_t clampMouseValue(float pending, float& residual);
    void beginNeutralCapture();
    void accumulateNeutralCapture(float pitchAcc, float rollAcc, const SensorFrame& frame);
    void performAbsoluteCentering();
    void dispatchRelativeMove(int deltaX, int deltaY);

    // Click stabilization
    void updateClickSlowdown();

    // Member variables
    bool neutralCapturePending;
    uint16_t neutralCaptureSamples;
    float neutralPitchAccum;
    float neutralRollAccum;
    float gyroBiasAccumX;
    float gyroBiasAccumY;
    float gyroBiasAccumZ;
};

#endif // GYROMOUSE_H
