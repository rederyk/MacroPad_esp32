# Gesture System Overview (Swipe & Shake Only)

## Current Architecture

- **GestureRead**  
  Handles raw sampling from `MotionSensor`. Provides calibrated accelerometer (and gyro if available) data and manages the capture buffer.

- **GestureAnalyze**  
  Instantiates the proper recognizer based on the configured sensor (`mpu6050` or `adxl345`) and exposes a single `recognizeWithRecognizer()` entrypoint. All gesture modes now map to the single `Swipe+Shake` pipeline.

- **Recognizers**
  1. `MPU6050GestureRecognizer` – Uses accelerometer + gyroscope data. Tracks swipe left/right and shake using the shared detector.
  2. `ADXL345GestureRecognizer` – Accelerometer-only version of the same detector.

- **SimpleGestureDetector**  
  Shared helper that analyses the captured samples, computes axis ranges, and classifies gestures as:
    - `G_SWIPE_LEFT`
    - `G_SWIPE_RIGHT`
    - `G_SHAKE`
  The detector logs detailed diagnostics (axis ranges, gravity axis, confidence) and returns a unified `GestureRecognitionResult`.

## Supported Gestures

| Gesture ID | Name            | Notes                              |
|------------|-----------------|------------------------------------|
| 201        | `G_SWIPE_RIGHT` | Dominant axis positive excursion   |
| 202        | `G_SWIPE_LEFT`  | Dominant axis negative excursion   |
| 203        | `G_SHAKE`       | Bidirectional movement > threshold |

The MPU6050 recognizer uses gyroscope data to refine swipe direction, while ADXL345 relies solely on accelerometer readings. Legacy shape/orientation recognizers and their helper utilities (filters, Madgwick integration, etc.) have been removed.

## Logging

- MPU6050 recognizer reports: `Swipe+Shake (Accel+Gyro)`
- ADXL345 recognizer reports: `Swipe+Shake (Accel only)`

Both recognizers share identical thresholds, ensuring consistent behaviour across sensors with the only distinction being gyro involvement for the MPU6050.
