
#ifndef SPECIAL_ACTION_H
#define SPECIAL_ACTION_H

#include <Arduino.h>

class SpecialAction
{
public:
    /// System commands
    void resetDevice();
    void actionDelay(int totalDelayMs);

    /// Sensor management
    void calibrateSensor();
    void toggleSampling(bool pressed);

    /// Gesture handling
    bool saveGesture(int id); // Returns success status
    void clearAllGestures();
    void clearGestureWithID(int key = -1);

    /// Data conversion
    bool convertJsonToBinary(); // Returns success status
    void printJson();           // Prints JSON data

    /// Gesture training
    void trainGesture(bool pressed, int key = -1);
    String getGestureID();

    void printMemoryInfo();
    void executeGesture(bool pressed);
    void hopBleDevice();
    void toggleBleWifi();
    void enterSleep();
    void toggleAP(bool toogle);

private:
    int getKeypadInput(unsigned long timeout);
};

#endif
