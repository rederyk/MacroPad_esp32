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


#ifndef GESTUREANALYZE_H
#define GESTUREANALYZE_H

#include "gestureRead.h"
#include "gestureNormalize.h"
#include "gestureFilter.h"
#include "gestureFeatures.h"
#include <vector>

struct GestureAnalysis
{
    SampleBuffer samples;
};

class GestureAnalyze
{
public:
    GestureAnalyze(GestureRead &gestureReader);

    // Memory management
    void clearSamples();

    // Process and return analysis data
    SampleBuffer& getFiltered();

    SampleBuffer& getRawSample();

    // Normalize rotation of gesture samples
    void normalizeRotation(SampleBuffer *buffer);

    void extractFeatures(SampleBuffer *buffer, float *features);

    // Save extracted features with gesture ID
    bool saveFeaturesWithID(uint8_t gestureID);

    int findKNNMatch(int k);

private:
    GestureRead &_gestureReader;
    bool _isAnalyzing;
    SampleBuffer _samples;
};

extern GestureAnalyze gestureAnalyzer;  // Dichiarazione extern per l'istanza globale

#endif
