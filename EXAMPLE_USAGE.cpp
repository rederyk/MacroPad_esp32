/*
 * ESP32 MacroPad Project - Gesture Recognition Example
 * Copyright (C) [2025] [Enrico Mori]
 *
 * Example usage of the new dual-mode gesture recognition system
 */

#include "gestureAnalyze.h"
#include "gestureRead.h"
#include "Logger.h"

// Global instances (usually declared in main.cpp)
extern GestureRead gestureSensor;
extern GestureAnalyze gestureAnalyzer;

// ============================================================================
// EXAMPLE 1: Auto-Mode (Recommended)
// ============================================================================

void example_auto_mode()
{
    Logger::getInstance().log("=== Auto-Mode Example ===");

    // Setup gesture analyzer (do once in setup())
    gestureAnalyzer.setRecognitionMode(MODE_AUTO);      // Auto-select best mode
    gestureAnalyzer.setConfidenceThreshold(0.6f);       // Require 60% confidence

    // Start gesture capture (when user presses button)
    if (gestureSensor.startSampling()) {
        Logger::getInstance().log("Recording gesture... (move device now)");

        // Wait for gesture completion (e.g., 2 seconds or until buffer full)
        while (gestureSensor.isSampling()) {
            gestureSensor.updateSampling();
            delay(10);
        }

        Logger::getInstance().log("Gesture captured, analyzing...");

        // Recognize gesture using best available method
        GestureResult result = gestureAnalyzer.recognizeGesture();

        // Check if gesture was recognized
        if (result.gestureID >= 0 && result.confidence >= 0.6f) {
            Logger::getInstance().log("SUCCESS: Detected " + String(result.getName()));
            Logger::getInstance().log("Mode: " + String(result.mode));
            Logger::getInstance().log("Confidence: " + String(result.confidence * 100, 0) + "%");

            // Perform actions based on gesture type
            handleGestureResult(result);
        } else {
            Logger::getInstance().log("No gesture recognized (low confidence or unknown)");
        }

        // Cleanup
        gestureAnalyzer.clearSamples();
    } else {
        Logger::getInstance().log("Failed to start sampling");
    }
}

// ============================================================================
// EXAMPLE 2: Shape Recognition Only
// ============================================================================

void example_shape_recognition()
{
    Logger::getInstance().log("=== Shape Recognition Example ===");

    // Force shape recognition mode
    gestureAnalyzer.setRecognitionMode(MODE_SHAPE_RECOGNITION);

    gestureSensor.startSampling();

    // User draws shape in air (1-2 seconds)
    delay(2000);

    gestureSensor.stopSampling();

    // Recognize shape
    GestureResult result = gestureAnalyzer.recognizeShape();

    if (result.shapeType != SHAPE_UNKNOWN) {
        Logger::getInstance().log("Shape detected: " + String(getShapeName(result.shapeType)));

        switch (result.shapeType) {
            case SHAPE_CIRCLE:
                Logger::getInstance().log("ACTION: Confirm/Select");
                // confirmAction();
                break;

            case SHAPE_LINE:
                Logger::getInstance().log("ACTION: Navigate");
                // navigateDirection();
                break;

            case SHAPE_TRIANGLE:
                Logger::getInstance().log("ACTION: Open Menu");
                // openMenu();
                break;

            case SHAPE_SQUARE:
                Logger::getInstance().log("ACTION: Toggle Mode");
                // toggleMode();
                break;

            case SHAPE_ZIGZAG:
                Logger::getInstance().log("ACTION: Delete/Clear");
                // clearAction();
                break;

            case SHAPE_INFINITY:
                Logger::getInstance().log("ACTION: Loop/Repeat");
                // repeatAction();
                break;

            default:
                Logger::getInstance().log("Unknown shape");
        }
    } else {
        Logger::getInstance().log("No shape recognized");
    }

    gestureAnalyzer.clearSamples();
}

// ============================================================================
// EXAMPLE 3: Orientation Recognition (MPU6050 required)
// ============================================================================

