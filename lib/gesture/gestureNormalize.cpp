#include "gestureNormalize.h"
#include <cmath>

// Helper function to calculate mean of vectors
static Vector3D calculateMean(const SampleBuffer* buffer) {
    Vector3D mean = {0, 0, 0};
    for (uint16_t i = 0; i < buffer->sampleCount; i++) {
        mean.x += buffer->samples[i].x;
        mean.y += buffer->samples[i].y;
        mean.z += buffer->samples[i].z;
    }
    mean.x /= buffer->sampleCount;
    mean.y /= buffer->sampleCount;
    mean.z /= buffer->sampleCount;
    return mean;
}

// Helper function to calculate covariance matrix
static void calculateCovariance(const SampleBuffer* buffer, const Vector3D& mean, float covariance[3][3]) {
    for (uint16_t i = 0; i < buffer->sampleCount; i++) {
        Vector3D diff = {
            buffer->samples[i].x - mean.x,
            buffer->samples[i].y - mean.y,
            buffer->samples[i].z - mean.z
        };
        
        for (int j = 0; j < 3; j++) {
            for (int k = 0; k < 3; k++) {
                covariance[j][k] += diff.v[j] * diff.v[k];
            }
        }
    }
    
    for (int j = 0; j < 3; j++) {
        for (int k = 0; k < 3; k++) {
            covariance[j][k] /= (buffer->sampleCount - 1);
        }
    }
}

// Helper function to project vector onto plane
static Vector3D projectOntoPlane(const Vector3D& v, const Vector3D& normal) {
    float dot = v.x * normal.x + v.y * normal.y + v.z * normal.z;
    return {
        v.x - dot * normal.x,
        v.y - dot * normal.y,
        v.z - dot * normal.z
    };
}

void normalizeRotation(SampleBuffer* buffer) {
    if (buffer == nullptr || buffer->sampleCount == 0) return;

    // Step 1: Extract main plane using PCA
    Vector3D mean = calculateMean(buffer);
    
    float covariance[3][3] = {0};
    calculateCovariance(buffer, mean, covariance);
    
    // Find the eigenvector with smallest eigenvalue (normal to the plane)
    Vector3D normal = {
        covariance[1][0] * covariance[2][1] - covariance[1][1] * covariance[2][0],
        covariance[2][0] * covariance[0][1] - covariance[2][1] * covariance[0][0],
        covariance[0][0] * covariance[1][1] - covariance[0][1] * covariance[1][0]
    };
    
    // Normalize the normal vector
    float length = sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    normal.x /= length;
    normal.y /= length;
    normal.z /= length;

    // Step 2: Project points onto the plane
    for (uint16_t i = 0; i < buffer->sampleCount; i++) {
        Vector3D sample = {buffer->samples[i].x, buffer->samples[i].y, buffer->samples[i].z};
        Vector3D projected = projectOntoPlane(sample, normal);
        buffer->samples[i].x = projected.x;
        buffer->samples[i].y = projected.y;
        buffer->samples[i].z = projected.z;
    }

    // Step 3: Normalize rotation within the plane
    // Calculate centroid of projected points
    Vector3D centroid = calculateMean(buffer);
    
    // Calculate rotation angle to align with x-axis
    float sumX = 0, sumY = 0;
    for (uint16_t i = 0; i < buffer->sampleCount; i++) {
        sumX += buffer->samples[i].x - centroid.x;
        sumY += buffer->samples[i].y - centroid.y;
    }
    float angle = atan2(sumY, sumX);
    
    // Rotate points to align with x-axis
    float cosA = cos(-angle);
    float sinA = sin(-angle);
    for (uint16_t i = 0; i < buffer->sampleCount; i++) {
        float x = buffer->samples[i].x - centroid.x;
        float y = buffer->samples[i].y - centroid.y;
        
        buffer->samples[i].x = x * cosA - y * sinA + centroid.x;
        buffer->samples[i].y = x * sinA + y * cosA + centroid.y;
    }
}
