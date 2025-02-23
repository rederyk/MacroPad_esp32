#include "gestureFeatures.h"
#include <math.h>

void extractFeatures(SampleBuffer *buffer, float *features) {
    if (buffer->sampleCount == 0) {
        return;
    }

    // Variables to hold intermediate values
    float sumX = 0, sumY = 0, sumZ = 0;
    float sumSqX = 0, sumSqY = 0, sumSqZ = 0;
    float maxX = -INFINITY, maxY = -INFINITY, maxZ = -INFINITY;
    float minX = INFINITY, minY = INFINITY, minZ = INFINITY;
    int zeroCrossingsX = 0, zeroCrossingsY = 0, zeroCrossingsZ = 0;

    // Previous samples for zero-crossing calculation
    float prevX = 0, prevY = 0, prevZ = 0;
    int isFirstSample = 1;

    // Iterate through all samples
    for (int i = 0; i < buffer->sampleCount; i++) {
        float x = buffer->samples[i].x;
        float y = buffer->samples[i].y;
        float z = buffer->samples[i].z;

        // Accumulate sums and squared sums
        sumX += x;
        sumY += y;
        sumZ += z;

        sumSqX += x * x;
        sumSqY += y * y;
        sumSqZ += z * z;

        // Update max and min values
        if (x > maxX) maxX = x;
        if (y > maxY) maxY = y;
        if (z > maxZ) maxZ = z;

        if (x < minX) minX = x;
        if (y < minY) minY = y;
        if (z < minZ) minZ = z;

        // Calculate zero-crossings
        if (!isFirstSample) {
            if ((x > 0 && prevX < 0) || (x < 0 && prevX > 0)) zeroCrossingsX++;
            if ((y > 0 && prevY < 0) || (y < 0 && prevY > 0)) zeroCrossingsY++;
            if ((z > 0 && prevZ < 0) || (z < 0 && prevZ > 0)) zeroCrossingsZ++;
        }

        prevX = x;
        prevY = y;
        prevZ = z;
        isFirstSample = 0;
    }

    // Calculate means
    float meanX = sumX / buffer->sampleCount;
    float meanY = sumY / buffer->sampleCount;
    float meanZ = sumZ / buffer->sampleCount;

    // Calculate standard deviations
    float stdDevX = sqrt(sumSqX / buffer->sampleCount - meanX * meanX);
    float stdDevY = sqrt(sumSqY / buffer->sampleCount - meanY * meanY);
    float stdDevZ = sqrt(sumSqZ / buffer->sampleCount - meanZ * meanZ);

    // Populate the feature vector
    features[0] = meanX;          // Mean X
    features[1] = stdDevX;        // Std Dev X
    features[2] = sumSqX;         // Energy X
    features[3] = maxX;           // Max X
    features[4] = minX;           // Min X

    features[5] = meanY;          // Mean Y
    features[6] = stdDevY;        // Std Dev Y
    features[7] = sumSqY;         // Energy Y
    features[8] = maxY;           // Max Y
    features[9] = minY;           // Min Y

    features[10] = meanZ;         // Mean Z
    features[11] = stdDevZ;       // Std Dev Z
    features[12] = sumSqZ;        // Energy Z
    features[13] = maxZ;          // Max Z
    features[14] = minZ;          // Min Z
}
