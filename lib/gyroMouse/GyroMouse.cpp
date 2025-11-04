#include "GyroMouse.h"
#include "Logger.h"
#include "BLEController.h"
#include "InputHub.h"

#include <cmath>

namespace
{
    constexpr float kRateScaleFactor = 0.5f * 100.0f; // Empirical °/s -> px factor
    constexpr float kRadToDeg = 57.2957795f;
    constexpr float kDegToRad = 1.0f / kRadToDeg;
    constexpr float kAccelReliableMin = 0.25f;   // g
    constexpr float kAccelReliableMax = 1.85f;   // g
    constexpr float kGyroQuietThreshold = 0.15f; // rad/s
}

// Riferimento esterno al BLE controller
extern BLEController bleController;
extern InputHub inputHub;

GyroMouse::GyroMouse()
    : gestureSensor(nullptr),
      active(false),
      currentSensitivityIndex(1),
      config(),
      ownsSampling(false),
      gestureCaptureSuspended(false),
      gyroAvailable(false),
      smoothedMouseX(0.0f),
      smoothedMouseY(0.0f),
      lastUpdateTime(0),
      fusedPitchRad(0.0f),
      fusedRollRad(0.0f),
      neutralPitchRad(0.0f),
      neutralRollRad(0.0f),
      neutralCaptured(false) {
}

GyroMouse::~GyroMouse() {
    stop();
}

bool GyroMouse::begin(GestureRead* sensor, const GyroMouseConfig& cfg) {
    if (!sensor) {
        Logger::getInstance().log("GyroMouse: Invalid sensor pointer");
        return false;
    }

    gestureSensor = sensor;
    config = cfg;
    config.smoothing = constrain(config.smoothing, 0.0f, 1.0f);
    config.orientationAlpha = constrain(config.orientationAlpha, 0.0f, 0.999f);
    if (config.orientationAlpha <= 0.0f) {
        config.orientationAlpha = 0.96f;
    }
    if (config.tiltLimitDegrees <= 0.0f) {
        config.tiltLimitDegrees = 55.0f;
    }
    config.tiltLimitDegrees = constrain(config.tiltLimitDegrees, 5.0f, 90.0f);
    if (config.tiltDeadzoneDegrees < 0.0f) {
        config.tiltDeadzoneDegrees = 0.0f;
    }
    if (config.tiltDeadzoneDegrees == 0.0f) {
        config.tiltDeadzoneDegrees = 1.5f;
    }
    config.tiltDeadzoneDegrees = constrain(config.tiltDeadzoneDegrees, 0.0f, 15.0f);
    config.recenterRate = constrain(config.recenterRate, 0.0f, 1.0f);
    if (config.recenterThresholdDegrees <= 0.0f) {
        config.recenterThresholdDegrees = 2.0f;
    }
    config.recenterThresholdDegrees = constrain(config.recenterThresholdDegrees, 0.1f, 20.0f);

    for (auto &sens : config.sensitivities) {
        if (sens.mode.length() == 0) {
            sens.mode = "gyro";
        }
        if (sens.gyroScale <= 0.0f) {
            sens.gyroScale = sens.scale > 0.0f ? sens.scale : 1.0f;
        }
        if (sens.tiltScale <= 0.0f) {
            sens.tiltScale = (sens.scale > 0.0f ? sens.scale : 1.0f) * 20.0f;
        }
        if (sens.tiltDeadzone < 0.0f) {
            sens.tiltDeadzone = config.tiltDeadzoneDegrees;
        }
        if (sens.tiltDeadzone == 0.0f) {
            sens.tiltDeadzone = config.tiltDeadzoneDegrees;
        }
        sens.hybridBlend = constrain(sens.hybridBlend, 0.0f, 1.0f);
    }

    active = false;
    ownsSampling = false;
    gestureCaptureSuspended = false;

    // Valida configurazione
    if (config.sensitivities.empty()) {
        Logger::getInstance().log("GyroMouse: No sensitivity settings defined");
        return false;
    }

    // Imposta sensibilità default
    if (config.defaultSensitivity < config.sensitivities.size()) {
        currentSensitivityIndex = config.defaultSensitivity;
    } else {
        currentSensitivityIndex = 0;
        Logger::getInstance().log("GyroMouse: Invalid default sensitivity, using 0");
    }

    Logger::getInstance().log("GyroMouse: Initialized with " +
                             String(config.sensitivities.size()) + " sensitivity modes");
    return true;
}

