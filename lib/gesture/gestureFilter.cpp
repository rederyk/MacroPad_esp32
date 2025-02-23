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