void example_orientation_recognition()
{
    Logger::getInstance().log("=== Orientation Recognition Example ===");

    // Check if gyroscope is available
    if (!gestureAnalyzer.hasGyroscope()) {
        Logger::getInstance().log("ERROR: No gyroscope available (MPU6050 required)");
        return;
    }

    // Force orientation recognition mode
    gestureAnalyzer.setRecognitionMode(MODE_ORIENTATION);

    gestureSensor.startSampling();

    // User performs rotation/tilt gesture
    delay(1500);

    gestureSensor.stopSampling();

    // Recognize orientation gesture
    GestureResult result = gestureAnalyzer.recognizeOrientation();

    if (result.orientationType != ORIENT_UNKNOWN) {
        Logger::getInstance().log("Orientation detected: " + String(getOrientationName(result.orientationType)));

        switch (result.orientationType) {
            case ORIENT_ROTATE_90_CW:
                Logger::getInstance().log("ACTION: Rotate screen 90° clockwise");
                // rotateDisplay(90);
                break;

            case ORIENT_ROTATE_90_CCW:
                Logger::getInstance().log("ACTION: Rotate screen 90° counter-clockwise");
                // rotateDisplay(-90);
                break;

            case ORIENT_ROTATE_180:
                Logger::getInstance().log("ACTION: Flip screen 180°");
                // rotateDisplay(180);
                break;

            case ORIENT_TILT_FORWARD:
                Logger::getInstance().log("ACTION: Scroll down");
                // scrollDown();
                break;

            case ORIENT_TILT_BACKWARD:
                Logger::getInstance().log("ACTION: Scroll up");
                // scrollUp();
                break;

            case ORIENT_TILT_LEFT:
                Logger::getInstance().log("ACTION: Volume down");
                // volumeDown();
                break;

            case ORIENT_TILT_RIGHT:
                Logger::getInstance().log("ACTION: Volume up");
                // volumeUp();
                break;

            case ORIENT_FACE_DOWN:
                Logger::getInstance().log("ACTION: Mute/Sleep");
                // mute();
                break;

            case ORIENT_FACE_UP:
                Logger::getInstance().log("ACTION: Wake/Unmute");
                // wake();
                break;

            case ORIENT_SHAKE_X:
            case ORIENT_SHAKE_Y:
            case ORIENT_SHAKE_Z:
                Logger::getInstance().log("ACTION: Undo");
                // undo();
                break;

            case ORIENT_SPIN:
                Logger::getInstance().log("ACTION: Spin menu");
                // spinMenu();
                break;

            default:
                Logger::getInstance().log("Unknown orientation");
        }
    } else {
        Logger::getInstance().log("No orientation gesture recognized");
    }

    gestureAnalyzer.clearSamples();
}

// ============================================================================
// EXAMPLE 4: Hybrid System (Both Shape and Orientation)
// ============================================================================

void example_hybrid_system()
{
    Logger::getInstance().log("=== Hybrid System Example ===");

    // Use auto-mode for intelligent selection
    gestureAnalyzer.setRecognitionMode(MODE_AUTO);
    gestureAnalyzer.setConfidenceThreshold(0.5f);

    gestureSensor.startSampling();
    delay(2000);
    gestureSensor.stopSampling();

    GestureResult result = gestureAnalyzer.recognizeGesture();

    if (result.gestureID < 0) {
        Logger::getInstance().log("No gesture detected");
        return;
    }

    // Handle based on which mode was used
    if (result.mode == MODE_SHAPE_RECOGNITION) {
        Logger::getInstance().log("Shape gesture: " + String(getShapeName(result.shapeType)));
        handleShapeGesture(result.shapeType);
    }
    else if (result.mode == MODE_ORIENTATION) {
        Logger::getInstance().log("Orientation gesture: " + String(getOrientationName(result.orientationType)));
        handleOrientationGesture(result.orientationType);
    }
    else {
        Logger::getInstance().log("Legacy KNN gesture ID: " + String(result.gestureID));
        handleLegacyGesture(result.gestureID);
    }

    gestureAnalyzer.clearSamples();
}

// ============================================================================
// EXAMPLE 5: Custom Tuning
// ============================================================================

