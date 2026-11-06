#pragma once

#include <ESPAsyncWebServer.h>

void wifiSetupBegin();
void wifiSetupLoop();
void wifiSetupRoutes(AsyncWebServer *server);
