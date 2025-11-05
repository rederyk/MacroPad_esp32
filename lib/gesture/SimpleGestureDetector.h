#ifndef SIMPLE_GESTURE_DETECTOR_H
#define SIMPLE_GESTURE_DETECTOR_H

#include "gestureRead.h"

/**
 * Sensor-specific gesture recognition type
 */
enum SensorGestureMode {
    SENSOR_MODE_MPU6050 = 0,    // MPU6050: Shape + Orientation (gyro available)
    SENSOR_MODE_ADXL345,        // ADXL345: Legacy KNN (no gyro)
    SENSOR_MODE_AUTO            // Auto-detect based on sensor type
};

/**
 * Unified gesture result structure
 */
struct GestureRecognitionResult {
    int gestureID;              // Gesture ID (-1 if unknown, 0-8 for custom, 100+ for predefined)
    float confidence;           // Confidence score 0-1
    SensorGestureMode sensorMode; // Which sensor mode was used
    String gestureName;         // Human-readable name

    // Mode-specific data (optional)
    void* modeSpecificData;     // Can hold ShapeType, OrientationType, etc.

    GestureRecognitionResult()
        : gestureID(-1), confidence(0.0f), sensorMode(SENSOR_MODE_AUTO),
          gestureName("G_UNKNOWN"), modeSpecificData(nullptr) {}
};

struct SimpleGestureConfig {
    const char *sensorTag;          // Short identifier used in logs
    SensorGestureMode sensorMode;   // Returned in GestureRecognitionResult
    bool useGyro;                   // true for MPU6050, false for ADXL345

    // Gyro-based detection (MPU6050)
    float gyroSwipeThreshold;       // Minimum gyro peak for swipe (deg/s)
    float gyroShakeThreshold;       // Minimum gyro peak for shake (deg/s)

    // Accel-based detection (ADXL345 fallback)
    float accelSwipeThreshold;      // Minimum accel range for swipe (g)
    float accelShakeThreshold;      // Minimum accel range for shake (g)

    SimpleGestureConfig()
        : sensorTag("Unknown"),
          sensorMode(SENSOR_MODE_AUTO),
          useGyro(false),
          gyroSwipeThreshold(100.0f),    // deg/s - easy to trigger
          gyroShakeThreshold(150.0f),    // deg/s - needs more motion
          accelSwipeThreshold(0.6f),     // g
          accelShakeThreshold(1.2f) {}   // g
};

/**
 * Simple, fast gesture detection
 * Uses gyroscope if available (MPU6050), otherwise accelerometer (ADXL345)
 */
GestureRecognitionResult detectSimpleGesture(SampleBuffer *buffer, const SimpleGestureConfig &config);

#endif // SIMPLE_GESTURE_DETECTOR_H