void example_custom_tuning()
{
    Logger::getInstance().log("=== Custom Tuning Example ===");

    // Get references to internal analyzers (advanced usage)
    // Note: These would need to be exposed via getters in GestureAnalyze class

    // Tune shape analysis
    ShapeAnalysis shapeAnalyzer;
    shapeAnalyzer.setCircularityThreshold(0.8f);   // Stricter circle detection
    shapeAnalyzer.setSharpTurnAngle(45.0f);        // More sensitive corner detection

    // Tune orientation analysis
    OrientationFeatureExtractor orientExtractor;
    orientExtractor.setRotationThreshold(60.0f);   // More sensitive rotations
    orientExtractor.setTiltThreshold(25.0f);       // More sensitive tilts
    orientExtractor.setShakeThreshold(1.5f);       // More sensitive shakes

    // Tune motion integrator
    MotionIntegrator motionIntegrator;
    motionIntegrator.setMaxIntegrationTime(2.5f);  // Allow slightly longer gestures
    motionIntegrator.setDriftThreshold(2.5f);      // More tolerant of drift
    motionIntegrator.setUseMadgwick(true);         // Use accurate gravity removal

    Logger::getInstance().log("Custom tuning applied");
}

// ============================================================================
// Helper Functions
// ============================================================================

void handleGestureResult(const GestureResult& result)
{
    if (result.mode == MODE_SHAPE_RECOGNITION) {
        handleShapeGesture(result.shapeType);
    } else if (result.mode == MODE_ORIENTATION) {
        handleOrientationGesture(result.orientationType);
    } else {
        handleLegacyGesture(result.gestureID);
    }
}

void handleShapeGesture(ShapeType type)
{
    switch (type) {
        case SHAPE_CIRCLE:   /* Confirm action */; break;
        case SHAPE_LINE:     /* Navigate */; break;
        case SHAPE_TRIANGLE: /* Menu */; break;
        case SHAPE_SQUARE:   /* Select */; break;
        case SHAPE_ZIGZAG:   /* Delete */; break;
        case SHAPE_INFINITY: /* Repeat */; break;
        default: break;
    }
}

void handleOrientationGesture(OrientationType type)
{
    switch (type) {
        case ORIENT_ROTATE_90_CW:   /* Rotate CW */; break;
        case ORIENT_ROTATE_90_CCW:  /* Rotate CCW */; break;
        case ORIENT_TILT_FORWARD:   /* Scroll down */; break;
        case ORIENT_TILT_BACKWARD:  /* Scroll up */; break;
        case ORIENT_FACE_DOWN:      /* Mute */; break;
        case ORIENT_SHAKE_X:        /* Undo */; break;
        default: break;
    }
}

void handleLegacyGesture(int gestureID)
{
    Logger::getInstance().log("Legacy gesture ID: " + String(gestureID));
    // Handle based on trained gesture ID
}

// ============================================================================
// Main Loop Integration Example
// ============================================================================

void loop_gesture_example()
{
    static bool gestureActive = false;
    static unsigned long gestureStartTime = 0;

    // Check for gesture trigger (e.g., button press)
    if (digitalRead(GESTURE_BUTTON_PIN) == LOW && !gestureActive) {
        gestureActive = true;
        gestureStartTime = millis();

        gestureAnalyzer.setRecognitionMode(MODE_AUTO);
        gestureSensor.startSampling();

        Logger::getInstance().log("Gesture recording started...");
    }

    // Update sampling while active
    if (gestureActive) {
        gestureSensor.updateSampling();

        // Auto-stop after 3 seconds or buffer full
        if (!gestureSensor.isSampling() || (millis() - gestureStartTime > 3000)) {
            gestureSensor.stopSampling();

            // Recognize gesture
            GestureResult result = gestureAnalyzer.recognizeGesture();

            if (result.gestureID >= 0 && result.confidence >= 0.5f) {
                Logger::getInstance().log("Gesture: " + String(result.getName()) +
                                         " (" + String(result.confidence * 100, 0) + "%)");
                handleGestureResult(result);
            } else {
                Logger::getInstance().log("No gesture recognized");
            }

            gestureAnalyzer.clearSamples();
            gestureActive = false;
        }
    }
}
