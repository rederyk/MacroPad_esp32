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


#include "gestureFilter.h"
#include <math.h>

void applyLowPassFilter(SampleBuffer* buffer, float cutoffFrequency) {
    if (buffer->sampleCount == 0) return;

    // Calculate alpha based on cutoff frequency and sample rate
    float alpha = 2.0f * M_PI * cutoffFrequency / 
                 (2.0f * M_PI * cutoffFrequency + buffer->sampleHZ);
    
    // Initialize with first sample
    Sample previous = buffer->samples[0];
    
    // Apply filter to each sample
    for (uint16_t i = 1; i < buffer->sampleCount; i++) {
        buffer->samples[i].x = alpha * buffer->samples[i].x + 
                              (1 - alpha) * previous.x;
        buffer->samples[i].y = alpha * buffer->samples[i].y + 
                              (1 - alpha) * previous.y;
        buffer->samples[i].z = alpha * buffer->samples[i].z + 
                              (1 - alpha) * previous.z;
        previous = buffer->samples[i];
    }
}