void GyroMouse::start() {
    if (!config.enabled) {
        Logger::getInstance().log("GyroMouse: Disabled in config");
        return;
    }

    if (!gestureSensor) {
        Logger::getInstance().log("GyroMouse: Sensor not available");
        return;
    }

    if (!bleController.isBleEnabled()) {
        Logger::getInstance().log("GyroMouse: BLE disabled, cannot start");
        return;
    }

    if (active) {
        Logger::getInstance().log("GyroMouse: Already active");
        return;
    }

    gestureSensor->setStreamingMode(true);

    // Suspend gesture capture pipeline if active
    if (inputHub.isGestureCaptureEnabled()) {
        inputHub.setGestureCaptureEnabled(false);
        gestureCaptureSuspended = true;
    } else {
        gestureCaptureSuspended = false;
    }

    ownsSampling = false;
    if (!gestureSensor->isSampling()) {
        if (!gestureSensor->startSampling()) {
            Logger::getInstance().log("GyroMouse: Failed to start sensor sampling");
            if (gestureCaptureSuspended) {
                inputHub.setGestureCaptureEnabled(true);
                gestureCaptureSuspended = false;
            }
            gestureSensor->setStreamingMode(false);
            return;
        }
        ownsSampling = true;
    }

    gestureSensor->clearMemory();

    gyroAvailable = gestureSensor->getMotionSensor() &&
                    gestureSensor->getMotionSensor()->hasGyro();
    fusedPitchRad = 0.0f;
    fusedRollRad = 0.0f;
    neutralPitchRad = 0.0f;
    neutralRollRad = 0.0f;
    neutralCaptured = false;

    // Reset stato movimento
    smoothedMouseX = 0.0f;
    smoothedMouseY = 0.0f;
    lastUpdateTime = millis();

    active = true;

    String sensName = config.sensitivities[currentSensitivityIndex].name;
    Logger::getInstance().log("GyroMouse: Started (sensitivity: " + sensName + ")");
}

void GyroMouse::stop() {
    if (!active) {
        return;
    }

    active = false;
    smoothedMouseX = 0.0f;
    smoothedMouseY = 0.0f;
    gyroAvailable = false;
    neutralCaptured = false;
    fusedPitchRad = 0.0f;
    fusedRollRad = 0.0f;
    neutralPitchRad = 0.0f;
    neutralRollRad = 0.0f;

    if (ownsSampling && gestureSensor) {
        gestureSensor->ensureMinimumSamplingTime();
        gestureSensor->stopSampling();
        gestureSensor->clearMemory();
        gestureSensor->flushSensorBuffer();
    }
    ownsSampling = false;

    if (gestureCaptureSuspended) {
        inputHub.setGestureCaptureEnabled(true);
        gestureCaptureSuspended = false;
    }

    if (gestureSensor) {
        gestureSensor->setStreamingMode(false);
    }

    Logger::getInstance().log("GyroMouse: Stopped");
}

void GyroMouse::update() {
    if (!active || !gestureSensor) {
        return;
    }

    // Calcola delta time
    unsigned long currentTime = millis();
    float deltaTime = (currentTime - lastUpdateTime) / 1000.0f; // Converti in secondi
    lastUpdateTime = currentTime;

    // Evita delta troppo grandi (primo update o overflow)
    if (deltaTime > 0.1f || deltaTime <= 0.0f) {
        deltaTime = 0.005f; // Assume 200Hz
    }

    SensorFrame frame{};
    frame.gyroX = gestureSensor->getMappedGyroX();
    frame.gyroY = gestureSensor->getMappedGyroY();
    frame.gyroZ = gestureSensor->getMappedGyroZ();
    frame.accelX = gestureSensor->getMappedX();
    frame.accelY = gestureSensor->getMappedY();
    frame.accelZ = gestureSensor->getMappedZ();
    frame.accelMagnitude = sqrtf(frame.accelX * frame.accelX +
                                 frame.accelY * frame.accelY +
                                 frame.accelZ * frame.accelZ);

    updateOrientation(frame, deltaTime);
    updateNeutralBaseline(deltaTime, frame);

    // Calcola movimento mouse
    int8_t mouseX = 0;
    int8_t mouseY = 0;
    calculateMouseMovement(frame, deltaTime, mouseX, mouseY);

    // Invia movimento se non nullo
    if (mouseX != 0 || mouseY != 0) {
        bleController.moveMouse(mouseX, mouseY, 0, 0);
    }
}

