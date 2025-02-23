#ifndef CONFIGWEBSERVER_H
#define CONFIGWEBSERVER_H
#include "specialAction.h"

#include <ESPAsyncWebServer.h>
extern     SpecialAction specialAction;
static bool eventsOutputAdded = false;
class configWebServer {
public:
    configWebServer();
    void begin();
    void end();
    void updateStatus(const String& apIP, const String& staIP, const String& status);
    
private:
    void setupRoutes();
    
    AsyncWebServer server;
    String wifiStatus;
    String apIPAddress;
    String staIPAddress;
    
};

#endif
