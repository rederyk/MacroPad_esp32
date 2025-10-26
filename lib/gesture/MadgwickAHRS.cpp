/*
 * ESP32 MacroPad Project
 * Copyright (C) [2025] [Enrico Mori]
 *
 * Madgwick AHRS Filter Implementation
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

#include "MadgwickAHRS.h"

MadgwickAHRS::MadgwickAHRS(float sampleFreq, float beta)
    : _q0(1.0f), _q1(0.0f), _q2(0.0f), _q3(0.0f),
      _beta(beta), _sampleFreq(sampleFreq)
{
}

void MadgwickAHRS::update(float gx, float gy, float gz, float ax, float ay, float az)
{
    float recipNorm;
    float s0, s1, s2, s3;
    float qDot1, qDot2, qDot3, qDot4;
    float _2q0, _2q1, _2q2, _2q3, _4q0, _4q1, _4q2, _8q1, _8q2, q0q0, q1q1, q2q2, q3q3;

    // Rate of change of quaternion from gyroscope
    qDot1 = 0.5f * (-_q1 * gx - _q2 * gy - _q3 * gz);
    qDot2 = 0.5f * (_q0 * gx + _q2 * gz - _q3 * gy);
    qDot3 = 0.5f * (_q0 * gy - _q1 * gz + _q3 * gx);
    qDot4 = 0.5f * (_q0 * gz + _q1 * gy - _q2 * gx);

    // Compute feedback only if accelerometer measurement valid (avoids NaN in accelerometer normalisation)
    if (!((ax == 0.0f) && (ay == 0.0f) && (az == 0.0f)))
    {
        // Normalise accelerometer measurement
        recipNorm = invSqrt(ax * ax + ay * ay + az * az);
        ax *= recipNorm;
        ay *= recipNorm;
        az *= recipNorm;

        // Auxiliary variables to avoid repeated arithmetic
        _2q0 = 2.0f * _q0;
        _2q1 = 2.0f * _q1;
        _2q2 = 2.0f * _q2;
        _2q3 = 2.0f * _q3;
        _4q0 = 4.0f * _q0;
        _4q1 = 4.0f * _q1;
        _4q2 = 4.0f * _q2;
        _8q1 = 8.0f * _q1;
        _8q2 = 8.0f * _q2;
        q0q0 = _q0 * _q0;
        q1q1 = _q1 * _q1;
        q2q2 = _q2 * _q2;
        q3q3 = _q3 * _q3;

        // Gradient descent algorithm corrective step
        s0 = _4q0 * q2q2 + _2q2 * ax + _4q0 * q1q1 - _2q1 * ay;
        s1 = _4q1 * q3q3 - _2q3 * ax + 4.0f * q0q0 * _q1 - _2q0 * ay - _4q1 + _8q1 * q1q1 + _8q1 * q2q2 + _4q1 * az;
        s2 = 4.0f * q0q0 * _q2 + _2q0 * ax + _4q2 * q3q3 - _2q3 * ay - _4q2 + _8q2 * q1q1 + _8q2 * q2q2 + _4q2 * az;
        s3 = 4.0f * q1q1 * _q3 - _2q1 * ax + 4.0f * q2q2 * _q3 - _2q2 * ay;
        recipNorm = invSqrt(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3); // normalise step magnitude
        s0 *= recipNorm;
        s1 *= recipNorm;
        s2 *= recipNorm;
        s3 *= recipNorm;

        // Apply feedback step
        qDot1 -= _beta * s0;
        qDot2 -= _beta * s1;
        qDot3 -= _beta * s2;
        qDot4 -= _beta * s3;
    }

    // Integrate rate of change of quaternion to yield quaternion
    _q0 += qDot1 * (1.0f / _sampleFreq);
    _q1 += qDot2 * (1.0f / _sampleFreq);
    _q2 += qDot3 * (1.0f / _sampleFreq);
    _q3 += qDot4 * (1.0f / _sampleFreq);

    // Normalise quaternion
    recipNorm = invSqrt(_q0 * _q0 + _q1 * _q1 + _q2 * _q2 + _q3 * _q3);
    _q0 *= recipNorm;
    _q1 *= recipNorm;
    _q2 *= recipNorm;
    _q3 *= recipNorm;
}

void MadgwickAHRS::getQuaternion(float &q0, float &q1, float &q2, float &q3) const
{
    q0 = _q0;
    q1 = _q1;
    q2 = _q2;
    q3 = _q3;
}

void MadgwickAHRS::getEulerAngles(float &roll, float &pitch, float &yaw) const
{
    // Roll (x-axis rotation)
    float sinr_cosp = 2.0f * (_q0 * _q1 + _q2 * _q3);
    float cosr_cosp = 1.0f - 2.0f * (_q1 * _q1 + _q2 * _q2);
    roll = atan2f(sinr_cosp, cosr_cosp);

    // Pitch (y-axis rotation)
    float sinp = 2.0f * (_q0 * _q2 - _q3 * _q1);
    if (fabsf(sinp) >= 1.0f)
    {
        // Use 90 degrees if out of range (gimbal lock)
        pitch = copysignf(M_PI / 2.0f, sinp);
    }
    else
    {
        pitch = asinf(sinp);
    }

    // Yaw (z-axis rotation)
    float siny_cosp = 2.0f * (_q0 * _q3 + _q1 * _q2);
    float cosy_cosp = 1.0f - 2.0f * (_q2 * _q2 + _q3 * _q3);
    yaw = atan2f(siny_cosp, cosy_cosp);
}

void MadgwickAHRS::getRotationMatrix(float R[3][3]) const
{
    float q0q0 = _q0 * _q0;
    float q0q1 = _q0 * _q1;
    float q0q2 = _q0 * _q2;
    float q0q3 = _q0 * _q3;
    float q1q1 = _q1 * _q1;
    float q1q2 = _q1 * _q2;
    float q1q3 = _q1 * _q3;
    float q2q2 = _q2 * _q2;
    float q2q3 = _q2 * _q3;
    float q3q3 = _q3 * _q3;

    R[0][0] = 1.0f - 2.0f * (q2q2 + q3q3);
    R[0][1] = 2.0f * (q1q2 - q0q3);
    R[0][2] = 2.0f * (q1q3 + q0q2);

    R[1][0] = 2.0f * (q1q2 + q0q3);
    R[1][1] = 1.0f - 2.0f * (q1q1 + q3q3);
    R[1][2] = 2.0f * (q2q3 - q0q1);

    R[2][0] = 2.0f * (q1q3 - q0q2);
    R[2][1] = 2.0f * (q2q3 + q0q1);
    R[2][2] = 1.0f - 2.0f * (q1q1 + q2q2);
}

void MadgwickAHRS::reset()
{
    _q0 = 1.0f;
    _q1 = 0.0f;
    _q2 = 0.0f;
    _q3 = 0.0f;
}

void MadgwickAHRS::setBeta(float beta)
{
    _beta = beta;
}

void MadgwickAHRS::setSampleFrequency(float freq)
{
    _sampleFreq = freq;
}

float MadgwickAHRS::invSqrt(float x)
{
    // Fast inverse square root using Quake III algorithm
    // Provides ~1% accuracy which is sufficient for filter
    float halfx = 0.5f * x;
    float y = x;
    long i = *(long *)&y;
    i = 0x5f3759df - (i >> 1);
    y = *(float *)&i;
    y = y * (1.5f - (halfx * y * y));
    return y;
}
