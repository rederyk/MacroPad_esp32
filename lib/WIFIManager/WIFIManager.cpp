#include "WIFIManager.h"
#include <DNSServer.h>
#include <esp_wifi.h>
#include <Logger.h>
//#include <Led.h>

WIFIManager::WIFIManager() : apEnabled(false), staEnabled(false), webServer(), webServerRunning(false)
{
    WiFi.mode(WIFI_OFF); // Inizialmente spento
}

void WIFIManager::setWifiStaticMacAddress()
{
    uint8_t baseMac[6];
    esp_err_t ret_sta = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
    if (ret_sta == ESP_OK)
    {
        Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
                      baseMac[0], baseMac[1], baseMac[2],
                      baseMac[3], baseMac[4], baseMac[5]);
    }
    else
    {
        Logger::getInstance().log("Failed to read MAC address");
    }
    // Change ESP32 Mac Address
    esp_err_t err_sta = esp_wifi_set_mac(WIFI_IF_STA, baseMac);
    esp_err_t err_ap = esp_wifi_set_mac(WIFI_IF_AP, baseMac);
    if (err_sta == ESP_OK || err_ap == ESP_OK)
    {
        Logger::getInstance().log("Success set Base Mac for WIFI");
    }
}

void WIFIManager::startWebServer()
{
    if (!webServerRunning)
    {
        webServer.begin();
        webServerRunning = true;
        Logger::getInstance().log("✅ WebServer avviato.");
    }
}

void WIFIManager::stopWebServer()
{
    if (!apEnabled && !staEnabled && webServerRunning)
    {
        webServer.end();
        webServerRunning = false;
        Logger::getInstance().log("❌ WebServer fermato.");
    }
}

void WIFIManager::beginAP(const char *apSSID, const char *apPassword)
{
    if (apEnabled)
        return;
    apEnabled = true;

    updateWiFiMode();
    delay(500);

    IPAddress apIP(192, 168, 4, 1);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

    WiFi.setTxPower(WIFI_POWER_19_5dBm); // Massima potenza per il segnale

    if (WiFi.softAP(apSSID, apPassword, 1))
    {
        Logger::getInstance().log("✅ AP Mode attivata con successo."
                                  "IP Adress: " +
                                  String(WiFi.softAPIP()));


        wifi_mode_t mode;
        esp_wifi_get_mode(&mode);
        if (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA)
        {
            esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
        }
    }
    else
    {
        Logger::getInstance().log("❌ AP Mode fallita.");
    }

    dnsServer.stop(); // Evita problemi di DNS
    updateStatus();
}

void WIFIManager::stopAP()
{
    if (!apEnabled)
        return;

    Logger::getInstance().log("🔴 Arresto AP Mode...");
    WiFi.softAPdisconnect(true); // Disattiva completamente l'AP

    dnsServer.stop();
    apEnabled = false;

    updateWiFiMode();
    updateStatus();
}

void WIFIManager::enableAP(const char *ssid, const char *password)
{
    if (!apEnabled)
    {
        beginAP(ssid, password);
    }
}

bool WIFIManager::isAPMode()
{
    return apEnabled;
}

void WIFIManager::toggleAp(const char *ssid, const char *password)
{
    if (apEnabled)
    {
        stopAP();
    }
    else
    {
        enableAP(ssid, password);
    }
}

void WIFIManager::connectWiFi(const char *ssid, const char *password)
{
    enableSTA(ssid, password);
}

void WIFIManager::enableSTA(const char *ssid, const char *password)
{
    if (staEnabled)
        return;
    staEnabled = true;

    updateWiFiMode();
    delay(1000);

    WiFi.begin(ssid, password);
    Logger::getInstance().log("🌐 Connessione alla rete WiFi in corso...");
    // Definisci alcuni colori
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20)
    {
        delay(500);
        Logger::getInstance().log(".", false);
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Logger::getInstance().log("\n✅ WiFi connesso con successo."
                                  "IP Adress: " +
                                  String(WiFi.localIP()));
 //       Led::getInstance().setColor(0, 255, 0);

        updateStatus();
    }
    else
    {
        Logger::getInstance().log("\n❌ Connessione WiFi fallita.");
        staEnabled = false; // Se fallisce, disabilitiamo STA per evitare incoerenze
    }
}

void WIFIManager::disableSTA()
{
    if (!staEnabled)
        return;

    Logger::getInstance().log("🔴 Disconnessione dalla rete WiFi...");
    WiFi.disconnect(true, true); // Disconnette e rimuove credenziali

    staEnabled = false;

    updateWiFiMode();
    updateStatus();
}

void WIFIManager::updateWiFiMode()
{
    static WiFiMode_t lastMode = WIFI_OFF;

    WiFiMode_t newMode;
    if (apEnabled && staEnabled)
    {
        newMode = WIFI_AP_STA;
    }
    else if (apEnabled)
    {
        newMode = WIFI_AP;
    }
    else if (staEnabled)
    {
        newMode = WIFI_STA;
    }
    else
    {
        newMode = WIFI_OFF;
    }

    if (newMode != lastMode)
    {
        WiFi.mode(newMode);
        lastMode = newMode;
        Logger::getInstance().log("🔄 Modalità WiFi aggiornata.");
        delay(100);
    }

    if (apEnabled || staEnabled)
    {
        startWebServer();
    }
    else
    {
        stopWebServer();
    }
}

void WIFIManager::updateStatus()
{
    String apIPAddress = WiFi.softAPIP().toString();
    String staIPAddress = WiFi.localIP().toString();
    String wifiStatus = "WiFi Mode: ";

    WiFiMode_t currentMode = WiFi.getMode();
    if (currentMode == WIFI_AP_STA)
    {
        wifiStatus += "AP + STA";
    }
    else if (currentMode == WIFI_AP)
    {
        wifiStatus += "AP";
    }
    else if (currentMode == WIFI_STA)
    {
        wifiStatus += "STA";
    }
    else
    {
        wifiStatus = "WiFi Off";
    }

    webServer.updateStatus(apIPAddress, staIPAddress, wifiStatus);
    Logger::getInstance().log("📡 Stato WiFi aggiornato: " + wifiStatus);
}

void WIFIManager::stopWiFi()
{
    Logger::getInstance().log("🛑 Arresto completo del WiFi...");
    stopAP();
    disableSTA();
}

void WIFIManager::toggleSta(const char *ssid, const char *password)
{
    if (staEnabled)
    {
        disableSTA();
    }
    else
    {
        enableSTA(ssid, password);
    }
}

bool WIFIManager::isConnected()
{
    return staEnabled && (WiFi.status() == WL_CONNECTED);
}
