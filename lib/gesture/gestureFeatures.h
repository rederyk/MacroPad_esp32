#ifndef GESTUREFEATURES_H
#define GESTUREFEATURES_H

#include "gestureRead.h"

// Number of features per axis
#define FEATURES_PER_AXIS 5
#define TOTAL_FEATURES (FEATURES_PER_AXIS * 3) // x, y, z axes

void extractFeatures(SampleBuffer *buffer, float *features);

#endif
