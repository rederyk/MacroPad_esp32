/*
 * ESP32 MacroPad Project
 *
 * Shared declaration for the HTTP/web scheduler special action dispatcher.
 */

#ifndef SPECIAL_ACTION_ROUTER_H
#define SPECIAL_ACTION_ROUTER_H

#include <Arduino.h>
#include <ArduinoJson.h>

// Implemented in lib/configWebServer/configWebServer.cpp
bool handleSpecialActionRequest(const String &actionId,
                                JsonVariantConst params,
                                String &message,
                                int &statusCode);

#endif // SPECIAL_ACTION_ROUTER_H
