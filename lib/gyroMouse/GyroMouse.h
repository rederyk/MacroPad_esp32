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

    struct Quaternion
    {
        float w;
        float x;
        float y;
        float z;

        Quaternion() : w(1.0f), x(0.0f), y(0.0f), z(0.0f) {}
        Quaternion(float w_, float x_, float y_, float z_) : w(w_), x(x_), y(y_), z(z_) {}

        void normalize();
        Quaternion conjugate() const;
        Quaternion multiply(const Quaternion& q) const;
        void rotateVector(float& vx, float& vy, float& vz) const;
        void toEuler(float& pitch, float& roll, float& yaw) const;
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
    float residualMouseX;
    float residualMouseY;
    float gyroBiasX;
    float gyroBiasY;
    float gyroBiasZ;
    unsigned long lastUpdateTime;
    float fusedPitchRad;
    float fusedRollRad;
    float neutralPitchRad;
    float neutralRollRad;
    bool neutralCaptured;

    // Quaternion-based orientation tracking
    Quaternion currentOrientation;
    Quaternion neutralOrientation;
    Quaternion lastOrientation;

    // Adaptive filtering
    float gyroNoiseEstimate;
    float adaptiveSmoothingFactor;
    float velocityMagnitude;
    unsigned long lastMotionTime;

    // Madgwick filter parameters
    float madgwickBeta;  // Filter gain (inversely proportional to noise)
    float madgwickSampleFreq; // Sample frequency in Hz

    // Helper
    void calculateMouseMovement(const SensorFrame& frame, float deltaTime,
                               int8_t& mouseX, int8_t& mouseY);
    void updateOrientation(const SensorFrame& frame, float deltaTime);
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
    void updateAdaptiveFiltering(const SensorFrame& frame, float deltaTime);

    // New quaternion-based methods
    Quaternion createQuaternionFromGyro(const SensorFrame& frame, float deltaTime) const;
    void updateQuaternionOrientation(const SensorFrame& frame, float deltaTime);
    void calculateMouseMovementFromRelativeRotation(const SensorFrame& frame, float deltaTime,
                                                     int8_t& mouseX, int8_t& mouseY);
    void extractLocalAngularVelocity(float& localPitch, float& localYaw) const;

    // Madgwick filter implementation
    void madgwickUpdate(float gx, float gy, float gz, float ax, float ay, float az, float deltaTime);
    void updateMadgwickBeta(); // Adaptive beta based on motion

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
