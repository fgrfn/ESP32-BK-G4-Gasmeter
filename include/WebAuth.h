#ifndef WEB_AUTH_H
#define WEB_AUTH_H

#include <Arduino.h>
#include <WebServer.h>
#include "constants.h"

// ==========================================
// WEB AUTHENTICATION
// ==========================================
class WebAuth {
public:
  static void init(const char* username = "admin", const char* password = "");
  static bool authenticate(WebServer& server);
  static void requestAuthentication(WebServer& server);
  static bool isEnabled() { return authEnabled; }
  static void setEnabled(bool enabled) { authEnabled = enabled; }
  static void setCredentials(const char* user, const char* pass);
  static const char* getUsername() { return authUsername; }
  
private:
  static bool authEnabled;
  static char authUsername[32];
  static char authPassword[64];
};

#endif // WEB_AUTH_H