void GyroMouse::calculateMouseMovement(const SensorFrame& frame, float deltaTime,
                                       int8_t& mouseX, int8_t& mouseY) {
    if (config.sensitivities.empty()) {
        mouseX = 0;
        mouseY = 0;
        return;
    }

    const SensitivitySettings& sens = config.sensitivities[currentSensitivityIndex];
    const float rateScale = sens.gyroScale > 0.0f ? sens.gyroScale : sens.scale;
    const float tiltScale = sens.tiltScale > 0.0f ? sens.tiltScale : rateScale;
    float tiltDeadzone = sens.tiltDeadzone > 0.0f ? sens.tiltDeadzone : config.tiltDeadzoneDegrees;
    tiltDeadzone = tiltDeadzone < 0.0f ? 0.0f : tiltDeadzone;

    PointingMode mode = resolvePointingMode(sens);

    float rawMouseX = 0.0f;
    float rawMouseY = 0.0f;

    auto computeRateComponent = [&](float& outX, float& outY) {
        float gyroX = frame.gyroX;
        float gyroY = frame.gyroY;
        float gyroZ = frame.gyroZ;

        applyNeutralOrientationRotation(gyroX, gyroY, gyroZ);

        gyroX *= kRadToDeg;
        gyroY *= kRadToDeg;

        gyroX = applyDeadzone(gyroX, sens.deadzone);
        gyroY = applyDeadzone(gyroY, sens.deadzone);
        outX = gyroX * rateScale * deltaTime * kRateScaleFactor;
        outY = gyroY * rateScale * deltaTime * kRateScaleFactor;
    };

    auto computeTiltComponent = [&](float& outX, float& outY) {
        float tiltXDeg = (fusedRollRad - neutralRollRad) * kRadToDeg;
        float tiltYDeg = (fusedPitchRad - neutralPitchRad) * kRadToDeg;

        tiltXDeg = applyDeadzone(tiltXDeg, tiltDeadzone);
        tiltYDeg = applyDeadzone(tiltYDeg, tiltDeadzone);

        float clampedLimit = config.tiltLimitDegrees;
        tiltXDeg = constrain(tiltXDeg, -clampedLimit, clampedLimit);
        tiltYDeg = constrain(tiltYDeg, -clampedLimit, clampedLimit);

        outX = tiltXDeg * tiltScale * deltaTime;
        outY = tiltYDeg * tiltScale * deltaTime;
    };

    switch (mode) {
        case PointingMode::GyroRate: {
            computeRateComponent(rawMouseX, rawMouseY);
            break;
        }
        case PointingMode::TiltVelocity: {
            computeTiltComponent(rawMouseX, rawMouseY);
            break;
        }
        case PointingMode::Hybrid: {
            float rateX, rateY;
            float tiltX, tiltY;
            computeRateComponent(rateX, rateY);
            computeTiltComponent(tiltX, tiltY);
            float blend = constrain(sens.hybridBlend, 0.0f, 1.0f);
            rawMouseX = rateX * (1.0f - blend) + tiltX * blend;
            rawMouseY = rateY * (1.0f - blend) + tiltY * blend;
            break;
        }
    }

    // Applica inversioni
    if (config.invertX) rawMouseX = -rawMouseX;
    if (config.invertY) rawMouseY = -rawMouseY;

    // Swap assi se richiesto
    if (config.swapAxes) {
        float temp = rawMouseX;
        rawMouseX = rawMouseY;
        rawMouseY = temp;
    }

    // Applica smoothing esponenziale
    smoothedMouseX = smoothedMouseX * (1.0f - config.smoothing) + rawMouseX * config.smoothing;
    smoothedMouseY = smoothedMouseY * (1.0f - config.smoothing) + rawMouseY * config.smoothing;

    // Clamp a range BLE HID [-127, 127]
    mouseX = clampMouseValue(smoothedMouseX);
    mouseY = clampMouseValue(smoothedMouseY);
}

