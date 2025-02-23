#include "featureNormalyze.h"
#include <cmath>

void normalizeFeatures(float*** matrix, size_t numGestures, size_t numSamples, size_t numFeatures)
{
    // Normalize each feature across all gestures and samples
    for (size_t f = 0; f < numFeatures; f++) {
        float minVal = INFINITY;
        float maxVal = -INFINITY;

        // Find min and max for current feature
        for (size_t g = 0; g < numGestures; g++) {
            for (size_t s = 0; s < numSamples; s++) {
                float val = matrix[g][s][f];
                if (val < minVal) minVal = val;
                if (val > maxVal) maxVal = val;
            }
        }

        // Avoid division by zero
        if (maxVal == minVal) {
            continue;
        }

        // Normalize values
        for (size_t g = 0; g < numGestures; g++) {
            for (size_t s = 0; s < numSamples; s++) {
                matrix[g][s][f] = (matrix[g][s][f] - minVal) / (maxVal - minVal);
            }
        }
    }
}

void standardizeFeatures(float*** matrix, size_t numGestures, size_t numSamples, size_t numFeatures)
{
    // Standardize each feature to have mean=0 and stddev=1
    for (size_t f = 0; f < numFeatures; f++) {
        float sum = 0.0f;
        float sumSq = 0.0f;
        size_t totalValues = numGestures * numSamples;

        // Calculate mean and standard deviation
        for (size_t g = 0; g < numGestures; g++) {
            for (size_t s = 0; s < numSamples; s++) {
                float val = matrix[g][s][f];
                sum += val;
                sumSq += val * val;
            }
        }

        float mean = sum / totalValues;
        float variance = (sumSq / totalValues) - (mean * mean);
        float stdDev = sqrt(variance);

        // Avoid division by zero
        if (stdDev == 0) {
            continue;
        }

        // Standardize values
        for (size_t g = 0; g < numGestures; g++) {
            for (size_t s = 0; s < numSamples; s++) {
                matrix[g][s][f] = (matrix[g][s][f] - mean) / stdDev;
            }
        }
    }
}
