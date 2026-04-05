#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <Preferences.h>
#include "constants.h"

// ==========================================
// CONFIGURATION CLASS
// ==========================================
class Config {
public:
  // WiFi Configuration
  static char ssid[SSID_MAX_LEN];
  static char password[PASSWORD_MAX_LEN];
  static char hostname[HOSTNAME_MAX_LEN];
  static bool use_static_ip;
  static char static_ip[IP_ADDRESS_MAX_LEN];
  static char static_gateway[IP_ADDRESS_MAX_LEN];
  static char static_subnet[IP_ADDRESS_MAX_LEN];
  static char static_dns[IP_ADDRESS_MAX_LEN];
  
  // MQTT Configuration
  static char mqtt_server[MQTT_SERVER_MAX_LEN];
  static int mqtt_port;
  static char mqtt_user[MQTT_USER_MAX_LEN];
  static char mqtt_pass[MQTT_PASS_MAX_LEN];
  static char mqtt_topic[MQTT_TOPIC_MAX_LEN];
  static char mqtt_availability_topic[MQTT_TOPIC_MAX_LEN];
  static char mqtt_client_id[MQTT_CLIENT_ID_MAX_LEN];
  
  // Gas Configuration
  static unsigned long poll_interval;
  static float gas_calorific_value;
  static float gas_correction_factor;
  
  // Cost Configuration
  static float gas_base_price_monthly;     // Grundpreis pro Monat in €
  static float gas_work_price_per_kwh;     // Arbeitspreis pro kWh in €
  
  // Deep Sleep Configuration
  static bool enable_deep_sleep;
  static unsigned long deep_sleep_duration;
  
  // Methods
  static void init();
  static void load();
  static void save();
  static void setDefaults();
  static void generateMqttClientId();
  
  // Backup & Restore
  static String exportToJson(bool includePasswords = false);
  static bool importFromJson(const String& json, String* errorMsg = nullptr);
  static bool validateJson(const String& json, String* errorMsg = nullptr);
  
private:
  static Preferences preferences;
  static bool validatePollInterval();
};

#endif // CONFIG_H
