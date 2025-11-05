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
    constexpr float kNeutralCaptureGyroThreshold = 0.15f; // rad/s (increased from 0.05)
    constexpr uint16_t kNeutralCaptureSampleTarget = 40; // reduced from 80
    constexpr float kMotionTimeoutMs = 300.0f; // ms without motion for auto-recenter
    constexpr float kMinNoiseEstimate = 0.01f;
    constexpr float kMaxNoiseEstimate = 0.5f;
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
      residualMouseX(0.0f),
      residualMouseY(0.0f),
      gyroBiasX(0.0f),
      gyroBiasY(0.0f),
      gyroBiasZ(0.0f),
      lastUpdateTime(0),
      fusedPitchRad(0.0f),
      fusedRollRad(0.0f),
      neutralPitchRad(0.0f),
      neutralRollRad(0.0f),
      neutralCaptured(false),
      gyroNoiseEstimate(0.05f),
      adaptiveSmoothingFactor(0.3f),
      velocityMagnitude(0.0f),
      lastMotionTime(0),
      neutralCapturePending(false),
      neutralCaptureSamples(0),
      neutralPitchAccum(0.0f),
      neutralRollAccum(0.0f),
      gyroBiasAccumX(0.0f),
      gyroBiasAccumY(0.0f),
      gyroBiasAccumZ(0.0f) {
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
        if (sens.invertXOverride < -1 || sens.invertXOverride > 1) {
            sens.invertXOverride = -1;
        }
        if (sens.invertYOverride < -1 || sens.invertYOverride > 1) {
            sens.invertYOverride = -1;
        }
        if (sens.swapAxesOverride < -1 || sens.swapAxesOverride > 1) {
            sens.swapAxesOverride = -1;
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
    gyroNoiseEstimate = 0.05f;
    adaptiveSmoothingFactor = 0.3f;
    velocityMagnitude = 0.0f;
    lastMotionTime = millis();

    // Initialize quaternions
    currentOrientation = Quaternion();
    neutralOrientation = Quaternion();
    lastOrientation = Quaternion();

    beginNeutralCapture();
    Logger::getInstance().log("GyroMouse: Neutral capture requested");
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
    residualMouseX = 0.0f;
    residualMouseY = 0.0f;
    gyroBiasX = 0.0f;
    gyroBiasY = 0.0f;
    gyroBiasZ = 0.0f;
    neutralCapturePending = false;
    neutralCaptureSamples = 0;
    neutralPitchAccum = 0.0f;
    neutralRollAccum = 0.0f;
    gyroBiasAccumX = 0.0f;
    gyroBiasAccumY = 0.0f;
    gyroBiasAccumZ = 0.0f;
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

    updateAdaptiveFiltering(frame, deltaTime);

    // Use quaternion-based orientation tracking
    updateQuaternionOrientation(frame, deltaTime);

    // Keep legacy orientation for compatibility
    updateOrientation(frame, deltaTime);
    updateNeutralBaseline(deltaTime, frame);

    if (!neutralCaptured) {
        smoothedMouseX = 0.0f;
        smoothedMouseY = 0.0f;
        residualMouseX = 0.0f;
        residualMouseY = 0.0f;
        return;
    }

    // Calcola movimento mouse usando il nuovo algoritmo basato su rotazione relativa
    int8_t mouseX = 0;
    int8_t mouseY = 0;

    // Use new quaternion-based algorithm for all modes
    if (gyroAvailable) {
        calculateMouseMovementFromRelativeRotation(frame, deltaTime, mouseX, mouseY);
    } else {
        // Fallback to legacy algorithm if gyro not available
        calculateMouseMovement(frame, deltaTime, mouseX, mouseY);
    }

    // Invia movimento se non nullo
    if (mouseX != 0 || mouseY != 0) {
        bleController.moveMouse(mouseX, mouseY, 0, 0);
    }
}

void GyroMouse::calculateMouseMovement(const SensorFrame& frame, float deltaTime,
                                       int8_t& mouseX, int8_t& mouseY) {
    // Legacy fallback (non usato - gyro non disponibile)
    mouseX = 0;
    mouseY = 0;
}

void GyroMouse::updateOrientation(const SensorFrame& frame, float deltaTime) {
    float predictedPitch = fusedPitchRad;
    float predictedRoll = fusedRollRad;

    if (gyroAvailable && deltaTime > 0.0f) {
        predictedPitch += (frame.gyroY - gyroBiasY) * deltaTime;
        predictedRoll += (frame.gyroX - gyroBiasX) * deltaTime;
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
        } else {
            // Adaptive alpha: trust gyro more during fast motion, accel more when stationary
            float alpha = config.orientationAlpha;
            if (gyroAvailable) {
                // Reduce alpha (trust accel more) when moving slowly for better accuracy
                if (velocityMagnitude < 0.1f) {
                    alpha = alpha * 0.85f; // More correction from accelerometer
                } else if (velocityMagnitude > 0.8f) {
                    alpha = fminf(alpha * 1.1f, 0.99f); // Trust gyro more during fast motion
                }
            }

            if (!gyroAvailable) {
                fusedPitchRad = fusedPitchRad * alpha + pitchAcc * (1.0f - alpha);
                fusedRollRad = fusedRollRad * alpha + rollAcc * (1.0f - alpha);
            } else {
                fusedPitchRad = alpha * predictedPitch + (1.0f - alpha) * pitchAcc;
                fusedRollRad = alpha * predictedRoll + (1.0f - alpha) * rollAcc;
            }
        }

        accumulateNeutralCapture(pitchAcc, rollAcc, frame);
    } else {
        fusedPitchRad = predictedPitch;
        fusedRollRad = predictedRoll;
    }

    const float limitRad = config.tiltLimitDegrees * kDegToRad * 2.0f;
    fusedPitchRad = constrain(fusedPitchRad, -limitRad, limitRad);
    fusedRollRad = constrain(fusedRollRad, -limitRad, limitRad);
}

