
#include "gestureAnalyze.h"
#include "gestureNormalize.h"
#include "gestureFeatures.h"
#include "gestureStorage.h"
#include "Logger.h"
#include <cfloat>
#include <unordered_map>



GestureAnalyze::GestureAnalyze(GestureRead& gestureReader)
    : _gestureReader(gestureReader), _isAnalyzing(false)
{
}


SampleBuffer& GestureAnalyze::getFiltered()
{
    // Get reference to collected samples
    SampleBuffer& samples = getRawSample();
    
    // Apply low-pass filter with 5Hz cutoff frequency
    applyLowPassFilter(&samples, 5.0f);
    // normalizeRotation expects a SampleBuffer* pointer
    normalizeRotation(&samples);
    
    return samples;
}

SampleBuffer& GestureAnalyze::getRawSample()
{
    return _gestureReader.getCollectedSamples();
}

void GestureAnalyze::clearSamples()
{
    _gestureReader.clearMemory();
}

void GestureAnalyze::normalizeRotation(SampleBuffer* buffer) {
    ::normalizeRotation(buffer);
}

void GestureAnalyze::extractFeatures(SampleBuffer *buffer, float *features) {
    ::extractFeatures(buffer, features);
}

bool GestureAnalyze::saveFeaturesWithID(uint8_t gestureID) {
    // Check available memory before proceeding
    size_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < 10240) { // 10KB minimum
        Logger::getInstance().log("[Error] Insufficient memory to save features");
        return false;
    }

    GestureStorage storage;
    float features[MAX_FEATURES];
    
    // Get filtered samples
    SampleBuffer& buffer = getFiltered();
    
    // Verify buffer is valid
    if (buffer.sampleCount == 0 || !buffer.samples) {
        Logger::getInstance().log("[Error] No valid samples collected");
        return false;
    }

    // Check memory before feature extraction
    if (ESP.getFreeHeap() < 8192) { // 8KB buffer
        Logger::getInstance().log("[Warning] Low memory before feature extraction");
    }

    // Extract features
    extractFeatures(&buffer, features);
    
    // Check memory before saving
    if (ESP.getFreeHeap() < 6144) { // 6KB buffer
        Logger::getInstance().log("[Warning] Low memory before saving features");
    }

    // Save features with ID
    bool result = storage.saveGestureFeature(gestureID, features, MAX_FEATURES);
    
    // Log memory status after operation

    Logger::getInstance().log("[Debug] Memory after save: "+String(ESP.getFreeHeap())+" bytes");

    return result;
}

int GestureAnalyze::findKNNMatch(int k) {
    // Load feature matrix from binary
    GestureStorage storage;
    size_t numGestures, numSamples, numFeatures;
    float*** matrix = storage.loadMatrixFromBinary("/gestures.bin", numGestures, numSamples, numFeatures);
    
    if (!matrix || numGestures == 0) {
        Logger::getInstance().log("[Error] Failed to load feature matrix");
        return -1;
    }

    // Get filtered samples and extract features
    SampleBuffer& buffer = getFiltered();
    float queryFeatures[MAX_FEATURES];
    extractFeatures(&buffer, queryFeatures);

    // Calculate distances to all stored gestures using matrix index as ID
    std::vector<std::pair<float, int>> distances;
    for (size_t i = 0; i < numGestures; i++) {
        // Use matrix index as gesture ID
        const int gestureID = i;
        //Logger::getInstance().log("Processing Gesture ID: "+String(gestureID));

        // Check if all samples for this gesture ID are filled with 0.0
        bool allZero = true;
        for (size_t s = 0; s < numSamples; s++) {
            for (size_t f = 0; f < numFeatures; f++) {
                if (matrix[i][s][f] != 0.0f) {
                    allZero = false;
                    break;
                }
            }
            if (!allZero) break;
        }

        if (allZero) {
            //Logger::getInstance().log("[Info] Skipping gesture ID because all samples are 0.0");
            continue;
        }

        float minDist = FLT_MAX;
        
        // Find minimum distance across samples
        for (size_t s = 0; s < numSamples; s++) {
            float dist = 0.0f;
            for (size_t f = 0; f < numFeatures; f++) {
                float diff = queryFeatures[f] - matrix[i][s][f];
                dist += diff * diff;
            }
            if (dist < minDist) minDist = dist;
        }
        
        distances.emplace_back(sqrt(minDist), gestureID);
    }

    // Sort distances and find k nearest neighbors
    std::sort(distances.begin(), distances.end());

    Logger::getInstance().log("Distances sorted:");
    for (const auto& entry : distances) {
        float dist = entry.first;
        int id = entry.second;
        Logger::getInstance().log("Distance: "+String(dist)+", ID: "+String(id));
    }

    // Set threshold for minimum distance
    const float distanceThreshold = 100.0f;
    
    // If best match exceeds threshold, return empty string (represented as -2)
    if (distances.empty() || distances[0].first > distanceThreshold) {
        Logger::getInstance().log("No match found within threshold distance");
        
        // Cleanup matrix memory
        for (size_t i = 0; i < numGestures; i++) {
            for (size_t s = 0; s < numSamples; s++) {
                delete[] matrix[i][s];
            }
            delete[] matrix[i];
        }
        delete[] matrix;
        
        return -2;  // Special code to indicate "empty string" result
    }

    // Count votes among top k matches
    std::unordered_map<int, int> votes;
    for (int i = 0; i < std::min(k, static_cast<int>(distances.size())); i++) {
        votes[distances[i].second]++;
    }

    // Find gesture with most votes, using smallest distance for ties
    int bestMatch = -1;
    int maxVotes = 0;
    float minDistance = FLT_MAX;

    for (const auto& entry : votes) {
        int id = entry.first;
        int count = entry.second;
        
        // Find the smallest distance for this gesture
        float currDist = FLT_MAX;
        for (const auto& d : distances) {
            if (d.second == id && d.first < currDist) {
                currDist = d.first;
            }
        }

        if (count > maxVotes || (count == maxVotes && currDist < minDistance)) {
            maxVotes = count;
            bestMatch = id;
            minDistance = currDist;
        }
    }
    Logger::getInstance().log("Best Match ID: "+String(bestMatch));

    // Cleanup matrix memory
    for (size_t i = 0; i < numGestures; i++) {
        for (size_t s = 0; s < numSamples; s++) {
            delete[] matrix[i][s];
        }
        delete[] matrix[i];
    }
    delete[] matrix;

    return bestMatch;
}
