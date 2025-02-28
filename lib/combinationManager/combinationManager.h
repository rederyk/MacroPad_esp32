// combinationManager.h
#ifndef COMBINATION_MANAGER_H
#define COMBINATION_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>

class CombinationManager {
private:
    StaticJsonDocument<4096> doc;
    JsonObject combinations;
    
public:
    CombinationManager();
    bool loadCombinations(int setNumber = 0);
    JsonObject getCombinations();
};

#endif // COMBINATION_MANAGER_H
