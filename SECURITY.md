# 🔐 Sicherheits-Features

## Übersicht

Dieses Dokument beschreibt die implementierten Sicherheitsfeatures.

## 1. WebUI Authentifizierung

### Aktivierung

In `main.cpp`:
```cpp
#include "WebAuth.h"

void setup() {
  // ... andere Initialisierungen
  
  // WebUI Authentifizierung aktivieren
  WebAuth::init("admin", "MeinSicheresPasswort123");
  
  // Oder über Config laden (empfohlen)
  // WebAuth::init(Config::web_username, Config::web_password);
}
```

In WebServer-Handlern:
```cpp
void handleDashboard() {
  if (!WebAuth::authenticate(server)) {
    return; // 401 Unauthorized wird automatisch gesendet
  }
  
  // Normaler Handler-Code
  server.send(200, "text/html", htmlPage);
}
```

### Konfiguration in Config.h/cpp erweitern

```cpp
// In Config.h
static char web_username[32];
static char web_password[64];
static bool web_auth_enabled;

// In Config.cpp laden/speichern
preferences.getString("web_user", web_username, sizeof(web_username));
preferences.getString("web_pass", web_password, sizeof(web_password));
web_auth_enabled = preferences.getBool("web_auth", false);
```

### Deaktivierung

```cpp
WebAuth::setEnabled(false);
// Oder leeres Passwort verwenden
WebAuth::init("admin", "");
```

## 2. MQTT-TLS Support

### Voraussetzungen

1. **MQTT Broker mit TLS konfiguriert** (z.B. Mosquitto)
2. **CA-Zertifikat** des Brokers (oder Root CA)
3. Optional: **Client-Zertifikat** für Mutual TLS

### Mosquitto TLS-Konfiguration

`/etc/mosquitto/mosquitto.conf`:
```conf
listener 8883
protocol mqtt
cafile /etc/mosquitto/ca_certificates/ca.crt
certfile /etc/mosquitto/certs/server.crt
keyfile /etc/mosquitto/certs/server.key

# Optional: Client-Zertifikat erzwingen
# require_certificate true
```

### ESP32 Konfiguration

#### Option A: CA-Zertifikat verwenden (empfohlen)

```cpp
#include "MQTTTls.h"

WiFiClientSecure secureClient;

void setup() {
  // TLS initialisieren
  MQTTTls::init(secureClient);
  MQTTTls::setCACert(MQTT_CA_CERT);  // Vordefiniertes Let's Encrypt Cert
  
  // Oder eigenes CA-Cert
  const char* myCACert = "-----BEGIN CERTIFICATE-----\n..."
  MQTTTls::setCACert(myCACert);
  
  // MQTT mit secure client initialisieren
  MQTTHandler::init(secureClient);
  
  // In Config: mqtt_port auf 8883 setzen
  Config::mqtt_port = 8883;
}
```

#### Option B: Insecure Mode (nur für Testing!)

```cpp
MQTTTls::init(secureClient);
MQTTTls::setInsecure(true);  // ⚠️ Keine Zertifikatsprüfung!
```

**WARNUNG:** Insecure Mode bietet KEINE echte Sicherheit - nur verschlüsselte Verbindung ohne Authentifizierung!

#### Option C: Client-Zertifikat (Mutual TLS)

```cpp
const char* clientCert = \
"-----BEGIN CERTIFICATE-----\n"
"MIIDXTCCAkWgAwIBAgI...\n"
"-----END CERTIFICATE-----\n";

const char* clientKey = \
"-----BEGIN PRIVATE KEY-----\n"
"MIIEvgIBADANBgkqhki...\n"
"-----END PRIVATE KEY-----\n";

MQTTTls::init(secureClient);
MQTTTls::setCACert(MQTT_CA_CERT);
MQTTTls::setClientCert(clientCert, clientKey);
```

### Zertifikat-Generierung

#### Selbstsigniertes CA-Zertifikat

