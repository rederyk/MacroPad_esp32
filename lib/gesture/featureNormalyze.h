#ifndef FEATURE_NORMALYZE_H
#define FEATURE_NORMALYZE_H

#include <cstddef>

void normalizeFeatures(float*** matrix, size_t numGestures, size_t numSamples, size_t numFeatures);
void standardizeFeatures(float*** matrix, size_t numGestures, size_t numSamples, size_t numFeatures);

#endif // FEATURE_NORMALYZE_H
