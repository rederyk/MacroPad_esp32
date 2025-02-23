#ifndef GESTUREFILTER_H
#define GESTUREFILTER_H

#include "gestureRead.h"

// Apply low-pass filter to buffer samples
void applyLowPassFilter(SampleBuffer* buffer, float cutoffFrequency);

#endif
