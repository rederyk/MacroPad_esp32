#ifndef GESTURENORMALIZE_H
#define GESTURENORMALIZE_H

#include "gestureRead.h"

// 3D vector structure for mathematical operations
struct Vector3D {
    float x, y, z;
    float v[3]; // Array access for convenience
    
    Vector3D() : x(0), y(0), z(0) {
        v[0] = x; v[1] = y; v[2] = z;
    }
    
    Vector3D(float x, float y, float z) : x(x), y(y), z(z) {
        v[0] = x; v[1] = y; v[2] = z;
    }
};

// Normalize rotation of gesture samples
void normalizeRotation(SampleBuffer* buffer);


#endif
