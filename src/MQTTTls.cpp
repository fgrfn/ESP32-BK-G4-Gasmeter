#include "MQTTTls.h"

// Static member initialization
WiFiClientSecure* MQTTTls::secureClient = nullptr;
bool MQTTTls::tlsEnabled = false;

void MQTTTls::init(WiFiClientSecure& client) {
  secureClient = &client;
  tlsEnabled = false;
}

void MQTTTls::setCACert(const char* caCert) {
  if (secureClient && caCert) {
    secureClient->setCACert(caCert);
    tlsEnabled = true;
    Serial.println("MQTT TLS: CA-Zertifikat geladen");
  }
}

void MQTTTls::setClientCert(const char* clientCert, const char* clientKey) {
  if (secureClient && clientCert && clientKey) {
    secureClient->setCertificate(clientCert);
    secureClient->setPrivateKey(clientKey);
    tlsEnabled = true;
    Serial.println("MQTT TLS: Client-Zertifikat geladen");
  }
}

void MQTTTls::setInsecure(bool insecure) {
  if (secureClient) {
    secureClient->setInsecure();
    tlsEnabled = true;
    if (insecure) {
      Serial.println("MQTT TLS: Insecure-Modus (keine Zertifikatspruefung!)");
    }
  }
}
