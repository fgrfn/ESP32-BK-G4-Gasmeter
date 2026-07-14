#include "WebServerManager.h"
#include <ArduinoJson.h>
#include <cstdlib>
#include <cstring>
#include <WiFi.h>
#include <esp_system.h>
#include "BootGuard.h"
#include "Config.h"
#include "Logger.h"
#include "MBusReader.h"
#include "MQTTHandler.h"
#include "TimeManager.h"
#include "UsageTracker.h"
#include "WebUi.h"
#include "WiFiManager.h"
#include "constants.h"

AsyncWebServer WebServerManager::server_(80);
bool WebServerManager::safeMode_ = false;
uint32_t WebServerManager::restartAt_ = 0;

namespace {
constexpr char AUTH_REALM[] = "ESP32 Gas Meter";

void releaseJsonBody(AsyncWebServerRequest* request) {
  if (!request || !request->_tempObject) return;
  std::free(request->_tempObject);
  request->_tempObject = nullptr;
}

bool bufferJsonBody(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total, size_t limit) {
  if (total == 0 || total > limit || index > total || len > total - index) {
    releaseJsonBody(request);
    if (index == 0) request->send(413, "application/json", "{\"error\":\"request body too large or chunked\"}");
    return false;
  }
  if (index == 0) {
    releaseJsonBody(request);
    request->_tempObject = std::calloc(total + 1, sizeof(uint8_t));
    if (!request->_tempObject) {
      request->send(503, "application/json", "{\"error\":\"out of memory\"}");
      return false;
    }
  }
  if (!request->_tempObject) return false;
  std::memcpy(static_cast<uint8_t*>(request->_tempObject) + index, data, len);
  return index + len == total;
}

bool parseBufferedJson(AsyncWebServerRequest* request, size_t total, JsonDocument& document) {
  if (!request->_tempObject) return false;
  const DeserializationError error = deserializeJson(document, static_cast<const char*>(request->_tempObject), total);
  releaseJsonBody(request);
  if (error) {
    request->send(400, "application/json", "{\"error\":\"invalid JSON\"}");
    return false;
  }
  return true;
}

void addUsage(JsonObject target, const UsageSnapshot& usage) {
  target["raw_volume_m3"] = usage.rawVolumeM3;
  target["volume_m3"] = usage.correctedVolumeM3;
  target["energy_kwh"] = usage.energyKwh;
  target["flow_m3h"] = usage.flowM3h;
  target["day_m3"] = usage.dayM3;
  target["month_m3"] = usage.monthM3;
  target["year_m3"] = usage.yearM3;
  target["day_energy_kwh"] = usage.dayEnergyKwh;
  target["month_energy_kwh"] = usage.monthEnergyKwh;
  target["year_energy_kwh"] = usage.yearEnergyKwh;
  target["continuous_flow_minutes"] = usage.continuousFlowMinutes;
  target["continuous_flow_alert"] = usage.continuousFlowAlert;
  target["timestamp"] = static_cast<uint64_t>(usage.timestamp);
}
}

bool WebServerManager::authorize(AsyncWebServerRequest* request, bool allowSetupAp) {
  if (allowSetupAp && WiFiManager::accessPointMode()) return true;
  if (request->authenticate(Config::webUser, Config::webPassword, AUTH_REALM)) return true;
  request->requestAuthentication(AUTH_REALM, true);
  return false;
}

void WebServerManager::sendJson(AsyncWebServerRequest* request, JsonDocument& document, int status) {
  AsyncResponseStream* response = request->beginResponseStream("application/json");
  response->setCode(status);
  response->addHeader("Cache-Control", "no-store");
  response->addHeader("X-Content-Type-Options", "nosniff");
  response->addHeader("X-Frame-Options", "DENY");
  serializeJson(document, *response);
  request->send(response);
}

