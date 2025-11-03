/*
 * ESP32 MacroPad Project
 *
 * Simple swipe/shake gesture classification shared by motion sensors.
 */

#ifndef SIMPLE_GESTURE_DETECTOR_H
#define SIMPLE_GESTURE_DETECTOR_H

#include "IGestureRecognizer.h"

struct SimpleGestureConfig {
    const char *sensorTag;          // Short identifier used in logs
    SensorGestureMode sensorMode;   // Returned in GestureRecognitionResult
    bool useGyro;                   // true for MPU6050, false for ADXL345
    float swipeAccelThreshold;      // Minimum accel range (g) to detect swipe
    float shakeBidirectionalMin;    // Minimum negative peak for shake (g)
    float shakeBidirectionalMax;    // Minimum positive peak for shake (g)
    float shakeRangeThreshold;      // Minimum accel range (g) for shake
};

GestureRecognitionResult detectSimpleGesture(SampleBuffer *buffer, const SimpleGestureConfig &config);

#endif // SIMPLE_GESTURE_DETECTOR_H
