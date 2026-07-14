#pragma once

#include <Arduino.h>
#include "version.h"

namespace Constants {
constexpr uint8_t STATUS_LED_PIN = 2;
constexpr uint8_t RESET_BUTTON_PIN = 0;
constexpr uint8_t MBUS_RX_PIN = 16;
constexpr uint8_t MBUS_TX_PIN = 17;
constexpr uint32_t MBUS_BAUD = 2400;
constexpr uint32_t MBUS_RESPONSE_TIMEOUT_MS = 750;
constexpr size_t MBUS_BUFFER_SIZE = 256;
constexpr uint32_t DEFAULT_POLL_INTERVAL_MS = 30000;
constexpr uint32_t MIN_POLL_INTERVAL_MS = 10000;
constexpr uint32_t MAX_POLL_INTERVAL_MS = 3600000;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;
constexpr uint32_t MQTT_RECONNECT_MAX_MS = 300000;
constexpr uint32_t STABLE_BOOT_AFTER_MS = 60000;
constexpr uint8_t SAFE_MODE_BOOT_THRESHOLD = 3;
constexpr size_t MAX_LOG_ENTRIES = 50;
constexpr size_t MAX_JSON_BODY = 8192;
constexpr size_t MQTT_BUFFER_SIZE = 2048;
constexpr float DEFAULT_CALORIFIC_VALUE = 10.0f;
constexpr float DEFAULT_CORRECTION_FACTOR = 1.0f;
constexpr float DEFAULT_MAX_FLOW_M3H = 25.0f;
constexpr char DEFAULT_HOSTNAME[] = "esp32-gasmeter";
constexpr char DEFAULT_MQTT_TOPIC[] = "gasmeter";
constexpr char DEFAULT_TIMEZONE[] = "CET-1CEST,M3.5.0,M10.5.0/3";
constexpr char NTP_SERVER_1[] = "de.pool.ntp.org";
constexpr char NTP_SERVER_2[] = "pool.ntp.org";
}
