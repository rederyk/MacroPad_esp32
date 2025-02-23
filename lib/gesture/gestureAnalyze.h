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
