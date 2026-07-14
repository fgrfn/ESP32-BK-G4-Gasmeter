#pragma once

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

class WebServerManager {
 public:
  static void begin(bool safeMode);
  static void loop();

 private:
  static bool authorize(AsyncWebServerRequest* request, bool allowSetupAp = true);
  static void sendJson(AsyncWebServerRequest* request, JsonDocument& document, int status = 200);
  static void registerRoutes();
  static AsyncWebServer server_;
  static bool safeMode_;
  static uint32_t restartAt_;
};