GyroMouse::PointingMode GyroMouse::resolvePointingMode(const SensitivitySettings& settings) const {
    if (settings.mode.length() == 0) {
        return PointingMode::GyroRate;
    }

    if (settings.mode.equalsIgnoreCase("tilt") ||
        settings.mode.equalsIgnoreCase("tilt_velocity") ||
        settings.mode.equalsIgnoreCase("tilt-velocity") ||
        settings.mode.equalsIgnoreCase("tiltvelocity")) {
        return PointingMode::TiltVelocity;
    }

    if (settings.mode.equalsIgnoreCase("hybrid") ||
        settings.mode.equalsIgnoreCase("fusion") ||
        settings.mode.equalsIgnoreCase("blend")) {
        return PointingMode::Hybrid;
    }

    return PointingMode::GyroRate;
}

void GyroMouse::updateOrientation(const SensorFrame& frame, float deltaTime) {
    float predictedPitch = fusedPitchRad;
    float predictedRoll = fusedRollRad;

    if (gyroAvailable && deltaTime > 0.0f) {
        predictedPitch += frame.gyroY * deltaTime;
        predictedRoll += frame.gyroX * deltaTime;
    }

    bool accelReliable = (frame.accelMagnitude > kAccelReliableMin) &&
                         (frame.accelMagnitude < kAccelReliableMax);

    if (accelReliable) {
        float pitchAcc = atan2f(-frame.accelX, sqrtf(frame.accelY * frame.accelY + frame.accelZ * frame.accelZ));
        float rollAcc = atan2f(frame.accelY, frame.accelZ);

        if (!neutralCaptured) {
            fusedPitchRad = pitchAcc;
            fusedRollRad = rollAcc;
            neutralPitchRad = pitchAcc;
            neutralRollRad = rollAcc;
            neutralCaptured = true;
            return;
        }

        float alpha = config.orientationAlpha;
        if (!gyroAvailable) {
            fusedPitchRad = fusedPitchRad * alpha + pitchAcc * (1.0f - alpha);
            fusedRollRad = fusedRollRad * alpha + rollAcc * (1.0f - alpha);
        } else {
            fusedPitchRad = alpha * predictedPitch + (1.0f - alpha) * pitchAcc;
            fusedRollRad = alpha * predictedRoll + (1.0f - alpha) * rollAcc;
        }
    } else {
        fusedPitchRad = predictedPitch;
        fusedRollRad = predictedRoll;
    }

    const float limitRad = config.tiltLimitDegrees * kDegToRad * 2.0f;
    fusedPitchRad = constrain(fusedPitchRad, -limitRad, limitRad);
    fusedRollRad = constrain(fusedRollRad, -limitRad, limitRad);
}

void GyroMouse::updateNeutralBaseline(float deltaTime, const SensorFrame& frame) {
    if (!neutralCaptured || config.recenterRate <= 0.0f) {
        return;
    }

    float tiltXDeg = (fusedRollRad - neutralRollRad) * kRadToDeg;
    float tiltYDeg = (fusedPitchRad - neutralPitchRad) * kRadToDeg;

    float threshold = config.recenterThresholdDegrees;
    if (fabsf(tiltXDeg) > threshold || fabsf(tiltYDeg) > threshold) {
        return;
    }

    if (fabsf(frame.gyroX) > kGyroQuietThreshold || fabsf(frame.gyroY) > kGyroQuietThreshold) {
        return;
    }

    float gain = config.recenterRate * deltaTime;
    gain = constrain(gain, 0.0f, 0.2f);

    neutralRollRad += (fusedRollRad - neutralRollRad) * gain;
    neutralPitchRad += (fusedPitchRad - neutralPitchRad) * gain;
}