void GyroMouse::updateNeutralBaseline(float deltaTime, const SensorFrame& frame) {
    if (!neutralCaptured || config.recenterRate <= 0.0f || neutralCapturePending) {
        return;
    }

    float tiltXDeg = (fusedRollRad - neutralRollRad) * kRadToDeg;
    float tiltYDeg = (fusedPitchRad - neutralPitchRad) * kRadToDeg;

    float threshold = config.recenterThresholdDegrees;
    if (fabsf(tiltXDeg) > threshold || fabsf(tiltYDeg) > threshold) {
        return;
    }

    if (fabsf(frame.gyroX - gyroBiasX) > kGyroQuietThreshold ||
        fabsf(frame.gyroY - gyroBiasY) > kGyroQuietThreshold) {
        return;
    }

    float gain = config.recenterRate * deltaTime;
    gain = constrain(gain, 0.0f, 0.2f);

    neutralRollRad += (fusedRollRad - neutralRollRad) * gain;
    neutralPitchRad += (fusedPitchRad - neutralPitchRad) * gain;
    gyroBiasX += (frame.gyroX - gyroBiasX) * gain;
    gyroBiasY += (frame.gyroY - gyroBiasY) * gain;
    gyroBiasZ += (frame.gyroZ - gyroBiasZ) * gain;
}

void GyroMouse::recenterNeutral() {
    if (!gestureSensor) {
        Logger::getInstance().log("GyroMouse: recenter ignored (sensor unavailable)");
        return;
    }

    performAbsoluteCentering();

    beginNeutralCapture();

    Logger::getInstance().log("GyroMouse: Neutral capture requested");
}