```bash
# CA erstellen
openssl genrsa -out ca.key 2048
openssl req -new -x509 -days 3650 -key ca.key -out ca.crt \
  -subj "/CN=MQTT CA"

# Server-Zertifikat
openssl genrsa -out server.key 2048
openssl req -new -key server.key -out server.csr \
  -subj "/CN=mqtt.local"
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key \
  -CAcreateserial -out server.crt -days 365

# Client-Zertifikat (optional)
openssl genrsa -out client.key 2048
openssl req -new -key client.key -out client.csr \
  -subj "/CN=esp32-gaszaehler"
openssl x509 -req -in client.csr -CA ca.crt -CAkey ca.key \
  -CAcreateserial -out client.crt -days 365
```

#### CA-Zertifikat in Header konvertieren

```bash
cat ca.crt | sed 's/^/"/; s/$/\\n" \\/' > ca_cert.h
```

Dann in `MQTTTls.h` einfügen.

### Debugging TLS-Verbindungen

```cpp
// In constants.h
#define DEBUG_MQTT

// Dann in MQTTHandler::connect()
if (!connected) {
  Serial.println("TLS Error Code: " + String(secureClient.lastError()));
  MQTT_DEBUG("Verbindung fehlgeschlagen");
}
```

## 3. NVS Verschlüsselung

### ESP32 NVS Flash Encryption

⚠️ **WICHTIG:** NVS Encryption ist eine **One-Time-Operation** und kann nicht rückgängig gemacht werden!

### Partition Table anpassen

`partitions_encrypted.csv`:
```csv
# Name,   Type, SubType, Offset,  Size,    Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x1E0000,
app1,     app,  ota_1,   0x1F0000,0x1E0000,
nvs_key,  data, nvs_keys,0x3D0000,0x1000,  encrypted
```

### platformio.ini erweitern

```ini
[env:esp32dev-secure]
platform = espressif32
board = esp32dev
framework = arduino

board_build.partitions = partitions_encrypted.csv

build_flags = 
    -DCONFIG_NVS_ENCRYPTION=1

# Flash encryption aktivieren
board_build.flash_encryption = enabled
```

### Code-Änderungen

In `Config.cpp`:
```cpp
#include "nvs_flash.h"
#include "esp_partition.h"

void Config::init() {
  #ifdef CONFIG_NVS_ENCRYPTION
    // NVS mit Verschlüsselung initialisieren
    esp_err_t ret = nvs_flash_secure_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_secure_init();
    }
    ESP_ERROR_CHECK(ret);
    Serial.println("NVS Encryption aktiviert");
  #else
    nvs_flash_init();
  #endif
  
  setDefaults();
}
```

### Flash Encryption aktivieren

**Nur einmal durchführen - danach ist der Chip permanent verschlüsselt!**

```bash
# 1. Encryption Key generieren und flashen
esptool.py --port /dev/ttyUSB0 \
  burn_flash_encryption_key --do-not-confirm

# 2. Firmware mit Encryption flashen
pio run -e esp32dev-secure
esptool.py --port /dev/ttyUSB0 \
  --before default_reset --after hard_reset write_flash \
  --flash_mode dio --flash_freq 40m --flash_size detect \
  0x10000 .pio/build/esp32dev-secure/firmware.bin

# 3. Flash Encryption aktivieren (irreversibel!)
esptool.py --port /dev/ttyUSB0 burn_efuse FLASH_CRYPT_CNT
```

### Wichtige Hinweise

- ⚠️ **Flash Encryption kann NICHT deaktiviert werden** nach Aktivierung
- OTA Updates funktionieren weiterhin
- Performance-Einbußen: ~2-3% CPU-Last
- Einmal aktiviert, kann der Chip nur noch über OTA geupdatet werden
- Debugging wird schwieriger (kein direktes Flash-Auslesen mehr)

### Alternative: Software-basierte Verschlüsselung

