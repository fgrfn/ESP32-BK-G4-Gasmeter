#ifndef CONSTANTS_H
#define CONSTANTS_H

// ==========================================
// FIRMWARE VERSION
// ==========================================
#define FIRMWARE_VERSION "2.0.5"

// ==========================================
// DEBUG SETTINGS
// ==========================================
// Uncomment to enable debug output
// #define DEBUG_MODE
// #define DEBUG_MBUS
// #define DEBUG_MQTT
// #define DEBUG_WIFI

#ifdef DEBUG_MODE
  #define DEBUG_LOG(msg) Serial.println(String("[DEBUG] ") + msg)
  #define DEBUG_LOGF(fmt, ...) Serial.printf("[DEBUG] " fmt "\n", __VA_ARGS__)
#else
  #define DEBUG_LOG(msg)
  #define DEBUG_LOGF(fmt, ...)
#endif

#ifdef DEBUG_MBUS
  #define MBUS_DEBUG(msg) Serial.println(String("[MBUS] ") + msg)
#else
  #define MBUS_DEBUG(msg)
#endif

#ifdef DEBUG_MQTT
  #define MQTT_DEBUG(msg) Serial.println(String("[MQTT] ") + msg)
#else
  #define MQTT_DEBUG(msg)
#endif

#ifdef DEBUG_WIFI
  #define WIFI_DEBUG(msg) Serial.println(String("[WIFI] ") + msg)
#else
  #define WIFI_DEBUG(msg)
#endif

// ==========================================
// GPIO PIN DEFINITIONS
// ==========================================
#define STATUS_LED_PIN     2    // Onboard LED (GPIO2)
#define RESET_BUTTON_PIN   0    // BOOT Button (GPIO0)
#define MBUS_RX_PIN       16    // M-Bus UART RX (GPIO16)
#define MBUS_TX_PIN       17    // M-Bus UART TX (GPIO17)

// ==========================================
// M-BUS CONFIGURATION
// ==========================================
#define MBUS_BAUD              2400
#define MBUS_RESPONSE_TIMEOUT   500    // milliseconds
#define MBUS_BUFFER_SIZE        256    // bytes

// ==========================================
// TIMING CONSTANTS
// ==========================================
#define DEFAULT_POLL_INTERVAL      30000   // 30 seconds (milliseconds)
#define MIN_POLL_INTERVAL          10000   // 10 seconds minimum
#define MAX_POLL_INTERVAL         300000   // 5 minutes maximum
#define MEMORY_CHECK_INTERVAL      60000   // 1 minute
#define STATUS_PRINT_INTERVAL      60000   // 1 minute
#define AP_MODE_TIMEOUT           300000   // 5 minutes
#define INACTIVITY_TIMEOUT        600000   // 10 minutes
#define WIFI_CONNECT_TIMEOUT       15000   // 15 seconds
#define LED_BLINK_AP_MODE           100    // milliseconds
#define LED_BLINK_WIFI_ERROR        200    // milliseconds
#define LED_BLINK_MQTT_ERROR        500    // milliseconds
#define LED_BLINK_NORMAL           2000    // milliseconds

// ==========================================
// BUFFER SIZES & LIMITS
// ==========================================
#define MAX_LOG_ENTRIES            50
#define MAX_MEASUREMENTS           50
#define OTA_BUFFER_SIZE          1460    // bytes
#define MQTT_BUFFER_SIZE          512    // bytes
#define LOG_SAVE_THRESHOLD         10    // Save history every X measurements

// ==========================================
// MEMORY THRESHOLDS
// ==========================================
#define MEMORY_WARNING_THRESHOLD  10240   // 10 KB - warn when free heap below this
#define MEMORY_CRITICAL_THRESHOLD  3072   // 3 KB - restart when below this
#define MEMORY_CLEANUP_THRESHOLD   5120   // 5 KB - aggressive cleanup

// ==========================================
// DEFAULT CONFIGURATION VALUES
// ==========================================
#define DEFAULT_SSID              "SSID"
#define DEFAULT_PASSWORD          "Password"
#define DEFAULT_HOSTNAME          "ESP32-GasZaehler"
#define DEFAULT_MQTT_SERVER       "192.168.178.1"
#define DEFAULT_MQTT_PORT         1883
#define DEFAULT_MQTT_TOPIC        "gaszaehler/verbrauch"
#define DEFAULT_MQTT_CLIENT_ID    "ESP32GasClient"
#define DEFAULT_GAS_CALORIFIC     10.0f   // kWh/m³
#define DEFAULT_GAS_CORRECTION    1.0f    // Z-Zahl
#define DEFAULT_STATIC_IP         "192.168.1.100"
#define DEFAULT_GATEWAY           "192.168.1.1"
#define DEFAULT_SUBNET            "255.255.255.0"
#define DEFAULT_DNS               "192.168.1.1"

// ==========================================
// ACCESS POINT CONFIGURATION
// ==========================================
#define AP_SSID                   "ESP32-GasZaehler"
#define AP_PASSWORD               "12345678"  // Min 8 characters

// ==========================================
// NTP TIME CONFIGURATION
// ==========================================
#define NTP_SERVER                "de.pool.ntp.org"
#define GMT_OFFSET_SEC            3600    // UTC+1 (CET)
#define DAYLIGHT_OFFSET_SEC       3600    // DST offset

// ==========================================
// STRING BUFFER SIZES
// ==========================================
#define SSID_MAX_LEN              32
#define PASSWORD_MAX_LEN          64
#define HOSTNAME_MAX_LEN          32
#define MQTT_SERVER_MAX_LEN       64
#define MQTT_USER_MAX_LEN         64
#define MQTT_PASS_MAX_LEN         64
#define MQTT_TOPIC_MAX_LEN        64
#define MQTT_CLIENT_ID_MAX_LEN    32
#define IP_ADDRESS_MAX_LEN        16
#define ERROR_MSG_MAX_LEN         64

// ==========================================
// ANSI COLOR CODES (for Serial Monitor)
// ==========================================
#define ANSI_RESET                ""
#define ANSI_BOLD                 ""
#define ANSI_RED                  ""
#define ANSI_GREEN                ""
#define ANSI_YELLOW               ""
#define ANSI_BLUE                 ""
#define ANSI_MAGENTA              ""
#define ANSI_CYAN                 ""
#define ANSI_WHITE                ""

// Enable for colored output (requires ANSI-capable terminal)
// #define ENABLE_ANSI_COLORS
#ifdef ENABLE_ANSI_COLORS
  #undef ANSI_RESET
  #undef ANSI_BOLD
  #undef ANSI_RED
  #undef ANSI_GREEN
  #undef ANSI_YELLOW
  #undef ANSI_BLUE
  #undef ANSI_MAGENTA
  #undef ANSI_CYAN
  #undef ANSI_WHITE
  #define ANSI_RESET   "\033[0m"
  #define ANSI_BOLD    "\033[1m"
  #define ANSI_RED     "\033[31m"
  #define ANSI_GREEN   "\033[32m"
  #define ANSI_YELLOW  "\033[33m"
  #define ANSI_BLUE    "\033[34m"
  #define ANSI_MAGENTA "\033[35m"
  #define ANSI_CYAN    "\033[36m"
  #define ANSI_WHITE   "\033[37m"
#endif

#endif // CONSTANTS_H
