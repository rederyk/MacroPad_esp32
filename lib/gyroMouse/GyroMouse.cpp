#include "GyroMouse.h"
#include "Logger.h"
#include "BLEController.h"
#include "InputHub.h"

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
      smoothedMouseX(0.0f),
      smoothedMouseY(0.0f),
      lastUpdateTime(0) {
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

    // Leggi valori gyro correnti (°/s)
    float gyroX = gestureSensor->getMappedGyroX();
    float gyroY = gestureSensor->getMappedGyroY();

    // Calcola movimento mouse
    int8_t mouseX, mouseY;
    calculateMouseMovement(gyroY, gyroX, deltaTime, mouseX, mouseY);

    // Invia movimento se non nullo
    if (mouseX != 0 || mouseY != 0) {
        bleController.moveMouse(mouseX, mouseY, 0, 0);
    }
}

void GyroMouse::calculateMouseMovement(float gyroX, float gyroY, float deltaTime,
                                       int8_t& mouseX, int8_t& mouseY) {
    // Ottieni settings sensibilità corrente
    const SensitivitySettings& sens = config.sensitivities[currentSensitivityIndex];

    // Applica deadzone
    gyroX = applyDeadzone(gyroX, sens.deadzone);
    gyroY = applyDeadzone(gyroY, sens.deadzone);

    // Converti velocità angolare in movimento pixel
    // Formula: movimento = velocità_angolare * sensibilità * deltaTime * fattore_scala
    // Fattore scala empirico per mappare °/s a pixel
    const float SCALE_FACTOR = 0.5f;

    float rawMouseX = gyroX * sens.scale * deltaTime * SCALE_FACTOR * 100.0f;
    float rawMouseY = gyroY * sens.scale * deltaTime * SCALE_FACTOR * 100.0f;

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

float GyroMouse::applyDeadzone(float value, float threshold) {
    if (abs(value) < threshold) {
        return 0.0f;
    }
    return value;
}

int8_t GyroMouse::clampMouseValue(float value) {
    if (value > 127.0f) return 127;
    if (value < -127.0f) return -127;
    return static_cast<int8_t>(value);
}

void GyroMouse::cycleSensitivity() {
    currentSensitivityIndex = (currentSensitivityIndex + 1) % config.sensitivities.size();

    smoothedMouseX = 0.0f;
    smoothedMouseY = 0.0f;

    String sensName = config.sensitivities[currentSensitivityIndex].name;
    Logger::getInstance().log("GyroMouse: Sensitivity changed to " + sensName);
}

String GyroMouse::getSensitivityName() const {
    if (currentSensitivityIndex < config.sensitivities.size()) {
        return config.sensitivities[currentSensitivityIndex].name;
    }
    return "unknown";
}
