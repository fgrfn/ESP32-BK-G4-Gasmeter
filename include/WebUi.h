#pragma once
#include <Arduino.h>

namespace WebUi {
static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#111418">
<link rel="manifest" href="/manifest.json">
<title data-i18n>ESP32 Gas Meter</title>
<style>
:root{color-scheme:dark;--bg:#090b0e;--s:#13181e;--s2:#0d1115;--line:#2b333d;--txt:#f2f4f7;--mut:#96a0ab;--acc:#efb13d;--ok:#36c98f;--warn:#f2b94b;--bad:#ef6666;font-family:system-ui,-apple-system,"Segoe UI",sans-serif}
*{box-sizing:border-box}body{margin:0;background:radial-gradient(circle at 15% -10%,#293540,transparent 36%),linear-gradient(145deg,#080a0d,#0e1318);color:var(--txt);min-height:100vh}.wrap{max-width:1160px;margin:auto;padding:18px}.top{display:flex;align-items:center;justify-content:space-between;gap:14px;margin-bottom:14px}.brand{display:flex;align-items:center;gap:12px}.logo{width:44px;height:44px;border-radius:13px;display:grid;place-items:center;background:linear-gradient(145deg,#ffd471,#cc7b08);color:#1b1308;font-size:21px;font-weight:900;box-shadow:0 10px 30px #0008}.brand h1{margin:0;font-size:clamp(1.25rem,4vw,1.75rem)}.brand p{margin:3px 0 0;color:var(--mut);font-size:.82rem}.chips{display:flex;gap:7px;flex-wrap:wrap;justify-content:flex-end}.chip,.status-pill{padding:7px 10px;border:1px solid var(--line);border-radius:99px;background:#171d23;font-size:.75rem;font-weight:750}.chip:before{content:"";display:inline-block;width:8px;height:8px;border-radius:50%;background:var(--mut);margin-right:7px}.chip.ok:before{background:var(--ok);box-shadow:0 0 10px var(--ok)}.chip.warn:before{background:var(--warn)}.chip.bad:before{background:var(--bad)}.status-pill.ok{color:var(--ok);border-color:#245d49}.status-pill.warn{color:var(--warn);border-color:#735d2c}.tabs-wrap{position:sticky;top:0;z-index:20;padding:5px 0 12px;background:linear-gradient(#0b0f13 70%,transparent)}.tabs{display:flex;gap:7px;padding:6px;border:1px solid var(--line);border-radius:15px;background:#10151ae8;backdrop-filter:blur(12px);overflow-x:auto;scrollbar-width:none}.tabs::-webkit-scrollbar{display:none}.tab,.repo-link{width:auto;min-width:max-content;min-height:39px;padding:8px 14px;border:1px solid transparent;border-radius:10px;background:transparent;color:var(--mut);font:750 .82rem system-ui;cursor:pointer}.repo-link{display:flex;align-items:center;text-decoration:none;margin-left:auto;border-color:#303a44}.tab:hover,.repo-link:hover{color:var(--txt);background:#1a2128}.tab.active{color:#181109;background:linear-gradient(#f7ca68,#d48910);box-shadow:0 6px 18px #0006}.tab-panel{display:none;animation:fade .16s ease}.tab-panel.active{display:block}@keyframes fade{from{opacity:.55;transform:translateY(3px)}to{opacity:1;transform:none}}.hero{display:grid;grid-template-columns:1.6fr .8fr;gap:14px}.meter,.card,.page{background:linear-gradient(145deg,#171d23,#0e1216);border:1px solid var(--line);border-radius:19px;box-shadow:0 18px 55px #0005}.meter{padding:20px}.meter-head,.meter-meta{display:flex;justify-content:space-between;gap:10px;color:var(--mut);font-size:.77rem}.meter-head{margin-bottom:15px}.meter-head strong{color:#dfe4e9;letter-spacing:.12em;text-transform:uppercase}.shell{padding:22px 18px 16px;border-radius:16px;background:linear-gradient(145deg,#b9b3a6,#5e5a53 47%,#d1cabd);box-shadow:inset 0 2px 1px #fff9,inset 0 -4px 7px #0008,0 15px 30px #0008}.counter{display:flex;justify-content:center;align-items:center;gap:3px;padding:10px 8px;background:#060606;border:6px solid #292722;border-radius:9px;overflow:hidden}.digit{min-width:clamp(28px,6vw,52px);height:clamp(54px,10vw,80px);display:grid;place-items:center;border-radius:3px;background:linear-gradient(#f5f0df 0 47%,#cec8b6 49% 51%,#faf6e8 53%);color:#111;font:800 clamp(2rem,7vw,4rem)/1 "Courier New",monospace;box-shadow:inset 0 0 0 1px #0008,inset 0 6px 8px #fff8,inset 0 -6px 8px #0003}.digit.red{background:linear-gradient(#cf453b 0 47%,#8c231d 49% 51%,#dc5045 53%);color:#fff;text-shadow:0 1px 2px #000}.dot{align-self:end;padding-bottom:8px;font:900 2rem monospace}.plate{display:flex;justify-content:space-between;margin-top:10px;color:#26221d;font-weight:850}.plate small{letter-spacing:.11em}.meter-meta{margin-top:13px}.stack{display:grid;gap:10px}.card{padding:16px}.eyebrow{font-size:.68rem;text-transform:uppercase;letter-spacing:.1em;color:var(--mut);font-weight:800}.big{font-size:clamp(1.45rem,4vw,2rem);font-weight:850;margin-top:5px}.sub{font-size:.76rem;color:var(--mut);margin-top:4px}.stats{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;margin:14px 0}.value{font-size:1.45rem;font-weight:820;margin-top:5px}.ok-text{color:var(--ok)}.bad-text{color:var(--bad)}.page{padding:22px}.page-head{display:flex;align-items:flex-start;justify-content:space-between;gap:15px;padding-bottom:16px;margin-bottom:18px;border-bottom:1px solid var(--line)}.page-head h2{margin:0 0 5px;font-size:1.25rem}.page-head p{margin:0;color:var(--mut);font-size:.78rem;line-height:1.45}.section{padding:16px;border:1px solid #252d35;border-radius:14px;background:#0d1217;margin-bottom:14px}.section h3{margin:0 0 12px;color:var(--acc);font-size:.8rem;text-transform:uppercase;letter-spacing:.09em}.row{display:grid;grid-template-columns:repeat(2,1fr);gap:11px}.row3{display:grid;grid-template-columns:repeat(3,1fr);gap:11px}.field{display:flex;flex-direction:column;gap:5px;margin-bottom:9px}.field>label{font-size:.78rem;font-weight:750;color:#d6dce2}.field small{color:var(--mut);font-size:.68rem;line-height:1.35;min-height:1.7em}.field.full{grid-column:1/-1}input,textarea,select,button{width:100%;min-height:43px;border-radius:10px;font:inherit}input,textarea,select{padding:10px 11px;border:1px solid #343d47;background:#10151a;color:var(--txt);outline:none}input:focus,textarea:focus,select:focus{border-color:var(--acc);box-shadow:0 0 0 3px #efb13d1c}textarea{min-height:110px;resize:vertical}.check{display:flex;align-items:flex-start;gap:9px;padding:9px 0;color:#cbd2d9;font-size:.8rem}.check input{width:17px;min-height:17px;accent-color:var(--acc);margin-top:1px}.check-text{display:flex;flex-direction:column;gap:3px}.check-text small{color:var(--mut);font-size:.68rem;line-height:1.35}button{border:0;padding:10px 13px;background:linear-gradient(#f7ca68,#d48910);color:#181109;font-weight:850;cursor:pointer}button.secondary{background:#252d35;color:#e8ebef;border:1px solid #3a444f}button.danger{background:#96343a;color:white}.actions{display:flex;gap:9px;flex-wrap:wrap;margin-top:12px}.actions>*{flex:1 1 170px}.notice{padding:11px 12px;border-radius:10px;border:1px solid #795b23;background:#292112;color:#f0d18d;font-size:.78rem;line-height:1.45}.notice.info{border-color:#36566d;background:#13232e;color:#bcd9eb}.unit{color:var(--acc);font-weight:800}pre{white-space:pre-wrap;overflow-wrap:anywhere;max-height:340px;overflow:auto;padding:12px;border-radius:10px;background:#070a0c;border:1px solid #272f37;color:#b8c2cc;font:12px/1.5 monospace}#toast{position:fixed;right:16px;bottom:16px;padding:12px 14px;background:#1b2229;border:1px solid #3d4854;border-radius:11px;box-shadow:0 15px 45px #0009;opacity:0;transform:translateY(8px);transition:.2s;pointer-events:none;z-index:50}#toast.show{opacity:1;transform:none}@media(max-width:800px){.hero{grid-template-columns:1fr}.stack{grid-template-columns:repeat(3,1fr)}.stats{grid-template-columns:repeat(2,1fr)}.chips{display:none}.page-head{flex-direction:column}}@media(max-width:600px){.wrap{padding:12px}.stack{grid-template-columns:1fr}.row,.row3{grid-template-columns:1fr}.field.full{grid-column:auto}.meter{padding:13px}.shell{padding:18px 8px 13px}.counter{gap:2px;padding:7px 4px;border-width:5px}.digit{min-width:clamp(24px,9.2vw,40px);height:56px;font-size:clamp(1.7rem,10vw,2.8rem)}.dot{font-size:1.3rem}.stats{gap:7px}.card,.page{padding:13px}.meter-meta{flex-direction:column}.tab,.repo-link{padding:8px 11px}}
</style>
<style>
.notice.ok{border-color:#245d49;background:#10261e;color:#9de2c5}.version-grid{display:grid;grid-template-columns:repeat(2,1fr);gap:11px;margin-bottom:12px}.version-box{padding:13px;border:1px solid #303943;border-radius:11px;background:#10161b}.version-box strong{display:block;margin-top:5px;font-size:1.15rem}.release-link{display:inline-block;margin-top:10px;color:var(--acc);font-weight:750;text-decoration:none}.release-link:hover{text-decoration:underline}@media(max-width:600px){.version-grid{grid-template-columns:1fr}}
.header-tools{display:flex;align-items:center;gap:7px}
#language{width:auto;min-width:0}
.warn-text{color:var(--acc)}
</style>
</head>
<body>
<main class="wrap">
<header class="top"><div class="brand"><div class="logo">G</div><div><h1 data-i18n>BK-G4 Gas Meter</h1><p id="identity" data-i18n>ESP32 meter gateway</p></div></div><div class="header-tools"><select id="language" aria-label="Language" data-i18n-aria-label><option value="en" lang="en">English</option><option value="de" lang="de">Deutsch</option></select><div class="chips"><span id="healthChip" class="chip" data-i18n>Connecting…</span><span id="timeChip" class="chip" data-i18n>Time</span><span id="mqttChip" class="chip">MQTT</span></div></div></header>
<div class="tabs-wrap"><nav class="tabs" aria-label="Main navigation" data-i18n-aria-label><button class="tab active" type="button" data-tab="overview" data-i18n>Overview</button><button class="tab" type="button" data-tab="wifi" data-i18n>Wi-Fi</button><button class="tab" type="button" data-tab="mqtt">MQTT</button><button class="tab" type="button" data-tab="measurement" data-i18n>Measurement</button><button class="tab" type="button" data-tab="security" data-i18n>Security</button><button class="tab" type="button" data-tab="system">System</button><a class="repo-link" href="https://github.com/fgrfn/ESP32-BK-G4-Gasmeter" target="_blank" rel="noopener noreferrer" aria-label="Open project on GitHub" data-i18n-aria-label>GitHub ↗</a></nav></div>

<section id="tab-overview" class="tab-panel active">
<section class="hero"><article class="meter"><div class="meter-head"><strong data-i18n>Meter reading</strong><span id="lastUpdated" data-i18n>Waiting for a reading</span></div><div class="shell"><div id="meterDigits" class="counter"><span class="digit">–</span></div><div class="plate"><small>HONEYWELL / ELSTER · BK-G4</small><span>m³</span></div></div><div class="meter-meta"><span id="mode" data-i18n>Loading status…</span><span id="firmware">Firmware –</span></div></article><aside class="stack"><div class="card"><div class="eyebrow" data-i18n>Total energy</div><div id="energy" class="big">–</div><div class="sub">kWh</div></div><div class="card"><div class="eyebrow" data-i18n>Flow rate</div><div id="flow" class="big">–</div><div class="sub">m³/h</div></div><div class="card"><div class="eyebrow" data-i18n>M-Bus success rate</div><div id="mbus" class="big">–</div><div id="mbusDetail" class="sub" data-i18n>Waiting for a telegram</div></div></aside></section>
<section class="stats"><div class="card"><div class="eyebrow" data-i18n>Today</div><div id="day" class="value">–</div><div id="dayEnergy" class="sub">–</div></div><div class="card"><div class="eyebrow" data-i18n>This month</div><div id="month" class="value">–</div><div id="monthEnergy" class="sub">–</div></div><div class="card"><div class="eyebrow" data-i18n>This year</div><div id="year" class="value">–</div><div id="yearEnergy" class="sub">–</div></div><div class="card"><div class="eyebrow" data-i18n>Prolonged gas flow</div><div id="flowAlert" class="value">–</div><div id="flowMinutes" class="sub">–</div></div></section>
</section>

<form id="config">
<section id="tab-wifi" class="tab-panel page"><div class="page-head"><div><h2 data-i18n>Wi-Fi and network</h2><p data-i18n>Network access, hostname and optional static IPv4 settings.</p></div><button type="submit" data-i18n>Save</button></div>
<div class="section"><h3 data-i18n>Wi-Fi</h3><div class="row"><div class="field"><label for="ssid" data-i18n>Wi-Fi name (SSID)</label><input id="ssid" autocomplete="off" placeholder="e.g. IoT Wi-Fi" data-i18n-placeholder><small data-i18n>Name of the 2.4 GHz Wi-Fi network the ESP32 connects to.</small></div><div class="field"><label for="wifiPassword" data-i18n>Wi-Fi password</label><input id="wifiPassword" type="password" autocomplete="new-password" placeholder="Leave blank to keep current value" data-i18n-placeholder><small data-i18n>Enter a password to replace the saved Wi-Fi password.</small></div></div><div class="row"><div class="field"><label for="hostname">Hostname</label><input id="hostname" placeholder="esp32-gasmeter"><small><span data-i18n>Device name on the network and for mDNS, e.g.</span> <span class="unit">esp32-gasmeter.local</span>.</small></div>
<div class="field">
  <label for="timezone" data-i18n>Time zone</label>
  <select id="timezone" aria-describedby="timezoneHint">
    <option value="CET-1CEST,M3.5.0,M10.5.0/3" data-i18n>Berlin (Central Europe)</option>
    <option value="GMT0BST,M3.5.0/1,M10.5.0/2" data-i18n>London (United Kingdom)</option>
    <option value="EET-2EEST,M3.5.0/3,M10.5.0/4" data-i18n>Helsinki (Finland)</option>
    <option value="UTC0" data-i18n>UTC (Coordinated Universal Time)</option>
    <option value="" data-i18n>Custom</option>
  </select>
  <small id="timezoneHint" data-i18n>Sets the day, month and year boundaries. Changes take effect after a restart.</small>
  <div id="timezoneCustomField" hidden>
    <div class="field">
      <label for="timezoneCustom" data-i18n>Custom time zone rule (POSIX)</label>
      <input id="timezoneCustom" maxlength="64" required disabled>
    </div>
  </div>
</div></div></div>
<div class="section"><h3 data-i18n>Static IPv4</h3><label class="check"><input id="staticEnabled" type="checkbox"><span class="check-text"><strong data-i18n>Use static IPv4 settings</strong><small data-i18n>Enable to assign a fixed IP address to the ESP32.</small></span></label><div class="row3"><div class="field"><label for="staticIp" data-i18n>IP address</label><input id="staticIp" placeholder="10.10.40.102"><small data-i18n>Fixed IP address of the ESP32.</small></div><div class="field"><label for="gateway">Gateway</label><input id="gateway" placeholder="10.10.40.1"><small data-i18n>IP address of the router.</small></div><div class="field"><label for="subnet" data-i18n>Subnet mask</label><input id="subnet" placeholder="255.255.255.0"><small data-i18n>Usually 255.255.255.0 for a /24 network.</small></div></div><div class="field"><label for="dns">DNS-Server</label><input id="dns" placeholder="10.10.40.1"><small data-i18n>DNS resolver, usually the router, Pi-hole or AdGuard Home.</small></div></div>
</section>

<section id="tab-mqtt" class="tab-panel page"><div class="page-head"><div><h2>MQTT</h2><p data-i18n>Broker connection, Home Assistant commands and optional TLS.</p></div><button type="submit" data-i18n>Save</button></div>
<div class="section"><h3>Broker</h3><div class="row"><div class="field"><label for="mqttHost" data-i18n>MQTT broker</label><input id="mqttHost" placeholder="IP address or hostname" data-i18n-placeholder><small data-i18n>Address of Mosquitto or another MQTT broker.</small></div><div class="field"><label for="mqttPort" data-i18n>MQTT port</label><input id="mqttPort" type="number" min="1" max="65535" placeholder="1883"><small><span data-i18n>Default:</span> <span class="unit">1883</span><span data-i18n>, commonly with TLS:</span> <span class="unit">8883</span>.</small></div></div><div class="row"><div class="field"><label for="mqttUser" data-i18n>MQTT username</label><input id="mqttUser" autocomplete="off"><small data-i18n>User account on the broker.</small></div><div class="field"><label for="mqttPassword" data-i18n>MQTT password</label><input id="mqttPassword" type="password" autocomplete="new-password" placeholder="Leave blank to keep current value" data-i18n-placeholder><small data-i18n>Enter a password to replace the saved MQTT password.</small></div></div><div class="field"><label for="mqttTopic" data-i18n>MQTT base topic</label><input id="mqttTopic" placeholder="gasmeter/esp32_gas_xxxxxx"><small data-i18n>Topic prefix for state, diagnostics and availability.</small></div></div>
<div class="section"><h3 data-i18n>Options and TLS</h3><label class="check"><input id="mqttCommands" type="checkbox"><span class="check-text"><strong data-i18n>Enable Home Assistant commands</strong><small data-i18n>Allows polling, restarting and selected configuration changes over MQTT.</small></span></label><label class="check"><input id="mqttTls" type="checkbox"><span class="check-text"><strong data-i18n>Encrypt MQTT traffic with TLS</strong><small data-i18n>Requires a TLS-capable broker and usually a CA certificate.</small></span></label><label class="check"><input id="mqttInsecure" type="checkbox"><span class="check-text"><strong data-i18n>Disable certificate verification</strong><small data-i18n>Use only for troubleshooting. This reduces security.</small></span></label><div class="field"><label for="mqttCa" data-i18n>CA certificate (PEM)</label><textarea id="mqttCa" placeholder="-----BEGIN CERTIFICATE-----&#10;...&#10;-----END CERTIFICATE-----"></textarea><small data-i18n>Leave blank to keep the saved certificate.</small></div></div>
</section>

<section id="tab-measurement" class="tab-panel page"><div class="page-head"><div><h2 data-i18n>Measurement</h2><p data-i18n>M-Bus polling, meter correction, energy conversion and prolonged-flow alert.</p></div><button type="submit" data-i18n>Save</button></div>
<div class="section"><h3 data-i18n>Meter polling</h3><div class="row3"><div class="field"><label for="poll"><span data-i18n>Poll interval</span> <span class="unit" data-i18n>seconds</span></label><input id="poll" type="number" min="10" max="3600" step="1"><small data-i18n>Time between meter polls, from 10 to 3600 seconds.</small></div><div class="field"><label for="offset"><span data-i18n>Meter offset</span> <span class="unit">m³</span></label><input id="offset" type="number" step="0.001"><small data-i18n>Added to the reading to match the meter register.</small></div><div class="field"><label for="maxFlow"><span data-i18n>Maximum flow rate</span> <span class="unit">m³/h</span></label><input id="maxFlow" type="number" min="0.1" step="0.1"><small data-i18n>Upper limit for plausible flow readings.</small></div></div></div>
<div class="section"><h3 data-i18n>Energy conversion</h3><div class="row"><div class="field"><label for="calorific"><span data-i18n>Calorific value</span> <span class="unit">kWh/m³</span></label><input id="calorific" type="number" min="5" max="20" step="0.001"><small data-i18n>Use the value from your bill or network operator.</small></div><div class="field"><label for="correction" data-i18n>Correction factor</label><input id="correction" type="number" min="0.5" max="1.5" step="0.0001"><small data-i18n>Energy = volume × calorific value × correction factor.</small></div></div></div>
<div class="section"><h3 data-i18n>Prolonged-flow alert</h3><div class="row"><div class="field"><label for="flowThreshold"><span data-i18n>Flow threshold</span> <span class="unit">m³/h</span></label><input id="flowThreshold" type="number" step="0.001" min="0"><small data-i18n>Flow rate that starts the timer. Set to 0 to disable the alert.</small></div><div class="field"><label for="flowDelay"><span data-i18n>Alert after</span> <span class="unit" data-i18n>minutes</span></label><input id="flowDelay" type="number" min="0" max="10080" step="1"><small data-i18n>Delay before the alert. Long heating cycles can trigger it.</small></div></div></div>
</section>

<section id="tab-security" class="tab-panel page"><div class="page-head"><div><h2 data-i18n>Security</h2><p data-i18n>The WebUI login and optional ArduinoOTA password are managed separately.</p></div><button type="submit" data-i18n>Save</button></div>
<div class="section"><div class="page-head" style="padding:0 0 12px;margin:0 0 12px"><div><h3 style="margin:0 0 5px" data-i18n>WebUI login</h3><p data-i18n>Optional protection for settings and maintenance.</p></div><span id="webAuthStatus" class="status-pill warn" data-i18n>Loading status</span></div><div class="notice info" data-i18n>Enable the login if other devices or people can access this network.</div><label class="check"><input id="webAuthEnabled" type="checkbox"><span class="check-text"><strong data-i18n>Enable admin login</strong><small data-i18n>Protects configuration, exports, logs and maintenance with HTTP Digest.</small></span></label><div class="row"><div class="field"><label for="webUser" data-i18n>WebUI username</label><input id="webUser" autocomplete="username"><small><span data-i18n>Default on first activation:</span> <span class="unit">admin</span>.</small></div><div class="field"><label for="webPassword" data-i18n>New WebUI password</label><input id="webPassword" type="password" autocomplete="new-password" placeholder="Leave blank to keep current value" data-i18n-placeholder><small><span data-i18n>Default on first activation:</span> <span class="unit">admin</span><span data-i18n>. Change it after enabling the login.</span></small></div></div></div>
<div class="section"><div class="page-head" style="padding:0 0 12px;margin:0 0 12px"><div><h3 style="margin:0 0 5px">ArduinoOTA</h3><p data-i18n>Wireless firmware uploads on port 3232.</p></div><span id="otaStatus" class="status-pill warn" data-i18n>Loading status</span></div><div class="notice info" data-i18n>Without a password, any device that can reach ArduinoOTA can upload firmware. Use this setting only on a trusted, isolated network.</div><div class="field" style="margin-top:12px"><label for="otaPassword" data-i18n>New ArduinoOTA password</label><input id="otaPassword" type="password" autocomplete="new-password" placeholder="Leave blank to keep current value" data-i18n-placeholder><small><span data-i18n>Optional. Once set, pass the password to PlatformIO with</span> <span class="unit">--auth</span> <span data-i18n>.</span></small></div><label class="check"><input id="clearOtaPassword" type="checkbox"><span class="check-text"><strong data-i18n>Remove the saved OTA password</strong><small data-i18n>After restarting, OTA uploads can run without authentication.</small></span></label></div>
</section>
</form>

<section id="tab-system" class="tab-panel page"><div class="page-head"><div><h2 data-i18n>System and diagnostics</h2><p data-i18n>Back up settings, check M-Bus, view logs and maintain the device.</p></div></div>
<div class="section"><h3>Firmware</h3><div class="version-grid"><div class="version-box"><div class="eyebrow" data-i18n>Installed</div><strong id="installedVersion" data-i18n>Loading…</strong></div><div class="version-box"><div class="eyebrow" data-i18n>Latest release</div><strong id="latestVersion" data-i18n>Checking…</strong></div></div><div id="versionStatus" class="notice info" data-i18n>Checking for updates…</div><a id="releaseLink" class="release-link" href="https://github.com/fgrfn/ESP32-BK-G4-Gasmeter/releases" target="_blank" rel="noopener noreferrer" hidden data-i18n>Open GitHub release ↗</a></div>
<div class="section"><h3 data-i18n>Import and export</h3><div class="field"><label for="importFile" data-i18n>Choose a configuration file</label><input id="importFile" type="file" accept="application/json,.json"><small data-i18n>Import an exported JSON configuration, then restart to apply all settings.</small></div><div class="actions"><button id="export" type="button" data-i18n>Export configuration</button><button id="history" type="button" class="secondary" data-i18n>Daily history CSV</button><button id="import" type="button" class="secondary" data-i18n>Import configuration</button></div></div>
<div class="section"><h3 data-i18n>Maintenance</h3><div class="actions"><button id="pollNow" type="button" data-i18n>Poll M-Bus now</button><button id="reloadLogs" type="button" class="secondary" data-i18n>Refresh logs</button><button id="restart" type="button" class="secondary" data-i18n>Restart</button><button id="reset" type="button" class="danger">Factory Reset</button></div></div>
<div class="section"><h3>Status</h3><pre id="diagnostics" data-i18n>Loading status…</pre></div><div class="section"><h3 data-i18n>Last M-Bus telegram</h3><pre id="hex">–</pre></div><div class="section"><h3>Logs</h3><pre id="logs">–</pre></div>
</section>
</main>
<div id="toast"></div>
<script>
const german = {
  ", commonly with TLS:": ", mit TLS häufig",
  ".": "mitsenden.",
  ". Change it after enabling the login.": ". Danach ändern.",
  "Added to the reading to match the meter register.": "Wird zum gelesenen Zählerstand addiert, um ihn an den realen Stand anzugleichen.",
  "Address of Mosquitto or another MQTT broker.": "Adresse von Mosquitto oder einem anderen MQTT-Broker.",
  "After restarting, OTA uploads can run without authentication.": "Nach dem Neustart sind OTA-Uploads wieder ohne Authentifizierung möglich.",
  "Alert after": "Hinweis nach",
  "Allows polling, restarting and selected configuration changes over MQTT.": "Erlaubt Poll-, Neustart- und ausgewählte Konfigurationsbefehle über MQTT.",
  "BK-G4 Gas Meter": "BK-G4 Gaszähler",
  "Back up settings, check M-Bus, view logs and maintain the device.": "Konfiguration sichern, M-Bus prüfen, Logs anzeigen und Gerät warten.",
  "Berlin (Central Europe)": "Berlin (Mitteleuropa)",
  "Broker connection, Home Assistant commands and optional TLS.": "Broker-Verbindung, Home-Assistant-Kommandos und optionale TLS-Absicherung.",
  "CA certificate (PEM)": "CA-Zertifikat (PEM)",
  "CHECK M-BUS": "M-BUS PRÜFEN",
  "Calorific value": "Brennwert",
  "Can also occur during heating.": "Auch beim Heizen möglich.",
  "Checking for updates…": "Versionsstand wird geprüft…",
  "Checking…": "Wird geprüft…",
  "Choose a configuration file": "Konfigurationsdatei auswählen",
  "Connecting…": "Verbinde…",
  "Correction factor": "Zustandszahl",
  "Could not check for updates. Installed version:": "Aktuellen Release konnte nicht geprüft werden. Installierte Version:",
  "Custom": "Benutzerdefiniert",
  "Custom time zone rule (POSIX)": "Eigene Zeitzonenregel (POSIX)",
  "DNS resolver, usually the router, Pi-hole or AdGuard Home.": "DNS-Resolver, meist Gateway, Pi-hole oder AdGuard Home.",
  "Daily history CSV": "Tageshistorie als CSV",
  "Default on first activation:": "Standard bei der ersten Aktivierung:",
  "Default:": "Standard:",
  "Delay before the alert. Long heating cycles can trigger it.": "Zeit bis zum Hinweis. Auch längere Heizphasen können ihn auslösen.",
  "Detected for {minutes} min. Can also occur during heating.": "Seit {minutes} Min. erkannt. Auch beim Heizen möglich.",
  "Development build. Release comparison is unavailable.": "Entwicklungs-Build: Ein Vergleich mit offiziellen Releases ist nicht möglich.",
  "Device name on the network and for mDNS, e.g.": "Gerätename im Netzwerk und für mDNS, z. B.",
  "Disable certificate verification": "Zertifikatsprüfung deaktivieren",
  "ESP32 Gas Meter": "ESP32 Gaszähler",
  "ESP32 meter gateway": "ESP32 Messgateway",
  "Enable Home Assistant commands": "Home-Assistant-Kommandos aktivieren",
  "Enable admin login": "Admin-Login aktivieren",
  "Enable the login if other devices or people can access this network.": "Die Anmeldung ist standardmäßig ausgeschaltet. Aktiviere sie, wenn andere Geräte oder Personen Zugriff auf das Netzwerk haben.",
  "Enable to assign a fixed IP address to the ESP32.": "Aktivieren, wenn der ESP32 nicht per DHCP adressiert werden soll.",
  "Encrypt MQTT traffic with TLS": "MQTT über TLS verschlüsseln",
  "Energy = volume × calorific value × correction factor.": "Energie = Volumen × Brennwert × Zustandszahl.",
  "Energy conversion": "Energieumrechnung",
  "Enter a password to replace the saved MQTT password.": "Das gespeicherte Passwort wird aus Sicherheitsgründen nicht angezeigt.",
  "Enter a password to replace the saved Wi-Fi password.": "Nur ausfüllen, wenn das gespeicherte WLAN-Passwort geändert werden soll.",
  "Export configuration": "Konfiguration exportieren",
  "Factory reset started": "Factory Reset gestartet",
  "Fixed IP address of the ESP32.": "Feste Adresse des ESP32.",
  "Flow rate": "Durchfluss",
  "Flow rate that starts the timer. Set to 0 to disable the alert.": "Ab diesem kontinuierlichen Durchfluss startet die Überwachungszeit. 0 deaktiviert sie.",
  "Flow threshold": "Dauerfluss-Schwelle",
  "Helsinki (Finland)": "Helsinki (Finnland)",
  "IP address": "IP-Adresse",
  "IP address of the router.": "Router-Adresse des Netzes.",
  "IP address or hostname": "IP-Adresse oder Hostname",
  "Import an exported JSON configuration, then restart to apply all settings.": "Importiert eine zuvor exportierte JSON-Konfiguration. Für die vollständige Übernahme anschließend manuell neu starten.",
  "Import and export": "Import und Export",
  "Import configuration": "Konfiguration importieren",
  "Imported. Restart the device to apply all settings.": "Importiert. Bitte anschließend manuell neu starten.",
  "Installed": "Installiert",
  "Language": "Sprache",
  "Last M-Bus telegram": "Letztes M-Bus-Telegramm",
  "Latest release": "Aktueller Release",
  "Leave blank to keep current value": "Leer lassen = unverändert",
  "Leave blank to keep the saved certificate.": "Leer lassen, um das vorhandene Zertifikat beizubehalten.",
  "Loading status": "Status wird geladen",
  "Loading status…": "Status wird geladen…",
  "Loading…": "Wird geladen…",
  "Login disabled": "Login aus",
  "Login enabled": "Login aktiv",
  "London (United Kingdom)": "London (Großbritannien)",
  "M-Bus poll started": "M-Bus-Abfrage gestartet",
  "M-Bus polling, meter correction, energy conversion and prolonged-flow alert.": "M-Bus-Abfrage, Zählerkorrektur, Energieumrechnung und Dauerfluss-Hinweis.",
  "M-Bus success rate": "M-Bus Erfolgsrate",
  "MQTT base topic": "MQTT-Basistopic",
  "MQTT broker": "MQTT-Broker",
  "MQTT connected": "MQTT verbunden",
  "MQTT disconnected": "MQTT getrennt",
  "MQTT password": "MQTT-Passwort",
  "MQTT port": "MQTT-Port",
  "MQTT username": "MQTT-Benutzer",
  "Main navigation": "Hauptnavigation",
  "Maintenance": "Wartung",
  "Maximum flow rate": "Maximaler Durchfluss",
  "Measurement": "Messung",
  "Meter offset": "Zähler-Offset",
  "Meter polling": "Erfassung",
  "Meter reading": "Zählerstand",
  "Name of the 2.4 GHz Wi-Fi network the ESP32 connects to.": "Name des 2,4-GHz-WLANs, mit dem sich der ESP32 verbindet.",
  "Network access, hostname and optional static IPv4 settings.": "Netzwerkzugang, Hostname und optionale feste IPv4-Konfiguration.",
  "New ArduinoOTA password": "Neues ArduinoOTA-Passwort",
  "New WebUI password": "Neues WebUI-Passwort",
  "No alert": "Kein Hinweis",
  "No entries": "Keine Einträge",
  "No password": "Ohne Passwort",
  "No readings": "Keine Messdaten",
  "Open GitHub release ↗": "GitHub-Release öffnen ↗",
  "Open project on GitHub": "Projekt auf GitHub öffnen",
  "Optional protection for settings and maintenance.": "Optionaler Schutz für Einstellungen und Wartungsfunktionen.",
  "Optional. Once set, pass the password to PlatformIO with": "Optional. Nach dem Setzen muss PlatformIO dieses Passwort mit",
  "Options and TLS": "Optionen und TLS",
  "Overview": "Übersicht",
  "Password set": "Passwort aktiv",
  "Poll M-Bus now": "M-Bus jetzt abfragen",
  "Poll interval": "Abfrageintervall",
  "Prolonged gas flow": "Längerer Gasfluss",
  "Prolonged-flow alert": "Dauerfluss-Hinweis",
  "Protects configuration, exports, logs and maintenance with HTTP Digest.": "Schützt Konfiguration, Exporte, Logs und Wartungsfunktionen mit HTTP Digest.",
  "Refresh logs": "Logs aktualisieren",
  "Remove the saved OTA password": "Gespeichertes OTA-Passwort entfernen",
  "Requires a TLS-capable broker and usually a CA certificate.": "Benötigt einen TLS-fähigen Broker und normalerweise ein CA-Zertifikat.",
  "Restart": "Neu starten",
  "Restart started": "Neustart gestartet",
  "Restart the device?": "Gerät neu starten?",
  "Save": "Speichern",
  "Saved. Login changes apply immediately. Other changes may require a restart.": "Gespeichert. Die Login-Einstellung wirkt sofort; andere Änderungen ggf. nach Neustart.",
  "Security": "Sicherheit",
  "Select a JSON file.": "Bitte JSON-Datei auswählen.",
  "Sets the day, month and year boundaries. Changes take effect after a restart.": "Legt Tages-, Monats- und Jahreswechsel fest. Änderungen gelten nach einem Neustart.",
  "Static IPv4": "Statische IPv4",
  "Subnet mask": "Subnetzmaske",
  "System and diagnostics": "System und Diagnose",
  "The WebUI login and optional ArduinoOTA password are managed separately.": "WebUI-Zugang und optionales ArduinoOTA-Passwort werden getrennt verwaltet.",
  "The installed firmware is newer than the latest public release.": "Die installierte Firmware ist neuer als der aktuelle öffentliche Release.",
  "The installed firmware is up to date.": "Die installierte Firmware ist aktuell.",
  "The latest GitHub release has no valid version number.": "Der aktuelle GitHub-Release hat keine gültige Versionsnummer.",
  "This month": "Dieser Monat",
  "This year": "Dieses Jahr",
  "Time": "Zeit",
  "Time between meter polls, from 10 to 3600 seconds.": "Wie oft der BK-G4 abgefragt wird. Zulässig: 10–3600 Sekunden.",
  "Time limit reached": "Zeitlimit erreicht",
  "Time not synced": "Zeit fehlt",
  "Time not synchronized": "Zeit nicht synchronisiert",
  "Time synced": "Zeit OK",
  "Time zone": "Zeitzone",
  "Today": "Heute",
  "Topic prefix for state, diagnostics and availability.": "Unter diesem Präfix werden Status, Diagnose und Verfügbarkeit veröffentlicht.",
  "Total energy": "Energie gesamt",
  "Type RESET to restore factory settings": "Zum Löschen RESET eingeben",
  "UTC (Coordinated Universal Time)": "UTC (Weltzeit)",
  "Unavailable": "Nicht erreichbar",
  "Unknown": "Unbekannt",
  "Update available. A newer firmware version has been released.": "Update verfügbar: Eine neuere Firmware wurde veröffentlicht.",
  "Upper limit for plausible flow readings.": "Plausibilitätsgrenze für die Durchflussberechnung.",
  "Use only for troubleshooting. This reduces security.": "Nur zur Fehlersuche verwenden; reduziert die Sicherheit.",
  "Use static IPv4 settings": "Statische IPv4-Konfiguration verwenden",
  "Use the value from your bill or network operator.": "Energieinhalt laut Abrechnung oder Netzbetreiber.",
  "User account on the broker.": "Benutzerkonto des Brokers.",
  "Usually 255.255.255.0 for a /24 network.": "Üblicherweise 255.255.255.0 bei /24.",
  "Waiting for a reading": "Noch kein Messwert",
  "Waiting for a telegram": "Warte auf Telegramm",
  "Waiting for fresh M-Bus readings": "Wartet auf aktuelle M-Bus-Werte",
  "WebUI login": "WebUI-Anmeldung",
  "WebUI username": "WebUI-Benutzer",
  "Wi-Fi": "WLAN",
  "Wi-Fi and network": "WLAN und Netzwerk",
  "Wi-Fi name (SSID)": "WLAN-Name (SSID)",
  "Wi-Fi password": "WLAN-Passwort",
  "Wireless firmware uploads on port 3232.": "Drahtlose Firmware-Uploads über Port 3232.",
  "Without a password, any device that can reach ArduinoOTA can upload firmware. Use this setting only on a trusted, isolated network.": "Ohne Passwort akzeptiert ArduinoOTA Uploads von jedem erreichbaren Gerät. Das ist bequem für ein abgeschottetes Management-/IoT-VLAN, sollte aber nicht in untrusted Netzen verwendet werden.",
  "e.g. IoT Wi-Fi": "z. B. IoT-WLAN",
  "minutes": "Minuten",
  "seconds": "Sekunden",
  "{successful} of {total} polls": "{successful} von {total} Abfragen"
};
let language = 'en';
try {
  if (localStorage.getItem('gasmeter-language') === 'de') language = 'de';
} catch {}
const t = english => language === 'de' ? german[english] ?? english : english;
const localized = [
  ...Array.from(document.querySelectorAll('[data-i18n]'), node => [node, 'textContent', node.textContent]),
  ...Array.from(document.querySelectorAll('[data-i18n-placeholder]'), node => [node, 'placeholder', node.placeholder]),
  ...Array.from(document.querySelectorAll('[data-i18n-aria-label]'), node => [node, 'aria-label', node.getAttribute('aria-label')])
];
function renderLanguage() {
  document.documentElement.lang = language;
  document.getElementById('language').value = language;
  for (const [node, field, english] of localized) {
    if (field === 'aria-label') node.setAttribute(field, t(english));
    else if (node[field] === english || node[field] === german[english]) {
      node[field] = t(english);
    }
  }
}
renderLanguage();

const $=id=>document.getElementById(id),fmt=(v,d=3)=>Number.isFinite(v)?v.toFixed(d):'–',RELEASE_API='https://api.github.com/repos/fgrfn/ESP32-BK-G4-Gasmeter/releases/latest';
let releaseCheckStarted=false;
let installedFirmware;
let releaseRequest;
let securityStatus;
$('language').onchange = () => {
  language = $('language').value;
  try { localStorage.setItem('gasmeter-language', language); } catch {}
  renderLanguage();
  if (releaseCheckStarted) checkLatestRelease(installedFirmware);
  if (securityStatus) renderSecurityStatus(securityStatus);
  status();
  logs();
};
function renderSecurityStatus(security) {
  const auth = !!security.web_auth_enabled, ota = !!security.ota_password_set;
  $('webAuthStatus').textContent = auth ? t('Login enabled') : t('Login disabled');
  $('webAuthStatus').className = 'status-pill ' + (auth ? 'ok' : 'warn');
  $('otaStatus').textContent = ota ? t('Password set') : t('No password');
  $('otaStatus').className = 'status-pill ' + (ota ? 'ok' : 'warn');
}
function toast(m){const t=$('toast');t.textContent=m;t.classList.add('show');clearTimeout(window.tt);window.tt=setTimeout(()=>t.classList.remove('show'),3000)}
function chip(id,text,state){const e=$(id);e.textContent=text;e.className='chip '+state}
function meter(v){const e=$('meterDigits');if(!Number.isFinite(v)||v<0){e.innerHTML='<span class="digit">–</span>';return}const p=v.toFixed(3).split('.'),w=p[0].padStart(6,'0').slice(-8);e.innerHTML=[...w].map(x=>`<span class="digit">${x}</span>`).join('')+'<span class="dot">.</span>'+[...p[1]].map(x=>`<span class="digit red">${x}</span>`).join('')}
function selectTab(name,updateHash=true){if(!document.getElementById('tab-'+name))name='overview';document.querySelectorAll('.tab-panel').forEach(x=>x.classList.toggle('active',x.id==='tab-'+name));document.querySelectorAll('.tab').forEach(x=>{const on=x.dataset.tab===name;x.classList.toggle('active',on);x.setAttribute('aria-selected',on?'true':'false')});if(updateHash&&location.hash!=='#'+name)history.replaceState(null,'','#'+name);window.scrollTo({top:0,behavior:'smooth'})}
document.querySelectorAll('.tab').forEach(x=>x.onclick=()=>selectTab(x.dataset.tab));window.addEventListener('hashchange',()=>selectTab(location.hash.slice(1),false));selectTab(location.hash.slice(1)||'overview',false);
async function json(url,opt){const r=await fetch(url,{credentials:'same-origin',...opt});if(!r.ok)throw new Error(await r.text());return r.json()}
function semver(v){const m=String(v||'').match(/^v?(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$/);return m?m.slice(1).map(Number):null}
function compareVersion(a,b){for(let i=0;i<3;i++){if(a[i]!==b[i])return a[i]-b[i]}return 0}
async function checkLatestRelease(installed){$('installedVersion').textContent=installed||t("Unknown");try{releaseRequest??=fetch(RELEASE_API,{headers:{Accept:'application/vnd.github+json'}}).then(response=>{if(!response.ok)throw new Error('HTTP '+response.status);return response.json()});const release=await releaseRequest,latest=release.tag_name||'';$('latestVersion').textContent=latest||t("Unknown");$('releaseLink').href=latest?'https://github.com/fgrfn/ESP32-BK-G4-Gasmeter/releases/tag/'+encodeURIComponent(latest):'https://github.com/fgrfn/ESP32-BK-G4-Gasmeter/releases';$('releaseLink').hidden=false;const currentParts=semver(installed),latestParts=semver(latest),box=$('versionStatus');if(!currentParts){box.textContent=t("Development build. Release comparison is unavailable.");box.className='notice info'}else if(!latestParts){box.textContent=t("The latest GitHub release has no valid version number.");box.className='notice'}else{const cmp=compareVersion(currentParts,latestParts);box.textContent=cmp<0?t("Update available. A newer firmware version has been released."):cmp===0?t("The installed firmware is up to date."):t("The installed firmware is newer than the latest public release.");box.className='notice '+(cmp===0?'ok':cmp>0?'info':'')}}catch(e){$('latestVersion').textContent=t("Unavailable");const box=$('versionStatus');box.textContent=t("Could not check for updates. Installed version:")+' '+(installed||t('Unknown'))+'.';box.className='notice'}}
function renderContinuousFlow(usage, healthy) {
  const indicator = $('flowAlert'), detail = $('flowMinutes');
  indicator.classList.toggle('warn-text', healthy && usage.continuous_flow_alert);
  if (!healthy) {
    indicator.textContent = t("No readings");
    detail.textContent = t("Waiting for fresh M-Bus readings");
    return;
  }
  indicator.textContent = usage.continuous_flow_alert ? t("Time limit reached") : t("No alert");
  const minutes = usage.continuous_flow_minutes;
  detail.textContent = minutes ? t('Detected for {minutes} min. Can also occur during heating.').replace('{minutes}',minutes) : t("Can also occur during heating.");
}

async function status(){try{const s=await json('/api/status'),health=s.safe_mode?'SAFE MODE':s.ap_mode?'SETUP AP':s.mbus.healthy?'ONLINE':t("CHECK M-BUS");$('identity').textContent=`${s.hostname} · ${s.ip}`;$('mode').textContent=health;$('firmware').textContent=`Firmware ${s.firmware}`;$('installedVersion').textContent=s.firmware||t("Unknown");if(!releaseCheckStarted){releaseCheckStarted=true;installedFirmware=s.firmware;checkLatestRelease(installedFirmware)}meter(s.usage.volume_m3);$('lastUpdated').textContent=s.usage.timestamp?new Date(s.usage.timestamp*1000).toLocaleString(language):t("Time not synchronized");$('energy').textContent=fmt(s.usage.energy_kwh,1);$('flow').textContent=fmt(s.usage.flow_m3h,4);$('mbus').textContent=fmt(s.mbus.success_rate,1)+' %';$('mbusDetail').textContent=t('{successful} of {total} polls').replace('{successful}',s.mbus.successful||0).replace('{total}',s.mbus.polls||0);$('day').textContent=fmt(s.usage.day_m3)+' m³';$('month').textContent=fmt(s.usage.month_m3)+' m³';$('year').textContent=fmt(s.usage.year_m3)+' m³';$('dayEnergy').textContent=fmt(s.usage.day_energy_kwh,1)+' kWh';$('monthEnergy').textContent=fmt(s.usage.month_energy_kwh,1)+' kWh';$('yearEnergy').textContent=fmt(s.usage.year_energy_kwh,1)+' kWh';renderContinuousFlow(s.usage,s.mbus.healthy);$('hex').textContent=s.mbus.last_hex||'–';$('diagnostics').textContent=JSON.stringify(s,null,2);chip('healthChip',health,s.safe_mode||!s.mbus.healthy?'bad':'ok');chip('timeChip',s.time_synchronized?t("Time synced"):t("Time not synced"),s.time_synchronized?'ok':'warn');chip('mqttChip',s.mqtt_connected?t("MQTT connected"):t("MQTT disconnected"),s.mqtt_connected?'ok':'warn')}catch(e){$('mode').textContent=e.message;chip('healthChip','Offline','bad')}}
setInterval(status,5000);status();
function updateCustomTimezone() {
  const custom = !$('timezone').value;
  $('timezoneCustomField').hidden = !custom;
  $('timezoneCustom').disabled = !custom;
}
function loadTimezone(rule) {
  const select = $('timezone');
  select.value = rule || select.options[0].value;
  if (select.selectedIndex < 0) select.value = '';
  $('timezoneCustom').value = select.value ? '' : rule;
  updateCustomTimezone();
}
$('timezone').onchange = updateCustomTimezone;
$('timezoneCustom').oninvalid = () => selectTab('wifi');

async function loadConfig(){try{const c=await json('/api/config');$('ssid').value=c.wifi.ssid||'';$('hostname').value=c.wifi.hostname||'';loadTimezone(c.timezone);$('staticEnabled').checked=!!c.wifi.static_ip_enabled;$('staticIp').value=c.wifi.static_ip||'';$('gateway').value=c.wifi.gateway||'';$('subnet').value=c.wifi.subnet||'';$('dns').value=c.wifi.dns||'';$('mqttHost').value=c.mqtt.host||'';$('mqttPort').value=c.mqtt.port||1883;$('mqttUser').value=c.mqtt.user||'';$('mqttTopic').value=c.mqtt.base_topic||'';$('mqttTls').checked=!!c.mqtt.tls;$('mqttInsecure').checked=!!c.mqtt.tls_insecure;$('mqttCommands').checked=!!c.mqtt.commands_enabled;$('poll').value=c.gas.poll_interval_seconds;$('offset').value=c.gas.meter_offset_m3;$('maxFlow').value=c.gas.max_flow_m3h;$('calorific').value=c.gas.calorific_value;$('correction').value=c.gas.correction_factor;$('flowThreshold').value=c.gas.continuous_flow_threshold_m3h;$('flowDelay').value=c.gas.continuous_flow_alert_minutes;securityStatus=c.security;$('webAuthEnabled').checked=!!securityStatus.web_auth_enabled;$('webUser').value=securityStatus.web_user||'';renderSecurityStatus(securityStatus)}catch(e){console.warn(e)}}
loadConfig();
function configPayload(){const c={timezone:$('timezone').value||$('timezoneCustom').value,wifi:{ssid:$('ssid').value,hostname:$('hostname').value,static_ip_enabled:$('staticEnabled').checked,static_ip:$('staticIp').value,gateway:$('gateway').value,subnet:$('subnet').value,dns:$('dns').value},mqtt:{host:$('mqttHost').value,port:+$('mqttPort').value,user:$('mqttUser').value,base_topic:$('mqttTopic').value,tls:$('mqttTls').checked,tls_insecure:$('mqttInsecure').checked,commands_enabled:$('mqttCommands').checked},gas:{poll_interval_seconds:+$('poll').value,meter_offset_m3:+$('offset').value,max_flow_m3h:+$('maxFlow').value,calorific_value:+$('calorific').value,correction_factor:+$('correction').value,continuous_flow_threshold_m3h:+$('flowThreshold').value,continuous_flow_alert_minutes:+$('flowDelay').value},security:{web_auth_enabled:$('webAuthEnabled').checked,web_user:$('webUser').value}};if($('wifiPassword').value)c.wifi.password=$('wifiPassword').value;if($('mqttPassword').value)c.mqtt.password=$('mqttPassword').value;if($('mqttCa').value)c.mqtt.ca_cert=$('mqttCa').value;if($('webPassword').value)c.security.web_password=$('webPassword').value;if($('clearOtaPassword').checked)c.security.ota_password='';else if($('otaPassword').value)c.security.ota_password=$('otaPassword').value;return c}
$('config').onsubmit=async e=>{e.preventDefault();try{await json('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(configPayload())});toast(t("Saved. Login changes apply immediately. Other changes may require a restart."))}catch(x){toast(x.message)}};
$('pollNow').onclick=()=>json('/api/mbus/poll',{method:'POST'}).then(()=>{toast(t("M-Bus poll started"));status()}).catch(e=>toast(e.message));$('export').onclick=()=>location.href='/api/config/export';$('history').onclick=()=>location.href='/api/usage.csv';
$('import').onclick=async()=>{const f=$('importFile').files[0];if(!f)return toast(t("Select a JSON file."));try{const text=await f.text();JSON.parse(text);await json('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:text});toast(t("Imported. Restart the device to apply all settings."))}catch(e){toast(e.message)}};
async function logs(){try{const l=await json('/api/logs');$('logs').textContent=l.logs.map(x=>`${x.timestamp_ms} ms  ${x.message}`).join('\n')||t("No entries")}catch(e){$('logs').textContent=e.message}}$('reloadLogs').onclick=logs;logs();
$('restart').onclick=async()=>{if(!confirm(t("Restart the device?")))return;await json('/api/restart',{method:'POST'});toast(t("Restart started"))};$('reset').onclick=async()=>{if(prompt(t("Type RESET to restore factory settings"))!=='RESET')return;await json('/api/factory-reset',{method:'POST',headers:{'Content-Type':'application/json'},body:'{"confirm":"RESET"}'});toast(t("Factory reset started"))};
if('serviceWorker'in navigator)navigator.serviceWorker.register('/sw.js').catch(()=>{});
</script>
</body>
</html>)HTML";
}
