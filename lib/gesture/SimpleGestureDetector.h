/*
 * ESP32 MacroPad Project
 *
 * Simple and efficient gesture detection using gyroscope and accelerometer data.
 * Optimized for fast, reliable recognition without unnecessary complexity.
 */

#ifndef SIMPLE_GESTURE_DETECTOR_H
#define SIMPLE_GESTURE_DETECTOR_H

#include "IGestureRecognizer.h"

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
