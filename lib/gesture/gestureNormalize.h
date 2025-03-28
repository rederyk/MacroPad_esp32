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
