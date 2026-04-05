#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include "constants.h"

// ==========================================
// WIFI MANAGER CLASS
// ==========================================
class WiFiManager {
public:
  static void init();
  static bool connect();
  static void startAPMode();
  static bool isAPMode() { return apMode; }
  static bool isConnected() { return WiFi.status() == WL_CONNECTED; }
  static void checkConnection();
  static String getLocalIP();
  static int getRSSI();
  
private:
  static bool apMode;
  static unsigned long apModeStartTime;
  
  static bool configureStaticIP();
};

#endif // WIFI_MANAGER_H