void GyroMouse::recenterNeutral() {
    if (!gestureSensor) {
        Logger::getInstance().log("GyroMouse: recenter ignored (sensor unavailable)");
        return;
    }

    performAbsoluteCentering();

    if (!neutralCaptured) {
        Logger::getInstance().log("GyroMouse: capturing initial neutral orientation");
    }

    neutralPitchRad = fusedPitchRad;
    neutralRollRad = fusedRollRad;
    neutralCaptured = true;
    smoothedMouseX = 0.0f;
    smoothedMouseY = 0.0f;
    lastUpdateTime = millis();

    Logger::getInstance().log("GyroMouse: Neutral orientation recentered");
}

void GyroMouse::applyNeutralOrientationRotation(float& x, float& y, float& z) const {
    if (!neutralCaptured) {
        return;
    }

    float sinPitch = sinf(neutralPitchRad);
    float cosPitch = cosf(neutralPitchRad);
    float sinRoll = sinf(neutralRollRad);
    float cosRoll = cosf(neutralRollRad);

    float x1 = cosPitch * x - sinPitch * z;
    float y1 = y;
    float z1 = sinPitch * x + cosPitch * z;

    float x2 = x1;
    float y2 = cosRoll * y1 + sinRoll * z1;
    float z2 = -sinRoll * y1 + cosRoll * z1;

    x = x2;
    y = y2;
    z = z2;
}

float GyroMouse::applyDeadzone(float value, float threshold) {
    if (fabsf(value) < threshold) {
        return 0.0f;
    }
    return value;
}

int8_t GyroMouse::clampMouseValue(float value) {
    if (value > 127.0f) return 127;
    if (value < -127.0f) return -127;
    return static_cast<int8_t>(value);
}

void GyroMouse::performAbsoluteCentering() {
    if (!config.absoluteRecenter) {
        return;
    }

    if (!bleController.isBleEnabled()) {
        return;
    }

    const int32_t rangeX = config.absoluteRangeX;
    const int32_t rangeY = config.absoluteRangeY;

    if (rangeX <= 0 && rangeY <= 0) {
        return;
    }

    dispatchRelativeMove(-rangeX, -rangeY);

    const int32_t halfX = (rangeX >= 0) ? ((rangeX + 1) / 2) : 0;
    const int32_t halfY = (rangeY >= 0) ? ((rangeY + 1) / 2) : 0;
    dispatchRelativeMove(halfX, halfY);

    Logger::getInstance().log("GyroMouse: Absolute pointer recentered");
}

void GyroMouse::dispatchRelativeMove(int deltaX, int deltaY) {
    const int maxStep = 127;

    while (deltaX != 0 || deltaY != 0) {
        int stepX = 0;
        if (deltaX > 0) {
            stepX = deltaX > maxStep ? maxStep : deltaX;
        } else if (deltaX < 0) {
            stepX = deltaX < -maxStep ? -maxStep : deltaX;
        }

        int stepY = 0;
        if (deltaY > 0) {
            stepY = deltaY > maxStep ? maxStep : deltaY;
        } else if (deltaY < 0) {
            stepY = deltaY < -maxStep ? -maxStep : deltaY;
        }

        if (stepX == 0 && stepY == 0) {
            break;
        }

        bleController.moveMouse(static_cast<signed char>(stepX),
                                static_cast<signed char>(stepY),
                                0,
                                0);

        deltaX -= stepX;
        deltaY -= stepY;

        delay(2);
    }
}

void GyroMouse::cycleSensitivity() {
    currentSensitivityIndex = (currentSensitivityIndex + 1) % config.sensitivities.size();

    smoothedMouseX = 0.0f;
    smoothedMouseY = 0.0f;
    recenterNeutral();

    String sensName = config.sensitivities[currentSensitivityIndex].name;
    Logger::getInstance().log("GyroMouse: Sensitivity changed to " + sensName);
}

String GyroMouse::getSensitivityName() const {
    if (currentSensitivityIndex < config.sensitivities.size()) {
        return config.sensitivities[currentSensitivityIndex].name;
    }
    return "unknown";
}