Für sensible Daten ohne Hardware-Flash-Encryption:

```cpp
#include "mbedtls/aes.h"

class SecureStorage {
  static void encryptData(const char* plain, char* encrypted, size_t len);
  static void decryptData(const char* encrypted, char* plain, size_t len);
};

// Passwörter verschlüsselt speichern
char encrypted[128];
SecureStorage::encryptData(Config::password, encrypted, strlen(Config::password));
preferences.putBytes("wifi_pass", encrypted, 128);
```

## 4. Sichere Credential-Verwaltung

### Best Practices

✅ **DO:**
- WiFi/MQTT Credentials nur über WebUI konfigurieren
- HTTPS für WebUI verwenden (falls möglich)
- Starke Passwörter erzwingen (min. 12 Zeichen)
- Regelmäßig Passwörter ändern
- TLS für MQTT verwenden
- WebUI Authentifizierung aktivieren

❌ **DON'T:**
- Credentials im Code hardcoden
- Credentials über Serial Monitor ausgeben
- Standard-Passwörter verwenden
- Unverschlüsselte Connections über offenes WiFi

### Credential-Validierung

In `Config.cpp`:
```cpp
bool Config::validatePassword(const char* password) {
  size_t len = strlen(password);
  
  if (len < 12) {
    Serial.println("Passwort zu kurz (min. 12 Zeichen)");
    return false;
  }
  
  bool hasUpper = false, hasLower = false, hasDigit = false;
  for (size_t i = 0; i < len; i++) {
    if (isupper(password[i])) hasUpper = true;
    if (islower(password[i])) hasLower = true;
    if (isdigit(password[i])) hasDigit = true;
  }
  
  if (!hasUpper || !hasLower || !hasDigit) {
    Serial.println("Passwort muss Groß-, Kleinbuchstaben und Zahlen enthalten");
    return false;
  }
  
  return true;
}
```

## 5. Sicherheits-Checklist

### Vor Production-Deployment

- [ ] WebUI Authentifizierung aktiviert
- [ ] Starkes WebUI-Passwort gesetzt
- [ ] MQTT-TLS konfiguriert
- [ ] WiFi-Passwort komplex (WPA2)
- [ ] Standard-AP-Passwort geändert
- [ ] OTA-Updates nur über sicheres Netzwerk
- [ ] Debug-Ausgaben deaktiviert (keine Passwörter in Logs)
- [ ] Regelmäßige Firmware-Updates geplant
- [ ] Backup der Konfiguration erstellt

### Optional (Hochsicherheit)

- [ ] NVS Flash Encryption aktiviert
- [ ] Client-Zertifikate für MQTT
- [ ] Firewall-Regeln für MQTT Broker
- [ ] VPN für Remote-Zugriff
- [ ] Regelmäßige Sicherheits-Audits

## 6. Notfall-Wiederherstellung

### Passwort vergessen

```cpp
// BOOT-Button beim Start gedrückt halten = Config Reset
void setup() {
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
  
  if (digitalRead(RESET_BUTTON_PIN) == LOW) {
    Serial.println("Config Reset durch Button-Press");
    preferences.begin("gas-config", false);
    preferences.clear();
    preferences.end();
    ESP.restart();
  }
}
```

### Factory Reset via Serial

```cpp
void handleFactoryReset() {
  Serial.println("FACTORY RESET in 5 Sekunden...");
  delay(5000);
  
  preferences.begin("gas-config", false);
  preferences.clear();
  preferences.end();
  
  Serial.println("Reset abgeschlossen. Neustart...");
  ESP.restart();
}
```

Aktivierung: Sende `FACTORY_RESET` über Serial Monitor.

## Weitere Informationen

- [ESP32 Security Features](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/security/index.html)
- [Mosquitto TLS Configuration](https://mosquitto.org/man/mosquitto-tls-7.html)
- [Let's Encrypt für MQTT](https://letsencrypt.org/)
