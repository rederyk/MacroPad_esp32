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
    void connectWiFi(const char *ssid, const char *password);
    void stopWiFi();
    void enableSTA(const char *ssid, const char *password);
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