void WebServerManager::registerRoutes() {
  server_.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response = request->beginResponse(
        200, "text/html; charset=utf-8", reinterpret_cast<const uint8_t*>(WebUi::INDEX_HTML), sizeof(WebUi::INDEX_HTML) - 1);
    response->addHeader("Content-Security-Policy", "default-src 'self'; style-src 'self' 'unsafe-inline'; script-src 'self' 'unsafe-inline'; connect-src 'self'");
    response->addHeader("X-Frame-Options", "DENY");
    response->addHeader("Referrer-Policy", "no-referrer");
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
  });

  server_.on("/manifest.json", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "application/manifest+json", R"({"name":"ESP32 Gaszähler","short_name":"Gaszähler","start_url":"/","display":"standalone","background_color":"#090b0e","theme_color":"#111418"})");
  });
  server_.on("/sw.js", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "application/javascript", "const C='gas-v311-ui';self.addEventListener('install',e=>e.waitUntil(caches.open(C).then(c=>c.add('/'))));self.addEventListener('activate',e=>e.waitUntil(caches.keys().then(k=>Promise.all(k.filter(x=>x!==C).map(x=>caches.delete(x))))));self.addEventListener('fetch',e=>e.respondWith(fetch(e.request).catch(()=>caches.match(e.request))));");
  });

  server_.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* request) {
    JsonDocument doc;
    doc["firmware"] = FIRMWARE_VERSION;
    doc["hostname"] = Config::hostname;
    doc["device_id"] = Config::deviceId;
    doc["ip"] = WiFiManager::ipAddress();
    doc["ap_mode"] = WiFiManager::accessPointMode();
    doc["safe_mode"] = safeMode_;
    doc["time_synchronized"] = TimeManager::synchronized();
    doc["last_time_sync"] = static_cast<uint64_t>(TimeManager::lastSyncEpoch());
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["mqtt_connected"] = MQTTHandler::connected();
    doc["uptime_seconds"] = millis() / 1000UL;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["min_free_heap"] = ESP.getMinFreeHeap();
    doc["reset_reason"] = static_cast<int>(esp_reset_reason());
    doc["boot_failures"] = BootGuard::failureCount();
    doc["boot_stable"] = BootGuard::stable();
    doc["ota_pending_validation"] = BootGuard::pendingOtaValidation();
    addUsage(doc["usage"].to<JsonObject>(), UsageTracker::snapshot());
    const MBusStats& stats = MBusReader::stats();
    JsonObject mbus = doc["mbus"].to<JsonObject>();
    mbus["healthy"] = MBusReader::healthy();
    mbus["polls"] = stats.polls;
    mbus["successful"] = stats.successful;
    mbus["success_rate"] = stats.polls ? stats.successful * 100.0f / stats.polls : 0.0f;
    mbus["timeouts"] = stats.timeouts;
    mbus["parse_errors"] = stats.parseErrors;
    mbus["checksum_errors"] = stats.checksumErrors;
    mbus["average_response_ms"] = stats.averageResponseMs;
    mbus["last_error"] = MBusProtocol::errorToString(stats.lastError);
    mbus["last_hex"] = stats.lastHexDump;
    sendJson(request, doc);
  });

  server_.on("/api/config", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!authorize(request)) return;
    JsonDocument doc;
    Config::toJson(doc.to<JsonObject>(), false);
    sendJson(request, doc);
  });

  server_.on("/api/config", HTTP_POST,
    [](AsyncWebServerRequest*) {}, nullptr,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      if (!authorize(request)) return;
      if (!bufferJsonBody(request, data, len, index, total, Constants::MAX_JSON_BODY)) return;
      JsonDocument input;
      if (!parseBufferedJson(request, total, input)) return;
      String error;
      if (!Config::importJson(input.as<JsonVariantConst>(), error)) {
        JsonDocument response;
        response["error"] = error.isEmpty() ? "save failed" : error;
        sendJson(request, response, 400);
        return;
      }
      BootGuard::markPlannedRestart("configuration_change");
      JsonDocument response;
      response["status"] = "ok";
      response["restart"] = true;
      sendJson(request, response);
      restartAt_ = millis() + 1500;
    }
  );

  server_.on("/api/config/export", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!authorize(request)) return;
    const bool includeSecrets = request->hasParam("secrets") && request->getParam("secrets")->value() == "EXPORT";
    JsonDocument doc;
    Config::toJson(doc.to<JsonObject>(), includeSecrets);
    AsyncResponseStream* response = request->beginResponseStream("application/json");
    response->addHeader("Content-Disposition", "attachment; filename=gasmeter-config.json");
    response->addHeader("Cache-Control", "no-store");
    serializeJsonPretty(doc, *response);
    request->send(response);
  });

  server_.on("/api/usage.csv", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!authorize(request)) return;
    AsyncResponseStream* response = request->beginResponseStream("text/csv; charset=utf-8");
    response->addHeader("Content-Disposition", "attachment; filename=gasmeter-daily-history.csv");
    response->addHeader("Cache-Control", "no-store");
    response->print("date,volume_m3,energy_kwh\n");
    size_t count = 0;
    const DailyUsageRecord* records = UsageTracker::dailyHistory(count);
    for (size_t i = 0; i < count; ++i) {
      response->printf("%s,%.3f,%.3f\n", records[i].date, records[i].volumeM3, records[i].energyKwh);
    }
    request->send(response);
  });

  server_.on("/api/mbus/poll", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (!authorize(request)) return;
    JsonDocument doc;
    const bool triggered = MBusReader::trigger();
    doc["triggered"] = triggered;
    sendJson(request, doc, triggered ? 200 : 409);
  });

  server_.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!authorize(request)) return;
    JsonDocument doc;
    JsonArray logs = doc["logs"].to<JsonArray>();
    for (const LogEntry& entry : Logger::entries()) {
      JsonObject item = logs.add<JsonObject>();
      item["timestamp_ms"] = entry.timestampMs;
      item["message"] = entry.message;
    }
    sendJson(request, doc);
  });

  server_.on("/metrics", HTTP_GET, [](AsyncWebServerRequest* request) {
    const UsageSnapshot& usage = UsageTracker::snapshot();
    const MBusStats& stats = MBusReader::stats();
    String output;
    output.reserve(2048);
    output += "# TYPE gasmeter_volume_m3 counter\n";
    output += "gasmeter_volume_m3 " + String(usage.correctedVolumeM3, 3) + "\n";
    output += "# TYPE gasmeter_energy_kwh counter\n";
    output += "gasmeter_energy_kwh " + String(usage.energyKwh, 3) + "\n";
    output += "# TYPE gasmeter_flow_m3h gauge\n";
    output += "gasmeter_flow_m3h " + String(usage.flowM3h, 4) + "\n";
    output += "gasmeter_day_volume_m3 " + String(usage.dayM3, 3) + "\n";
    output += "gasmeter_month_volume_m3 " + String(usage.monthM3, 3) + "\n";
    output += "gasmeter_year_volume_m3 " + String(usage.yearM3, 3) + "\n";
    output += "gasmeter_day_energy_kwh " + String(usage.dayEnergyKwh, 3) + "\n";
    output += "gasmeter_month_energy_kwh " + String(usage.monthEnergyKwh, 3) + "\n";
    output += "gasmeter_year_energy_kwh " + String(usage.yearEnergyKwh, 3) + "\n";
    output += "gasmeter_continuous_flow_alert " + String(usage.continuousFlowAlert ? 1 : 0) + "\n";
    output += "gasmeter_time_synchronized " + String(TimeManager::synchronized() ? 1 : 0) + "\n";
    output += "gasmeter_mbus_healthy " + String(MBusReader::healthy() ? 1 : 0) + "\n";
    output += "gasmeter_mbus_polls_total " + String(stats.polls) + "\n";
    output += "gasmeter_mbus_timeouts_total " + String(stats.timeouts) + "\n";
    output += "gasmeter_mbus_parse_errors_total " + String(stats.parseErrors) + "\n";
    output += "gasmeter_mqtt_reconnects_total " + String(MQTTHandler::reconnectCount()) + "\n";
    output += "gasmeter_mqtt_publish_errors_total " + String(MQTTHandler::publishErrors()) + "\n";
    output += "gasmeter_free_heap_bytes " + String(ESP.getFreeHeap()) + "\n";
    request->send(200, "text/plain; version=0.0.4", output);
  });

  server_.on("/api/factory-reset", HTTP_POST,
    [](AsyncWebServerRequest*) {}, nullptr,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      if (!authorize(request, false)) return;
      if (!bufferJsonBody(request, data, len, index, total, 256)) return;
      JsonDocument input;
      if (!parseBufferedJson(request, total, input)) return;
      if (strcmp(input["confirm"] | "", "RESET") != 0) {
        JsonDocument response;
        response["error"] = "confirmation required";
        sendJson(request, response, 400);
        return;
      }
      Config::factoryReset();
      BootGuard::markPlannedRestart("factory_reset");
      JsonDocument response;
      response["status"] = "resetting";
      sendJson(request, response);
      restartAt_ = millis() + 1000;
    }
  );

  server_.on("/api/restart", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (!authorize(request, false)) return;
    BootGuard::markPlannedRestart("web_restart");
    request->send(202, "application/json", "{\"status\":\"restarting\"}");
    restartAt_ = millis() + 1000;
  });

  server_.onNotFound([](AsyncWebServerRequest* request) {
    if (WiFiManager::accessPointMode()) request->redirect("/");
    else request->send(404, "application/json", "{\"error\":\"not found\"}");
  });
}

void WebServerManager::begin(bool safeMode) {
  safeMode_ = safeMode;
  registerRoutes();
  server_.begin();
  Logger::info("Web server started at " + WiFiManager::ipAddress());
}

void WebServerManager::loop() {
  if (restartAt_ && static_cast<int32_t>(millis() - restartAt_) >= 0) {
    restartAt_ = 0;
    ESP.restart();
  }
}
