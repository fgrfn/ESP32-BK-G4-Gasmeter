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
#include "WiFiManager.h"
#include "constants.h"

AsyncWebServer WebServerManager::server_(80);
bool WebServerManager::safeMode_ = false;
uint32_t WebServerManager::restartAt_ = 0;

namespace {
constexpr char AUTH_REALM[] = "ESP32 Gas Meter";

const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="de"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#111827"><link rel="manifest" href="/manifest.json">
<title>ESP32 Gaszähler</title><style>
:root{color-scheme:dark;font-family:system-ui,sans-serif}body{margin:0;background:#0b1020;color:#e5e7eb}.wrap{max-width:1100px;margin:auto;padding:20px}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(210px,1fr));gap:12px}.card{background:#151c30;border:1px solid #26304b;border-radius:14px;padding:16px;margin-bottom:14px}.value{font-size:1.55rem;font-weight:700}.muted{color:#9ca3af}.row{display:grid;grid-template-columns:1fr 1fr;gap:10px}.row3{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}button,input,select,textarea{box-sizing:border-box;width:100%;padding:10px;margin:5px 0;border-radius:8px;border:1px solid #374151;background:#0f172a;color:#e5e7eb}button{background:#2563eb;border:0;font-weight:700;cursor:pointer;min-height:44px}.secondary{background:#374151}.danger{background:#b91c1c}pre{white-space:pre-wrap;overflow-wrap:anywhere;max-height:360px;overflow:auto;background:#0f172a;padding:12px;border-radius:8px}.ok{color:#34d399}.bad{color:#fb7185}.warning{color:#fbbf24}h1,h2{margin-top:0}details{margin-bottom:14px}summary{cursor:pointer;font-weight:700;padding:10px 0}@media(max-width:700px){.row,.row3{grid-template-columns:1fr}}
</style></head><body><main class="wrap">
<h1>ESP32 BK-G4 Gaszähler</h1><p id="mode" class="muted">Lade Status…</p>
<section class="grid"><div class="card"><div class="muted">Zählerstand</div><div id="volume" class="value">–</div></div><div class="card"><div class="muted">Energie gesamt</div><div id="energy" class="value">–</div></div><div class="card"><div class="muted">Durchfluss</div><div id="flow" class="value">–</div></div><div class="card"><div class="muted">M-Bus</div><div id="mbus" class="value">–</div></div></section>
<h2>Zeiträume</h2><section class="grid"><div class="card">Heute<br><span id="day" class="value">–</span><br><span id="dayEnergy" class="muted"></span></div><div class="card">Monat<br><span id="month" class="value">–</span><br><span id="monthEnergy" class="muted"></span></div><div class="card">Jahr<br><span id="year" class="value">–</span><br><span id="yearEnergy" class="muted"></span></div><div class="card">Dauerfluss<br><span id="flowAlert" class="value">–</span><br><span id="flowMinutes" class="muted"></span></div></section>
<details open><summary>Konfiguration</summary><section class="card"><form id="config">
<h3>WLAN und Netzwerk</h3><div class="row"><input id="ssid" placeholder="WLAN SSID"><input id="wifiPassword" type="password" placeholder="WLAN-Passwort (leer = unverändert)"></div><div class="row"><input id="hostname" placeholder="Hostname"><input id="timezone" placeholder="POSIX-Zeitzone"></div><label><input id="staticEnabled" type="checkbox" style="width:auto"> Statische IPv4-Konfiguration</label><div class="row3"><input id="staticIp" placeholder="IP-Adresse"><input id="gateway" placeholder="Gateway"><input id="subnet" placeholder="Subnetzmaske"></div><input id="dns" placeholder="DNS-Server">
<h3>MQTT</h3><div class="row"><input id="mqttHost" placeholder="MQTT Host"><input id="mqttPort" type="number" placeholder="MQTT Port"></div><div class="row"><input id="mqttUser" placeholder="MQTT Benutzer"><input id="mqttPassword" type="password" placeholder="MQTT-Passwort (leer = unverändert)"></div><input id="mqttTopic" placeholder="MQTT Basistopic"><div class="row"><label><input id="mqttTls" type="checkbox" style="width:auto"> MQTT TLS</label><label><input id="mqttInsecure" type="checkbox" style="width:auto"> Zertifikatsprüfung deaktivieren</label></div><label><input id="mqttCommands" type="checkbox" style="width:auto"> Home-Assistant-Kommandos aktivieren</label><textarea id="mqttCa" rows="4" placeholder="CA-Zertifikat (PEM; leer = unverändert)"></textarea>
<h3>Messung</h3><div class="row3"><input id="poll" type="number" min="10" max="3600" placeholder="Poll-Intervall Sekunden"><input id="offset" type="number" step="0.001" placeholder="Zähler-Offset m³"><input id="maxFlow" type="number" step="0.1" placeholder="Max. Durchfluss m³/h"></div><div class="row"><input id="calorific" type="number" step="0.001" placeholder="Brennwert kWh/m³"><input id="correction" type="number" step="0.0001" placeholder="Zustandszahl"></div><div class="row"><input id="flowThreshold" type="number" step="0.001" min="0" placeholder="Dauerfluss-Schwelle m³/h"><input id="flowDelay" type="number" min="0" max="10080" placeholder="Dauerfluss-Warnung nach Minuten"></div>
<h3>Sicherheit</h3><div class="row3"><input id="webUser" placeholder="Web-Benutzer"><input id="webPassword" type="password" placeholder="Web-Passwort (leer = unverändert)"><input id="otaPassword" type="password" placeholder="ArduinoOTA-Passwort (leer = unverändert)"></div><button type="submit">Speichern und neu starten</button></form></section></details>
<details><summary>Import und Export</summary><section class="card"><div class="row3"><button id="export">Konfiguration exportieren</button><button id="history">Tageshistorie als CSV</button><input id="importFile" type="file" accept="application/json,.json"></div><button id="import" class="secondary">Ausgewählte Konfiguration importieren</button></section></details>
<details open><summary>Diagnose</summary><section class="card"><div class="row3"><button id="pollNow">M-Bus jetzt abfragen</button><button id="reloadLogs" class="secondary">Logs aktualisieren</button><button id="restart" class="secondary">Neu starten</button></div><h3>Status</h3><pre id="diagnostics"></pre><h3>Letztes M-Bus-Telegramm</h3><pre id="hex">–</pre><h3>Logs</h3><pre id="logs">–</pre><button id="reset" class="danger">Factory Reset</button></section></details>
<script>
const $=id=>document.getElementById(id);const fmt=(v,d=3)=>Number.isFinite(v)?v.toFixed(d):'–';
async function json(url,opt){const r=await fetch(url,{credentials:'same-origin',...opt});if(!r.ok)throw new Error(await r.text());return r.json()}
async function status(){try{const s=await json('/api/status');const health=s.safe_mode?'SAFE MODE':(s.ap_mode?'SETUP AP':(s.mbus.healthy?'ONLINE':'M-BUS PRÜFEN'));$('mode').textContent=`${s.hostname} · ${s.ip} · ${health} · Firmware ${s.firmware} · Zeit ${s.time_synchronized?'OK':'nicht synchronisiert'}`;$('mode').className=s.safe_mode||!s.time_synchronized?'warning':'muted';$('volume').textContent=fmt(s.usage.volume_m3)+' m³';$('energy').textContent=fmt(s.usage.energy_kwh,1)+' kWh';$('flow').textContent=fmt(s.usage.flow_m3h,4)+' m³/h';$('mbus').textContent=fmt(s.mbus.success_rate,1)+' %';$('day').textContent=fmt(s.usage.day_m3)+' m³';$('month').textContent=fmt(s.usage.month_m3)+' m³';$('year').textContent=fmt(s.usage.year_m3)+' m³';$('dayEnergy').textContent=fmt(s.usage.day_energy_kwh,1)+' kWh';$('monthEnergy').textContent=fmt(s.usage.month_energy_kwh,1)+' kWh';$('yearEnergy').textContent=fmt(s.usage.year_energy_kwh,1)+' kWh';$('flowAlert').textContent=s.usage.continuous_flow_alert?'WARNUNG':'OK';$('flowAlert').className='value '+(s.usage.continuous_flow_alert?'bad':'ok');$('flowMinutes').textContent=s.usage.continuous_flow_minutes+' Minuten';$('hex').textContent=s.mbus.last_hex||'–';$('diagnostics').textContent=JSON.stringify(s,null,2)}catch(e){$('mode').textContent=e.message;$('mode').className='bad'}}setInterval(status,5000);status();
async function loadConfig(){try{const c=await json('/api/config');$('ssid').value=c.wifi.ssid||'';$('hostname').value=c.wifi.hostname||'';$('timezone').value=c.timezone||'';$('staticEnabled').checked=!!c.wifi.static_ip_enabled;$('staticIp').value=c.wifi.static_ip||'';$('gateway').value=c.wifi.gateway||'';$('subnet').value=c.wifi.subnet||'';$('dns').value=c.wifi.dns||'';$('mqttHost').value=c.mqtt.host||'';$('mqttPort').value=c.mqtt.port||1883;$('mqttUser').value=c.mqtt.user||'';$('mqttTopic').value=c.mqtt.base_topic||'';$('mqttTls').checked=!!c.mqtt.tls;$('mqttInsecure').checked=!!c.mqtt.tls_insecure;$('mqttCommands').checked=!!c.mqtt.commands_enabled;$('poll').value=c.gas.poll_interval_seconds;$('offset').value=c.gas.meter_offset_m3;$('maxFlow').value=c.gas.max_flow_m3h;$('calorific').value=c.gas.calorific_value;$('correction').value=c.gas.correction_factor;$('flowThreshold').value=c.gas.continuous_flow_threshold_m3h;$('flowDelay').value=c.gas.continuous_flow_alert_minutes;$('webUser').value=c.security.web_user||''}catch(e){console.warn(e)}}loadConfig();
function configPayload(){const c={timezone:$('timezone').value,wifi:{ssid:$('ssid').value,hostname:$('hostname').value,static_ip_enabled:$('staticEnabled').checked,static_ip:$('staticIp').value,gateway:$('gateway').value,subnet:$('subnet').value,dns:$('dns').value},mqtt:{host:$('mqttHost').value,port:+$('mqttPort').value,user:$('mqttUser').value,base_topic:$('mqttTopic').value,tls:$('mqttTls').checked,tls_insecure:$('mqttInsecure').checked,commands_enabled:$('mqttCommands').checked},gas:{poll_interval_seconds:+$('poll').value,meter_offset_m3:+$('offset').value,max_flow_m3h:+$('maxFlow').value,calorific_value:+$('calorific').value,correction_factor:+$('correction').value,continuous_flow_threshold_m3h:+$('flowThreshold').value,continuous_flow_alert_minutes:+$('flowDelay').value},security:{web_user:$('webUser').value}};if($('wifiPassword').value)c.wifi.password=$('wifiPassword').value;if($('mqttPassword').value)c.mqtt.password=$('mqttPassword').value;if($('mqttCa').value)c.mqtt.ca_cert=$('mqttCa').value;if($('webPassword').value)c.security.web_password=$('webPassword').value;if($('otaPassword').value)c.security.ota_password=$('otaPassword').value;return c}
$('config').onsubmit=async e=>{e.preventDefault();await json('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(configPayload())});alert('Gespeichert. Gerät startet neu.')};
$('pollNow').onclick=()=>json('/api/mbus/poll',{method:'POST'}).then(status).catch(e=>alert(e.message));$('export').onclick=()=>location.href='/api/config/export';$('history').onclick=()=>location.href='/api/usage.csv';
$('import').onclick=async()=>{const f=$('importFile').files[0];if(!f)return alert('Bitte JSON-Datei auswählen.');const text=await f.text();JSON.parse(text);await json('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:text});alert('Importiert. Gerät startet neu.')};
async function logs(){try{const l=await json('/api/logs');$('logs').textContent=l.logs.map(x=>`${x.timestamp_ms} ms  ${x.message}`).join('\n')||'Keine Einträge'}catch(e){$('logs').textContent=e.message}}$('reloadLogs').onclick=logs;logs();
$('restart').onclick=async()=>{if(!confirm('Gerät neu starten?'))return;await json('/api/restart',{method:'POST'});alert('Neustart gestartet')};$('reset').onclick=async()=>{if(prompt('Zum Löschen RESET eingeben')!=='RESET')return;await json('/api/factory-reset',{method:'POST',headers:{'Content-Type':'application/json'},body:'{"confirm":"RESET"}'});alert('Factory Reset gestartet')};
if('serviceWorker'in navigator)navigator.serviceWorker.register('/sw.js').catch(()=>{});
</script></main></body></html>)HTML";

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
        200, "text/html; charset=utf-8", reinterpret_cast<const uint8_t*>(INDEX_HTML), sizeof(INDEX_HTML) - 1);
    response->addHeader("Content-Security-Policy", "default-src 'self'; style-src 'self' 'unsafe-inline'; script-src 'self' 'unsafe-inline'; connect-src 'self'");
    response->addHeader("X-Frame-Options", "DENY");
    response->addHeader("Referrer-Policy", "no-referrer");
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
  });

  server_.on("/manifest.json", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "application/manifest+json", R"({"name":"ESP32 Gaszähler","short_name":"Gaszähler","start_url":"/","display":"standalone","background_color":"#0b1020","theme_color":"#111827"})");
  });
  server_.on("/sw.js", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "application/javascript", "const C='gas-v31';self.addEventListener('install',e=>e.waitUntil(caches.open(C).then(c=>c.add('/'))));self.addEventListener('activate',e=>e.waitUntil(caches.keys().then(k=>Promise.all(k.filter(x=>x!==C).map(x=>caches.delete(x))))));self.addEventListener('fetch',e=>e.respondWith(fetch(e.request).catch(()=>caches.match(e.request))));");
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
