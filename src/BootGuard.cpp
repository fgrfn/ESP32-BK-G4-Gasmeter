#include "BootGuard.h"
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include "Logger.h"
#include "constants.h"

bool BootGuard::safeMode_ = false;
bool BootGuard::stable_ = false;
bool BootGuard::pendingOtaValidation_ = false;
uint8_t BootGuard::failureCount_ = 0;

bool BootGuard::isFailureResetReason(int reason) {
  return reason == ESP_RST_PANIC || reason == ESP_RST_INT_WDT || reason == ESP_RST_TASK_WDT ||
         reason == ESP_RST_WDT || reason == ESP_RST_BROWNOUT;
}

void BootGuard::begin() {
  Preferences preferences;
  preferences.begin("system", false);
  const bool plannedRestart = preferences.getBool("planned_restart", false);
  const String plannedReason = preferences.getString("planned_reason", "");
  preferences.remove("planned_restart");
  preferences.remove("planned_reason");
  failureCount_ = preferences.getUChar("boot_failures", 0);

  const int resetReason = static_cast<int>(esp_reset_reason());
  if (!plannedRestart && isFailureResetReason(resetReason)) {
    failureCount_ = failureCount_ == 255 ? 255 : static_cast<uint8_t>(failureCount_ + 1);
    preferences.putUChar("boot_failures", failureCount_);
    Logger::warn("Unstable reset detected, count=" + String(failureCount_));
  } else if (plannedRestart) {
    Logger::info("Planned restart completed: " + plannedReason);
  }
  preferences.end();

  safeMode_ = failureCount_ >= Constants::SAFE_MODE_BOOT_THRESHOLD;
  if (safeMode_) Logger::warn("Safe mode enabled after repeated unstable resets");

  const esp_partition_t* running = esp_ota_get_running_partition();
  esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
  pendingOtaValidation_ = running && esp_ota_get_state_partition(running, &state) == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY;
  if (pendingOtaValidation_) Logger::warn("OTA image is pending health validation");
}

void BootGuard::loop(bool networkHealthy, bool timeSynchronized, bool mbusHealthy) {
  if (stable_ || safeMode_ || millis() < Constants::STABLE_BOOT_AFTER_MS) return;
  const bool fallbackReached = millis() >= Constants::OTA_VALIDATION_FALLBACK_MS;
  if (!networkHealthy || !timeSynchronized || (!mbusHealthy && !fallbackReached)) return;

  Preferences preferences;
  preferences.begin("system", false);
  preferences.putUChar("boot_failures", 0);
  preferences.end();
  failureCount_ = 0;

  if (pendingOtaValidation_) {
    if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
      pendingOtaValidation_ = false;
      Logger::info("OTA image accepted after runtime health checks");
    } else {
      Logger::error("Failed to accept OTA image");
      return;
    }
  }
  stable_ = true;
  Logger::info("Boot marked stable");
}

bool BootGuard::safeMode() { return safeMode_; }
bool BootGuard::stable() { return stable_; }
bool BootGuard::pendingOtaValidation() { return pendingOtaValidation_; }
uint8_t BootGuard::failureCount() { return failureCount_; }

void BootGuard::markPlannedRestart(const char* reason) {
  Preferences preferences;
  if (!preferences.begin("system", false)) return;
  preferences.putBool("planned_restart", true);
  preferences.putString("planned_reason", reason ? reason : "unspecified");
  preferences.end();
}
