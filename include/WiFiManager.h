#pragma once

#include <Arduino.h>
#include <DNSServer.h>

class WiFiManager {
 public:
  static void begin(bool forceAccessPoint);
  static void loop();
  static bool connected();
  static bool accessPointMode();
  static const char* accessPointSsid();
  static const char* accessPointPassword();
  static String ipAddress();
  static void reconnect();

 private:
  static void startAccessPoint();
  static DNSServer dnsServer_;
  static bool apMode_;
  static char apSsid_[33];
  static char apPassword_[17];
  static uint32_t lastReconnectAttempt_;
};
