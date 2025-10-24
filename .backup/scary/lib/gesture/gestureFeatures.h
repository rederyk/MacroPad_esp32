/*
 * ESP32 MacroPad Project
 * Copyright (C) [2025] [Enrico Mori]
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


#ifndef GESTUREFEATURES_H
#define GESTUREFEATURES_H

#include "gestureRead.h"

// Number of features per axis (mean, stddev, energy, max, min)
#define FEATURES_PER_AXIS 5

// Accelerometer contributes 3 axes; gyroscope uses a magnitude summary
#define TOTAL_ACCEL_FEATURES (FEATURES_PER_AXIS * 3)
#define GYRO_MAG_FEATURES FEATURES_PER_AXIS
#define TOTAL_FEATURES (TOTAL_ACCEL_FEATURES + GYRO_MAG_FEATURES)

void extractFeatures(SampleBuffer *buffer, float *features);

#endif