void GyroMouse::rotateVectorByNeutral(float& x, float& y, float& z) const {
    if (!neutralCaptured) {
        return;
    }

    // Inverse rotation: rotate back from neutral orientation to world frame
    // Apply roll rotation first (inverse: -roll angle)
    float sinRoll = sinf(-neutralRollRad);
    float cosRoll = cosf(-neutralRollRad);

    float x1 = x;
    float y1 = cosRoll * y - sinRoll * z;
    float z1 = sinRoll * y + cosRoll * z;

    // Then apply pitch rotation (inverse: -pitch angle)
    float sinPitch = sinf(-neutralPitchRad);
    float cosPitch = cosf(-neutralPitchRad);

    float x2 = cosPitch * x1 + sinPitch * z1;
    float y2 = y1;
    float z2 = -sinPitch * x1 + cosPitch * z1;

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

int8_t GyroMouse::clampMouseValue(float pending, float& residual) {
    // BLE HID reports are signed char, so stay within [-127, 127]
    // Round-to-nearest so slow motion still accumulates
    float rounded = roundf(pending);

    if (rounded > 127.0f) {
        residual = 0.0f;
        return 127;
    }
    if (rounded < -127.0f) {
        residual = 0.0f;
        return -127;
    }

    const float reported = static_cast<float>(rounded);
    residual = pending - reported;
    return static_cast<int8_t>(rounded);
}

void GyroMouse::beginNeutralCapture() {
    neutralCaptured = false;
    neutralCapturePending = true;
    neutralCaptureSamples = 0;
    neutralPitchAccum = 0.0f;
    neutralRollAccum = 0.0f;
    gyroBiasAccumX = 0.0f;
    gyroBiasAccumY = 0.0f;
    gyroBiasAccumZ = 0.0f;
    smoothedMouseX = 0.0f;
    smoothedMouseY = 0.0f;
    residualMouseX = 0.0f;
    residualMouseY = 0.0f;
    gyroBiasX = 0.0f;
    gyroBiasY = 0.0f;
    gyroBiasZ = 0.0f;
}

void GyroMouse::accumulateNeutralCapture(float pitchAcc, float rollAcc, const SensorFrame& frame) {
    if (!neutralCapturePending) {
        return;
    }

    const float gyroQuietX = fabsf(frame.gyroX - gyroBiasX);
    const float gyroQuietY = fabsf(frame.gyroY - gyroBiasY);
    const float gyroQuietZ = fabsf(frame.gyroZ - gyroBiasZ);
    if (gyroQuietX > kNeutralCaptureGyroThreshold ||
        gyroQuietY > kNeutralCaptureGyroThreshold ||
        gyroQuietZ > kNeutralCaptureGyroThreshold) {
        neutralCaptureSamples = 0;
        neutralPitchAccum = 0.0f;
        neutralRollAccum = 0.0f;
        gyroBiasAccumX = 0.0f;
        gyroBiasAccumY = 0.0f;
        gyroBiasAccumZ = 0.0f;
        return;
    }

    neutralPitchAccum += pitchAcc;
    neutralRollAccum += rollAcc;
    gyroBiasAccumX += frame.gyroX;
    gyroBiasAccumY += frame.gyroY;
    gyroBiasAccumZ += frame.gyroZ;
    ++neutralCaptureSamples;

    if (neutralCaptureSamples < kNeutralCaptureSampleTarget) {
        return;
    }

    const float invCount = 1.0f / static_cast<float>(neutralCaptureSamples);
    neutralPitchRad = neutralPitchAccum * invCount;
    neutralRollRad = neutralRollAccum * invCount;
    fusedPitchRad = neutralPitchRad;
    fusedRollRad = neutralRollRad;
    gyroBiasX = gyroBiasAccumX * invCount;
    gyroBiasY = gyroBiasAccumY * invCount;
    gyroBiasZ = gyroBiasAccumZ * invCount;

    // Set neutral orientation quaternion from captured angles
    float cr = cosf(neutralRollRad * 0.5f);
    float sr = sinf(neutralRollRad * 0.5f);
    float cp = cosf(neutralPitchRad * 0.5f);
    float sp = sinf(neutralPitchRad * 0.5f);

    neutralOrientation = Quaternion(
        cr * cp,
        sr * cp,
        cr * sp,
        -sr * sp
    );
    currentOrientation = neutralOrientation;
    lastOrientation = neutralOrientation;

    neutralCapturePending = false;
    neutralCaptured = true;
    smoothedMouseX = 0.0f;
    smoothedMouseY = 0.0f;
    residualMouseX = 0.0f;
    residualMouseY = 0.0f;
    lastUpdateTime = millis();

    Logger::getInstance().log("GyroMouse: Neutral capture completed (" +
                              String(neutralCaptureSamples) + " samples)");
    Logger::getInstance().log("GyroMouse: Neutral orientation recentered");
}

void GyroMouse::performAbsoluteCentering() {
    if (!config.absoluteRecenter) {
        return;
    }

    if (!bleController.isBleEnabled()) {
        return;
    }

    const bool centerX = config.absoluteRangeX > 0;
    const bool centerY = config.absoluteRangeY > 0;

    if (!centerX && !centerY) {
        return;
    }

    const int32_t backX = centerX ? -config.absoluteRangeX : 0;
    const int32_t backY = centerY ? -config.absoluteRangeY : 0;
    if (backX != 0 || backY != 0) {
        dispatchRelativeMove(backX, backY);
    }

    const int32_t halfX = centerX ? ((config.absoluteRangeX + 1) / 2) : 0;
    const int32_t halfY = centerY ? ((config.absoluteRangeY + 1) / 2) : 0;
    if (halfX != 0 || halfY != 0) {
        dispatchRelativeMove(halfX, halfY);
    }

    Logger::getInstance().log("GyroMouse: Absolute pointer recentered");
}

void GyroMouse::dispatchRelativeMove(int deltaX, int deltaY) {
    const int maxStep = 127;
    int iterations = 0;
    const int maxIterations = 50; // Safety limit to prevent infinite loops

    while ((deltaX != 0 || deltaY != 0) && iterations < maxIterations) {
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
        iterations++;

        // Yield to system instead of blocking delay
        yield();
        delayMicroseconds(500); // Very short delay for HID stability
    }
}

void GyroMouse::cycleSensitivity() {
    currentSensitivityIndex = (currentSensitivityIndex + 1) % config.sensitivities.size();

    smoothedMouseX = 0.0f;
    smoothedMouseY = 0.0f;
    residualMouseX = 0.0f;
    residualMouseY = 0.0f;

    // Don't recenter on sensitivity change - just start neutral capture
    // This avoids the blocking absolute recenter operation
    beginNeutralCapture();

    String sensName = config.sensitivities[currentSensitivityIndex].name;
    Logger::getInstance().log("GyroMouse: Sensitivity changed to " + sensName);
}

String GyroMouse::getSensitivityName() const {
    if (currentSensitivityIndex < config.sensitivities.size()) {
        return config.sensitivities[currentSensitivityIndex].name;
    }
    return "unknown";
}

void GyroMouse::updateAdaptiveFiltering(const SensorFrame& frame, float /*deltaTime*/) {
    float gyroX = frame.gyroX - gyroBiasX;
    float gyroY = frame.gyroY - gyroBiasY;
    float gyroZ = frame.gyroZ - gyroBiasZ;

    float gyroMagnitude = sqrtf(gyroX * gyroX + gyroY * gyroY + gyroZ * gyroZ);

    // Simple EMA on overall motion energy
    const float velocityAlpha = 0.2f;
    velocityMagnitude += (gyroMagnitude - velocityMagnitude) * velocityAlpha;

    // Map the current motion into a 0..1 range
    float motionIntensity = constrain((velocityMagnitude - 0.05f) * 2.0f, 0.0f, 1.0f);

    float baseSmoothing = constrain(config.smoothing, 0.0f, 0.95f);
    float slowSmoothing = constrain(baseSmoothing * 1.2f + 0.05f, 0.05f, 0.9f);
    float fastSmoothing = constrain(baseSmoothing * 0.35f + 0.05f, 0.05f, slowSmoothing);
    float targetSmoothing = slowSmoothing + (fastSmoothing - slowSmoothing) * motionIntensity;

    adaptiveSmoothingFactor += (targetSmoothing - adaptiveSmoothingFactor) * 0.25f;

    float noiseTarget = gyroMagnitude;
    if (gyroMagnitude > 0.4f) {
        noiseTarget = gyroNoiseEstimate;
    }
    gyroNoiseEstimate += (noiseTarget - gyroNoiseEstimate) * 0.1f;
    gyroNoiseEstimate = constrain(gyroNoiseEstimate, kMinNoiseEstimate, kMaxNoiseEstimate);

    if (gyroMagnitude > 0.05f) {
        lastMotionTime = millis();
    }
}

float GyroMouse::applyDynamicDeadzone(float value, float baseThreshold, float noiseFactor) {
    float base = fmaxf(baseThreshold, 0.0f);
    float dynamicThreshold = base + noiseFactor * kRadToDeg * 1.2f;

    const float minThreshold = 0.05f;
    if (base <= 0.0f) {
        dynamicThreshold = fmaxf(dynamicThreshold, minThreshold);
    } else {
        dynamicThreshold = constrain(dynamicThreshold, base * 0.6f, base * 2.2f + minThreshold);
        dynamicThreshold = fmaxf(dynamicThreshold, minThreshold);
    }

    float absValue = fabsf(value);
    if (absValue <= dynamicThreshold) {
        return 0.0f;
    }

    float excess = absValue - dynamicThreshold;
    float response = dynamicThreshold + excess * 0.75f;
    return (value >= 0.0f) ? response : -response;
}

float GyroMouse::applySmoothCurve(float value, float deadzone, float maxValue) {
    // Apply deadzone
    if (fabsf(value) < deadzone) {
        return 0.0f;
    }

    float sign = (value >= 0.0f) ? 1.0f : -1.0f;
    float absValue = fabsf(value);

    // Remove deadzone from calculation
    float effective = absValue - deadzone;
    float effectiveMax = maxValue - deadzone;

    if (effectiveMax <= 0.0f) {
        return 0.0f;
    }

    // Normalize to [0, 1]
    float normalized = effective / effectiveMax;
    normalized = constrain(normalized, 0.0f, 1.0f);

    // Apply smooth S-curve (smoothstep function) for natural feel
    // This gives slow start, fast middle, slow end
    float smoothed = normalized * normalized * (3.0f - 2.0f * normalized);

    // Map back to output range with some scaling
    return sign * smoothed * effectiveMax;
}

// ============================================================================
// Quaternion Implementation
// ============================================================================

void GyroMouse::Quaternion::normalize() {
    float norm = sqrtf(w * w + x * x + y * y + z * z);
    if (norm > 1e-6f) {
        float invNorm = 1.0f / norm;
        w *= invNorm;
        x *= invNorm;
        y *= invNorm;
        z *= invNorm;
    }
}

GyroMouse::Quaternion GyroMouse::Quaternion::conjugate() const {
    return Quaternion(w, -x, -y, -z);
}

GyroMouse::Quaternion GyroMouse::Quaternion::multiply(const Quaternion& q) const {
    return Quaternion(
        w * q.w - x * q.x - y * q.y - z * q.z,
        w * q.x + x * q.w + y * q.z - z * q.y,
        w * q.y - x * q.z + y * q.w + z * q.x,
        w * q.z + x * q.y - y * q.x + z * q.w
    );
}

void GyroMouse::Quaternion::rotateVector(float& vx, float& vy, float& vz) const {
    // v' = q * v * q^-1
    // Optimized version for unit quaternion
    float qx = x, qy = y, qz = z, qw = w;

    // First rotation: q * v
    float tx = qw * vx + qy * vz - qz * vy;
    float ty = qw * vy + qz * vx - qx * vz;
    float tz = qw * vz + qx * vy - qy * vx;
    float tw = -qx * vx - qy * vy - qz * vz;

    // Second rotation: result * q^-1 (conjugate)
    vx = tw * (-qx) + tx * qw + ty * (-qz) - tz * (-qy);
    vy = tw * (-qy) + ty * qw + tz * (-qx) - tx * (-qz);
    vz = tw * (-qz) + tz * qw + tx * (-qy) - ty * (-qx);
}

void GyroMouse::Quaternion::toEuler(float& pitch, float& roll, float& yaw) const {
    // Convert quaternion to Euler angles (Tait-Bryan angles)
    // roll (x-axis rotation)
    float sinr_cosp = 2.0f * (w * x + y * z);
    float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
    roll = atan2f(sinr_cosp, cosr_cosp);

    // pitch (y-axis rotation)
    float sinp = 2.0f * (w * y - z * x);
    if (fabsf(sinp) >= 1.0f)
        pitch = copysignf(M_PI / 2.0f, sinp); // use 90 degrees if out of range
    else
        pitch = asinf(sinp);

    // yaw (z-axis rotation)
    float siny_cosp = 2.0f * (w * z + x * y);
    float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
    yaw = atan2f(siny_cosp, cosy_cosp);
}

// ============================================================================
// New Rotation-Relative Algorithm
// ============================================================================

GyroMouse::Quaternion GyroMouse::createQuaternionFromGyro(const SensorFrame& frame, float deltaTime) const {
    // Create quaternion from angular velocity (axis-angle representation)
    float gx = (frame.gyroX - gyroBiasX) * deltaTime;
    float gy = (frame.gyroY - gyroBiasY) * deltaTime;
    float gz = (frame.gyroZ - gyroBiasZ) * deltaTime;

    float angle = sqrtf(gx * gx + gy * gy + gz * gz);

    if (angle < 1e-6f) {
        return Quaternion(1.0f, 0.0f, 0.0f, 0.0f);
    }

    float halfAngle = angle * 0.5f;
    float sinHalfAngle = sinf(halfAngle);
    float invAngle = 1.0f / angle;

    return Quaternion(
        cosf(halfAngle),
        gx * invAngle * sinHalfAngle,
        gy * invAngle * sinHalfAngle,
        gz * invAngle * sinHalfAngle
    );
}

void GyroMouse::updateQuaternionOrientation(const SensorFrame& frame, float deltaTime) {
    if (!gyroAvailable || deltaTime <= 0.0f) {
        return;
    }

    // Store previous orientation
    lastOrientation = currentOrientation;

    // Create rotation quaternion from gyroscope
    Quaternion deltaQ = createQuaternionFromGyro(frame, deltaTime);

    // Update current orientation: q_new = q_current * q_delta
    currentOrientation = currentOrientation.multiply(deltaQ);
    currentOrientation.normalize();

    // Accel correction (complementary filter in quaternion space)
    bool accelReliable = (frame.accelMagnitude > kAccelReliableMin) &&
                         (frame.accelMagnitude < kAccelReliableMax);

    if (accelReliable) {
        float pitchAcc = atan2f(-frame.accelX, sqrtf(frame.accelY * frame.accelY + frame.accelZ * frame.accelZ));
        float rollAcc = atan2f(frame.accelY, frame.accelZ);

        // Create quaternion from accelerometer angles (simple conversion)
        float cr = cosf(rollAcc * 0.5f);
        float sr = sinf(rollAcc * 0.5f);
        float cp = cosf(pitchAcc * 0.5f);
        float sp = sinf(pitchAcc * 0.5f);

        Quaternion accelQ(
            cr * cp,
            sr * cp,
            cr * sp,
            -sr * sp
        );

        // Spherical linear interpolation (SLERP) for smooth correction
        float alpha = 1.0f - config.orientationAlpha;

        // Adaptive alpha based on motion
        if (velocityMagnitude < 0.1f) {
            alpha *= 1.2f; // Trust accel more when stationary
        } else if (velocityMagnitude > 0.8f) {
            alpha *= 0.5f; // Trust gyro more during fast motion
        }
        alpha = constrain(alpha, 0.001f, 0.15f);

        // Simple LERP + normalize (approximation of SLERP for small angles)
        currentOrientation.w = currentOrientation.w * (1.0f - alpha) + accelQ.w * alpha;
        currentOrientation.x = currentOrientation.x * (1.0f - alpha) + accelQ.x * alpha;
        currentOrientation.y = currentOrientation.y * (1.0f - alpha) + accelQ.y * alpha;
        currentOrientation.z = currentOrientation.z * (1.0f - alpha) + accelQ.z * alpha;
        currentOrientation.normalize();
    }
}

void GyroMouse::extractLocalAngularVelocity(float& localPitch, float& localYaw) const {
    if (!neutralCaptured) {
        localPitch = 0.0f;
        localYaw = 0.0f;
        return;
    }

    // Calculate relative orientation: q_rel = q_neutral^-1 * q_current
    Quaternion relativeOrientation = neutralOrientation.conjugate().multiply(currentOrientation);

    // Extract pitch and yaw from relative quaternion
    // This gives us the rotation in the neutral frame of reference
    float pitch, roll, yaw;
    relativeOrientation.toEuler(pitch, roll, yaw);

    // Return pitch (up/down) and yaw (left/right) relative to neutral
    // Roll is less important for cursor control
    localPitch = pitch;
    localYaw = yaw;
}

void GyroMouse::calculateMouseMovementFromRelativeRotation(const SensorFrame& frame, float deltaTime,
                                                            int8_t& mouseX, int8_t& mouseY) {
    if (!neutralCaptured || config.sensitivities.empty()) {
        mouseX = 0;
        mouseY = 0;
        return;
    }

    const SensitivitySettings& sens = config.sensitivities[currentSensitivityIndex];
    const float rateScale = sens.gyroScale > 0.0f ? sens.gyroScale : sens.scale;

    // Calculate relative rotation in neutral space
    Quaternion relativeRotation = neutralOrientation.conjugate().multiply(currentOrientation);
    Quaternion lastRelativeRotation = neutralOrientation.conjugate().multiply(lastOrientation);

    // Calculate delta rotation between frames in neutral space
    Quaternion deltaRotation = lastRelativeRotation.conjugate().multiply(relativeRotation);

    // Extract angular velocity in local (neutral) frame
    float deltaAngle = 2.0f * acosf(constrain(deltaRotation.w, -1.0f, 1.0f));

    float rawMouseX = 0.0f;
    float rawMouseY = 0.0f;

    if (deltaAngle > 1e-6f && deltaTime > 1e-6f) {
        float sinHalfAngle = sinf(deltaAngle * 0.5f);
        if (fabsf(sinHalfAngle) > 1e-6f) {
            // Extract rotation axis in local coordinates
            float invSinHalf = 1.0f / sinHalfAngle;
            float axisX = deltaRotation.x * invSinHalf;
            float axisY = deltaRotation.y * invSinHalf;

            // Angular velocity in local frame (rad/s)
            float angularVelX = axisX * deltaAngle / deltaTime;
            float angularVelY = axisY * deltaAngle / deltaTime;

            // Convert to degrees
            angularVelX *= kRadToDeg;
            angularVelY *= kRadToDeg;

            // Apply deadzone and scaling
            float rateX = applyDynamicDeadzone(angularVelX, sens.deadzone, gyroNoiseEstimate);
            float rateY = applyDynamicDeadzone(angularVelY, sens.deadzone, gyroNoiseEstimate);

            // Scale by sensitivity and time
            rawMouseX = rateX * rateScale * deltaTime * kRateScaleFactor;
            rawMouseY = rateY * rateScale * deltaTime * kRateScaleFactor;
        }
    }

    // Apply axis transformations
    bool invertX = config.invertX;
    bool invertY = config.invertY;
    bool swapAxes = config.swapAxes;

    if (sens.swapAxesOverride >= 0) {
        swapAxes = sens.swapAxesOverride > 0;
    }
    if (sens.invertXOverride >= 0) {
        invertX = sens.invertXOverride > 0;
    }
    if (sens.invertYOverride >= 0) {
        invertY = sens.invertYOverride > 0;
    }

    if (swapAxes) {
        float temp = rawMouseX;
        rawMouseX = rawMouseY;
        rawMouseY = temp;
    }
    if (invertX) rawMouseX = -rawMouseX;
    if (invertY) rawMouseY = -rawMouseY;

    // Apply adaptive smoothing
    float currentSmoothFactor = constrain(adaptiveSmoothingFactor, 0.0f, 0.95f);

    auto applySmoothing = [&](float rawValue, float& smoothValue, float& residualValue) -> int8_t {
        if (currentSmoothFactor <= 0.0f) {
            smoothValue = rawValue;
        } else {
            smoothValue += (rawValue - smoothValue) * currentSmoothFactor;
        }
        float pending = (currentSmoothFactor <= 0.0f ? rawValue : smoothValue) + residualValue;
        return clampMouseValue(pending, residualValue);
    };

    mouseX = applySmoothing(rawMouseX, smoothedMouseX, residualMouseX);
    mouseY = applySmoothing(rawMouseY, smoothedMouseY, residualMouseY);
}
