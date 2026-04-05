#include "WebAuth.h"

// Static member initialization
bool WebAuth::authEnabled = false;
char WebAuth::authUsername[32] = "admin";
char WebAuth::authPassword[64] = "";

void WebAuth::init(const char* username, const char* password) {
  if (username && strlen(username) > 0) {
    strncpy(authUsername, username, sizeof(authUsername) - 1);
    authUsername[sizeof(authUsername) - 1] = '\0';
  }
  
  if (password && strlen(password) > 0) {
    strncpy(authPassword, password, sizeof(authPassword) - 1);
    authPassword[sizeof(authPassword) - 1] = '\0';
    authEnabled = true;
  } else {
    authEnabled = false;
  }
}

void WebAuth::setCredentials(const char* user, const char* pass) {
  if (user) {
    strncpy(authUsername, user, sizeof(authUsername) - 1);
    authUsername[sizeof(authUsername) - 1] = '\0';
  }
  
  if (pass && strlen(pass) > 0) {
    strncpy(authPassword, pass, sizeof(authPassword) - 1);
    authPassword[sizeof(authPassword) - 1] = '\0';
    authEnabled = true;
  } else {
    authEnabled = false;
  }
}

bool WebAuth::authenticate(WebServer& server) {
  if (!authEnabled) {
    return true; // No auth required
  }
  
  if (!server.authenticate(authUsername, authPassword)) {
    requestAuthentication(server);
    return false;
  }
  
  return true;
}

void WebAuth::requestAuthentication(WebServer& server) {
  server.requestAuthentication(DIGEST_AUTH, "ESP32 Gaszaehler", 
                                 "Authentifizierung erforderlich");
}
