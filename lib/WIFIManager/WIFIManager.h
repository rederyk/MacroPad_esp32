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


#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>

#include <DNSServer.h> // Include the DNS server library
#include "configWebServer.h"

class WIFIManager
{
public:
    WIFIManager();
    void beginAP(const char *ssid, const char *password);                                // Start the Access Point
    void stopAP();                                                                       // Stop the Access Point
    void enableAP(const char *ssid = "ESP32-Numpad", const char *password = "password"); // Enable the Access Point
    bool isAPMode();
    void toggleAp(const char *ssid, const char *apass);
    void connectWiFi(const char *ssid, const char *password, const char *hostname = nullptr);
    void stopWiFi();
    void enableSTA(const char *ssid, const char *password, const char *hostname = nullptr);
    void disableSTA();
    void toggleSta(const char *ssid, const char *password);
    void updateWiFiMode();
    void startWebServer();
    void stopWebServer();
    void updateStatus();
    void setWifiStaticMacAddress();
    bool isConnected();

private:
    bool apEnabled;
    bool staEnabled;
    DNSServer dnsServer;
    configWebServer webServer;
    bool webServerRunning;
};

#endif // WIFI_MANAGER_H
