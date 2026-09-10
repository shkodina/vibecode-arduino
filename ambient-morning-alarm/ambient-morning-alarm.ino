// Ambient Morning Alarm — ESP32-C3 Super Mini
// WiFi + NTP + 10 будильников + таймер + PWM лента через IRF520
// Веб/API на порту 80 + BLE GATT для Android

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <time.h>
#include <inttypes.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLESecurity.h>

#include "config.h"
#include "web_page.h"

#ifndef WIFI_SSID
#error "WIFI_SSID ne zadan. Sobiray cherez ./sborka.sh, smotri readme.md"
#endif

#ifndef WIFI_PASS
#error "WIFI_PASS ne zadan. Sobiray cherez ./sborka.sh, smotri readme.md"
#endif

// Рабочие WiFi-учётные данные: сначала из NVS, иначе заводские из флагов сборки.
String wifiSsid;
String wifiPass;
bool wifiNeedsReconnect = false;

enum LightModeType {
  LIGHT_MODE_RAMP,
  LIGHT_MODE_PULSE
};

struct LightModeConfig {
  LightModeType type;
  uint8_t startBrightness;
  uint8_t finishBrightness;
  uint32_t rampSeconds;
  uint32_t darkSeconds;
  uint32_t totalSeconds;
};

struct AlarmConfig {
  uint8_t id;
  bool enabled;
  uint8_t hour;
  uint8_t minute;
  uint8_t weekdaysMask;
  LightModeConfig mode;
};

struct TimerConfig {
  uint8_t hours;
  uint8_t minutes;
  LightModeConfig mode;
};

enum ActiveRunType {
  ACTIVE_NONE,
  ACTIVE_ALARM,
  ACTIVE_TIMER,
  ACTIVE_TEST,
  ACTIVE_TIMER_WAIT
};

WebServer server(HTTP_PORT);
Preferences preferences;

AlarmConfig alarms[ALARM_COUNT];
TimerConfig timerConfig;

bool wifiConnected = false;
bool ntpSynced = false;
unsigned long lastWifiAttemptMs = 0;
unsigned long lastNtpAttemptMs = 0;

ActiveRunType activeType = ACTIVE_NONE;
int8_t activeAlarmId = -1;
LightModeConfig activeMode;
unsigned long activeStartedMs = 0;
unsigned long activeWaitUntilMs = 0;
unsigned long activeLightStartedMs = 0;
char activeStartedAt[32] = "";
uint8_t currentBrightness = 0;

int lastFiredMinuteKey[ALARM_COUNT];

BLEServer* bleServer = nullptr;
BLECharacteristic* bleCommandChar = nullptr;
BLECharacteristic* bleResponseChar = nullptr;
BLECharacteristic* bleStatusChar = nullptr;
bool bleClientConnected = false;
unsigned long lastStatusNotifyMs = 0;
String pendingBleResponse;
bool bleResponseReady = false;

const char* BLE_SERVICE_UUID = "7c1b0000-7df0-4b6f-bc6f-a110c0000001";
const char* BLE_COMMAND_UUID = "7c1b0001-7df0-4b6f-bc6f-a110c0000001";
const char* BLE_RESPONSE_UUID = "7c1b0002-7df0-4b6f-bc6f-a110c0000001";
const char* BLE_STATUS_UUID = "7c1b0003-7df0-4b6f-bc6f-a110c0000001";

// --- helpers ---

String firmwareVersionString() {
  char buf[16];
  snprintf(buf, sizeof(buf), "%02d.%02d.%03d",
           FIRMWARE_VERSION_MAJOR, FIRMWARE_VERSION_MINOR, FIRMWARE_VERSION_PATCH);
  return String(buf);
}

String bluetoothDeviceName() {
  char buf[48];
  snprintf(buf, sizeof(buf), "%s%02d%02d%03d",
           BLUETOOTH_NAME_PREFIX,
           FIRMWARE_VERSION_MAJOR, FIRMWARE_VERSION_MINOR, FIRMWARE_VERSION_PATCH);
  return String(buf);
}

const char* modeTypeToString(LightModeType type) {
  return type == LIGHT_MODE_PULSE ? "pulse" : "ramp";
}

LightModeType modeTypeFromString(const char* value) {
  if (value != nullptr && strcmp(value, "pulse") == 0) {
    return LIGHT_MODE_PULSE;
  }
  return LIGHT_MODE_RAMP;
}

LightModeConfig defaultRampMode() {
  LightModeConfig mode;
  mode.type = LIGHT_MODE_RAMP;
  mode.startBrightness = DEFAULT_RAMP_START_BRIGHTNESS;
  mode.finishBrightness = DEFAULT_RAMP_FINISH_BRIGHTNESS;
  mode.rampSeconds = DEFAULT_RAMP_SECONDS;
  mode.darkSeconds = 0;
  mode.totalSeconds = DEFAULT_RAMP_TOTAL_SECONDS;
  return mode;
}

LightModeConfig defaultPulseMode() {
  LightModeConfig mode;
  mode.type = LIGHT_MODE_PULSE;
  mode.startBrightness = DEFAULT_PULSE_START_BRIGHTNESS;
  mode.finishBrightness = DEFAULT_PULSE_FINISH_BRIGHTNESS;
  mode.rampSeconds = DEFAULT_PULSE_RAMP_SECONDS;
  mode.darkSeconds = DEFAULT_PULSE_DARK_SECONDS;
  mode.totalSeconds = DEFAULT_PULSE_TOTAL_SECONDS;
  return mode;
}

void applyFactoryWifiDefaults() {
  wifiSsid = WIFI_SSID;
  wifiPass = WIFI_PASS;
}

bool validateWifiCredentials(const String& ssid, const String& password, String& errorMessage) {
  if (ssid.length() == 0 || ssid.length() > WIFI_SSID_MAX_LEN) {
    errorMessage = "wifi-ssid-invalid";
    return false;
  }
  if (password.length() > WIFI_PASS_MAX_LEN) {
    errorMessage = "wifi-password-too-long";
    return false;
  }
  return true;
}

bool setWifiCredentials(const String& ssid, const String& password, String& errorMessage) {
  if (!validateWifiCredentials(ssid, password, errorMessage)) {
    return false;
  }
  if (ssid != wifiSsid || password != wifiPass) {
    wifiSsid = ssid;
    wifiPass = password;
    wifiNeedsReconnect = true;
  }
  return true;
}

void applyDefaultSettings() {
  applyFactoryWifiDefaults();

  for (uint8_t i = 0; i < ALARM_COUNT; i++) {
    alarms[i].id = i;
    alarms[i].enabled = false;
    alarms[i].hour = DEFAULT_ALARM_HOUR;
    alarms[i].minute = DEFAULT_ALARM_MINUTE;
    alarms[i].weekdaysMask = DEFAULT_ALARM_WEEKDAYS_MASK;
    alarms[i].mode = defaultRampMode();
    lastFiredMinuteKey[i] = -1;
  }

  timerConfig.hours = DEFAULT_TIMER_HOURS;
  timerConfig.minutes = DEFAULT_TIMER_MINUTES;
  timerConfig.mode = defaultPulseMode();
}

uint8_t weekdayBitFromTm(int tmWday) {
  // tm_wday: 0=вс ... 6=сб; наш bit0=пн ... bit6=вс
  if (tmWday == 0) {
    return 6;
  }
  return static_cast<uint8_t>(tmWday - 1);
}

bool localTimeNow(struct tm* out) {
  return getLocalTime(out, 50);
}

String formatLocalTimeIso() {
  struct tm nowTm;
  if (!localTimeNow(&nowTm)) {
    return "1970-01-01T00:00:00";
  }
  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &nowTm);
  return String(buf);
}

int minuteKeyFromTm(const struct tm& nowTm) {
  return nowTm.tm_yday * 24 * 60 + nowTm.tm_hour * 60 + nowTm.tm_min;
}

// --- PWM / light ---

void setupLedPwm() {
  ledcAttach(LED_PWM_PIN, LED_PWM_FREQUENCY, LED_PWM_RESOLUTION_BITS);
  ledcWrite(LED_PWM_PIN, 0);
  currentBrightness = 0;
}

uint32_t brightnessToDuty(uint8_t percent) {
  if (percent > 100) {
    percent = 100;
  }
  const uint32_t maxDuty = (1u << LED_PWM_RESOLUTION_BITS) - 1u;
  return (static_cast<uint32_t>(percent) * maxDuty) / 100u;
}

void setBrightnessPercent(uint8_t percent) {
  if (percent > 100) {
    percent = 100;
  }
  currentBrightness = percent;
  ledcWrite(LED_PWM_PIN, brightnessToDuty(percent));
}

void stopActiveRun() {
  activeType = ACTIVE_NONE;
  activeAlarmId = -1;
  activeStartedMs = 0;
  activeWaitUntilMs = 0;
  activeLightStartedMs = 0;
  activeStartedAt[0] = '\0';
  setBrightnessPercent(0);
}

uint8_t computeLightBrightness(unsigned long elapsedMs, const LightModeConfig& mode) {
  const unsigned long totalMs = mode.totalSeconds * 1000UL;
  if (elapsedMs >= totalMs) {
    return 0;
  }

  if (mode.type == LIGHT_MODE_RAMP) {
    const unsigned long rampMs = max(1UL, mode.rampSeconds * 1000UL);
    if (elapsedMs >= rampMs) {
      return mode.finishBrightness;
    }
    const float t = static_cast<float>(elapsedMs) / static_cast<float>(rampMs);
    const float delta = static_cast<float>(mode.finishBrightness - mode.startBrightness);
    return static_cast<uint8_t>(mode.startBrightness + delta * t + 0.5f);
  }

  const unsigned long rampMs = max(1UL, mode.rampSeconds * 1000UL);
  const unsigned long darkMs = mode.darkSeconds * 1000UL;
  const unsigned long cycleMs = rampMs + max(1UL, darkMs);
  const unsigned long phase = elapsedMs % cycleMs;
  if (phase >= rampMs) {
    return 0;
  }
  const float t = static_cast<float>(phase) / static_cast<float>(rampMs);
  const float delta = static_cast<float>(mode.finishBrightness - mode.startBrightness);
  return static_cast<uint8_t>(mode.startBrightness + delta * t + 0.5f);
}

void beginLightRun(ActiveRunType type, int8_t alarmId, const LightModeConfig& mode) {
  activeType = type;
  activeAlarmId = alarmId;
  activeMode = mode;
  activeLightStartedMs = millis();
  activeStartedMs = activeLightStartedMs;
  strncpy(activeStartedAt, formatLocalTimeIso().c_str(), sizeof(activeStartedAt) - 1);
  activeStartedAt[sizeof(activeStartedAt) - 1] = '\0';
  setBrightnessPercent(mode.startBrightness);
}

void updateLightEngine() {
  if (activeType == ACTIVE_NONE) {
    return;
  }

  const unsigned long nowMs = millis();

  if (activeType == ACTIVE_TIMER_WAIT) {
    if (nowMs >= activeWaitUntilMs) {
      beginLightRun(ACTIVE_TIMER, -1, timerConfig.mode);
    }
    return;
  }

  const unsigned long elapsedMs = nowMs - activeLightStartedMs;
  const unsigned long totalMs = activeMode.totalSeconds * 1000UL;
  if (elapsedMs >= totalMs) {
    stopActiveRun();
    return;
  }

  setBrightnessPercent(computeLightBrightness(elapsedMs, activeMode));
}

bool startTimer() {
  stopActiveRun();
  const unsigned long waitMs =
      (static_cast<unsigned long>(timerConfig.hours) * 3600UL +
       static_cast<unsigned long>(timerConfig.minutes) * 60UL) * 1000UL;

  activeType = ACTIVE_TIMER_WAIT;
  activeAlarmId = -1;
  activeMode = timerConfig.mode;
  activeStartedMs = millis();
  activeWaitUntilMs = activeStartedMs + waitMs;
  activeLightStartedMs = 0;
  strncpy(activeStartedAt, formatLocalTimeIso().c_str(), sizeof(activeStartedAt) - 1);
  activeStartedAt[sizeof(activeStartedAt) - 1] = '\0';
  setBrightnessPercent(0);

  if (waitMs == 0) {
    beginLightRun(ACTIVE_TIMER, -1, timerConfig.mode);
  }
  return true;
}

bool startLightTest(const LightModeConfig& mode) {
  stopActiveRun();
  beginLightRun(ACTIVE_TEST, -1, mode);
  return true;
}

bool startAlarmRun(const AlarmConfig& alarm) {
  stopActiveRun();
  beginLightRun(ACTIVE_ALARM, static_cast<int8_t>(alarm.id), alarm.mode);
  return true;
}

// --- validation / JSON ---

bool validateMode(const LightModeConfig& mode, String& errorMessage) {
  if (mode.startBrightness > 100 || mode.finishBrightness > 100) {
    errorMessage = "brightness-out-of-range";
    return false;
  }
  if (mode.finishBrightness < mode.startBrightness) {
    errorMessage = "finish-less-than-start";
    return false;
  }
  if (mode.rampSeconds == 0 || mode.totalSeconds == 0) {
    errorMessage = "duration-must-be-positive";
    return false;
  }
  if (mode.type == LIGHT_MODE_PULSE && mode.darkSeconds == 0) {
    errorMessage = "pulse-dark-must-be-positive";
    return false;
  }
  if (mode.type == LIGHT_MODE_RAMP && mode.darkSeconds != 0) {
    // допускаем любое, но нормализуем позже; валидация по ТЗ: darkSeconds может быть 0
  }
  return true;
}

bool validateAlarm(const AlarmConfig& alarm, String& errorMessage) {
  if (alarm.id >= ALARM_COUNT) {
    errorMessage = "alarm-id-out-of-range";
    return false;
  }
  if (alarm.hour > 23 || alarm.minute > 59) {
    errorMessage = "time-out-of-range";
    return false;
  }
  if (alarm.weekdaysMask > 127) {
    errorMessage = "weekdays-mask-out-of-range";
    return false;
  }
  return validateMode(alarm.mode, errorMessage);
}

bool validateTimer(const TimerConfig& timer, String& errorMessage) {
  if (timer.hours > 23 || timer.minutes > 59) {
    errorMessage = "timer-time-out-of-range";
    return false;
  }
  return validateMode(timer.mode, errorMessage);
}

void modeToJson(JsonObject obj, const LightModeConfig& mode) {
  obj["type"] = modeTypeToString(mode.type);
  obj["startBrightness"] = mode.startBrightness;
  obj["finishBrightness"] = mode.finishBrightness;
  obj["rampSeconds"] = mode.rampSeconds;
  obj["darkSeconds"] = mode.darkSeconds;
  obj["totalSeconds"] = mode.totalSeconds;
}

bool modeFromJson(JsonObjectConst obj, LightModeConfig& mode, String& errorMessage) {
  if (obj.isNull()) {
    errorMessage = "mode-missing";
    return false;
  }
  mode.type = modeTypeFromString(obj["type"] | "ramp");
  mode.startBrightness = obj["startBrightness"] | 0;
  mode.finishBrightness = obj["finishBrightness"] | 0;
  mode.rampSeconds = obj["rampSeconds"] | 0;
  mode.darkSeconds = obj["darkSeconds"] | 0;
  mode.totalSeconds = obj["totalSeconds"] | 0;
  if (mode.type == LIGHT_MODE_RAMP) {
    mode.darkSeconds = 0;
  }
  return validateMode(mode, errorMessage);
}

void settingsToJson(JsonDocument& doc) {
  JsonObject wifiObj = doc["wifi"].to<JsonObject>();
  wifiObj["ssid"] = wifiSsid;
  wifiObj["password"] = wifiPass;

  JsonArray alarmArr = doc["alarms"].to<JsonArray>();
  for (uint8_t i = 0; i < ALARM_COUNT; i++) {
    JsonObject alarmObj = alarmArr.add<JsonObject>();
    alarmObj["id"] = alarms[i].id;
    alarmObj["enabled"] = alarms[i].enabled;
    alarmObj["hour"] = alarms[i].hour;
    alarmObj["minute"] = alarms[i].minute;
    alarmObj["weekdaysMask"] = alarms[i].weekdaysMask;
    modeToJson(alarmObj["mode"].to<JsonObject>(), alarms[i].mode);
  }

  JsonObject timerObj = doc["timer"].to<JsonObject>();
  timerObj["hours"] = timerConfig.hours;
  timerObj["minutes"] = timerConfig.minutes;
  modeToJson(timerObj["mode"].to<JsonObject>(), timerConfig.mode);
}

String buildSettingsJson() {
  JsonDocument doc;
  settingsToJson(doc);
  String out;
  serializeJson(doc, out);
  return out;
}

bool applySettingsFromDoc(JsonVariantConst root, String& errorMessage) {
  if (!root["alarms"].is<JsonArrayConst>()) {
    errorMessage = "alarms-missing";
    return false;
  }
  if (!root["timer"].is<JsonObjectConst>()) {
    errorMessage = "timer-missing";
    return false;
  }

  // wifi опционален в старых JSON; если есть — валидируем и применяем.
  if (root["wifi"].is<JsonObjectConst>()) {
    JsonObjectConst wifiObj = root["wifi"].as<JsonObjectConst>();
    String nextSsid = wifiObj["ssid"] | "";
    String nextPass = wifiObj["password"] | "";
    if (!setWifiCredentials(nextSsid, nextPass, errorMessage)) {
      return false;
    }
  }

  AlarmConfig nextAlarms[ALARM_COUNT];
  for (uint8_t i = 0; i < ALARM_COUNT; i++) {
    nextAlarms[i] = alarms[i];
    nextAlarms[i].id = i;
  }

  JsonArrayConst alarmArr = root["alarms"].as<JsonArrayConst>();
  for (JsonObjectConst alarmObj : alarmArr) {
    uint8_t id = alarmObj["id"] | 255;
    if (id >= ALARM_COUNT) {
      errorMessage = "alarm-id-out-of-range";
      return false;
    }
    AlarmConfig alarm;
    alarm.id = id;
    alarm.enabled = alarmObj["enabled"] | false;
    alarm.hour = alarmObj["hour"] | 0;
    alarm.minute = alarmObj["minute"] | 0;
    alarm.weekdaysMask = alarmObj["weekdaysMask"] | 0;
    if (!modeFromJson(alarmObj["mode"].as<JsonObjectConst>(), alarm.mode, errorMessage)) {
      return false;
    }
    if (!validateAlarm(alarm, errorMessage)) {
      return false;
    }
    nextAlarms[id] = alarm;
  }

  TimerConfig nextTimer;
  JsonObjectConst timerObj = root["timer"].as<JsonObjectConst>();
  nextTimer.hours = timerObj["hours"] | 0;
  nextTimer.minutes = timerObj["minutes"] | 0;
  if (!modeFromJson(timerObj["mode"].as<JsonObjectConst>(), nextTimer.mode, errorMessage)) {
    return false;
  }
  if (!validateTimer(nextTimer, errorMessage)) {
    return false;
  }

  for (uint8_t i = 0; i < ALARM_COUNT; i++) {
    alarms[i] = nextAlarms[i];
  }
  timerConfig = nextTimer;
  return true;
}

bool applySettingsJson(const String& body, String& errorMessage) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    errorMessage = "invalid-json";
    return false;
  }
  if (!applySettingsFromDoc(doc.as<JsonVariantConst>(), errorMessage)) {
    return false;
  }
  saveSettings();
  if (wifiNeedsReconnect) {
    wifiNeedsReconnect = false;
    tryConnectWifi(true);
    trySyncNtp(true);
  }
  return true;
}

void saveWifiCredentials() {
  preferences.begin(PREFERENCES_NAMESPACE, false);
  preferences.putString(PREFERENCES_KEY_WIFI_SSID, wifiSsid);
  preferences.putString(PREFERENCES_KEY_WIFI_PASS, wifiPass);
  preferences.end();
}

void loadWifiCredentials() {
  preferences.begin(PREFERENCES_NAMESPACE, true);
  String storedSsid = preferences.getString(PREFERENCES_KEY_WIFI_SSID, "");
  String storedPass = preferences.getString(PREFERENCES_KEY_WIFI_PASS, "");
  preferences.end();

  if (storedSsid.length() == 0) {
    applyFactoryWifiDefaults();
    saveWifiCredentials();
    return;
  }

  String errorMessage;
  if (!setWifiCredentials(storedSsid, storedPass, errorMessage)) {
    applyFactoryWifiDefaults();
    saveWifiCredentials();
  }
  wifiNeedsReconnect = false;
}

void saveSettings() {
  String json = buildSettingsJson();
  preferences.begin(PREFERENCES_NAMESPACE, false);
  preferences.putString(PREFERENCES_KEY_SETTINGS, json);
  preferences.putString(PREFERENCES_KEY_WIFI_SSID, wifiSsid);
  preferences.putString(PREFERENCES_KEY_WIFI_PASS, wifiPass);
  preferences.end();
}

void loadSettings() {
  applyDefaultSettings();
  loadWifiCredentials();

  preferences.begin(PREFERENCES_NAMESPACE, true);
  String json = preferences.getString(PREFERENCES_KEY_SETTINGS, "");
  preferences.end();
  if (json.length() == 0) {
    saveSettings();
    return;
  }
  String errorMessage;
  JsonDocument doc;
  if (deserializeJson(doc, json)) {
    saveSettings();
    return;
  }
  // Не затираем WiFi из отдельных ключей, если в старом JSON поля wifi нет.
  const bool hadWifiInJson = doc["wifi"].is<JsonObject>();
  String keepSsid = wifiSsid;
  String keepPass = wifiPass;
  if (!applySettingsFromDoc(doc.as<JsonVariantConst>(), errorMessage)) {
    applyDefaultSettings();
    wifiSsid = keepSsid;
    wifiPass = keepPass;
    saveSettings();
    return;
  }
  if (!hadWifiInJson) {
    wifiSsid = keepSsid;
    wifiPass = keepPass;
    wifiNeedsReconnect = false;
    saveSettings();
  }
}

AlarmConfig getAlarm(uint8_t id) {
  if (id >= ALARM_COUNT) {
    return alarms[0];
  }
  return alarms[id];
}

bool updateAlarm(const AlarmConfig& alarm) {
  String errorMessage;
  if (!validateAlarm(alarm, errorMessage)) {
    return false;
  }
  alarms[alarm.id] = alarm;
  saveSettings();
  return true;
}

TimerConfig getTimerConfig() {
  return timerConfig;
}

bool updateTimerConfig(const TimerConfig& timer) {
  String errorMessage;
  if (!validateTimer(timer, errorMessage)) {
    return false;
  }
  timerConfig = timer;
  saveSettings();
  return true;
}

long remainingSecondsForActive() {
  if (activeType == ACTIVE_NONE) {
    return 0;
  }
  const unsigned long nowMs = millis();
  if (activeType == ACTIVE_TIMER_WAIT) {
    if (nowMs >= activeWaitUntilMs) {
      return 0;
    }
    return static_cast<long>((activeWaitUntilMs - nowMs + 999UL) / 1000UL);
  }
  const unsigned long elapsedMs = nowMs - activeLightStartedMs;
  const unsigned long totalMs = activeMode.totalSeconds * 1000UL;
  if (elapsedMs >= totalMs) {
    return 0;
  }
  return static_cast<long>((totalMs - elapsedMs + 999UL) / 1000UL);
}

String buildStatusJson() {
  JsonDocument doc;
  doc["currentTime"] = formatLocalTimeIso();
  doc["firmwareVersion"] = firmwareVersionString();
  doc["ntpSynced"] = ntpSynced;
  doc["wifiConnected"] = wifiConnected && (WiFi.status() == WL_CONNECTED);
  doc["wifiSsid"] = wifiSsid;

  if (activeType == ACTIVE_NONE) {
    doc["activeRun"] = nullptr;
  } else {
    JsonObject run = doc["activeRun"].to<JsonObject>();
    if (activeType == ACTIVE_ALARM) {
      run["type"] = "alarm";
      run["alarmId"] = activeAlarmId;
    } else if (activeType == ACTIVE_TEST) {
      run["type"] = "test";
      run["alarmId"] = nullptr;
    } else {
      run["type"] = "timer";
      run["alarmId"] = nullptr;
    }
    run["mode"] = modeTypeToString(activeMode.type);
    run["startedAt"] = activeStartedAt;
    run["remainingSeconds"] = remainingSecondsForActive();
    run["brightness"] = currentBrightness;
  }

  String out;
  serializeJson(doc, out);
  return out;
}

// --- scheduler / network ---

void checkAlarms() {
  if (!ntpSynced) {
    return;
  }
  if (activeType == ACTIVE_ALARM || activeType == ACTIVE_TEST ||
      activeType == ACTIVE_TIMER || activeType == ACTIVE_TIMER_WAIT) {
    // не прерываем уже идущий запуск новым будильником
    // (кроме случая, когда ничего не активно — тогда сработает ниже)
  }
  if (activeType != ACTIVE_NONE) {
    return;
  }

  struct tm nowTm;
  if (!localTimeNow(&nowTm)) {
    return;
  }

  const uint8_t dayBit = weekdayBitFromTm(nowTm.tm_wday);
  const int key = minuteKeyFromTm(nowTm);

  for (uint8_t i = 0; i < ALARM_COUNT; i++) {
    if (!alarms[i].enabled) {
      continue;
    }
    if (alarms[i].hour != nowTm.tm_hour || alarms[i].minute != nowTm.tm_min) {
      continue;
    }
    if ((alarms[i].weekdaysMask & (1u << dayBit)) == 0) {
      continue;
    }
    if (lastFiredMinuteKey[i] == key) {
      continue;
    }
    lastFiredMinuteKey[i] = key;
    startAlarmRun(alarms[i]);
    break;
  }
}

void tryConnectWifi(bool force) {
  const unsigned long nowMs = millis();
  if (!force && wifiConnected && WiFi.status() == WL_CONNECTED) {
    return;
  }
  if (!force && (nowMs - lastWifiAttemptMs) < WIFI_RETRY_INTERVAL_MS && lastWifiAttemptMs != 0) {
    return;
  }

  lastWifiAttemptMs = nowMs;
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(100);
  WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());

  const unsigned long startMs = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startMs) < WIFI_CONNECT_TIMEOUT_MS) {
    delay(200);
  }

  wifiConnected = (WiFi.status() == WL_CONNECTED);
}

void trySyncNtp(bool force) {
  const unsigned long nowMs = millis();
  if (!force && ntpSynced &&
      (nowMs - lastNtpAttemptMs) < (NTP_SYNC_INTERVAL_SECONDS * 1000UL)) {
    return;
  }
  if (!wifiConnected || WiFi.status() != WL_CONNECTED) {
    return;
  }

  lastNtpAttemptMs = nowMs;
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);

  struct tm nowTm;
  ntpSynced = getLocalTime(&nowTm, 5000);
}

// --- HTTP ---

void sendJson(int code, const String& body) {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.send(code, "application/json", body);
}

void sendOk() {
  sendJson(200, "{\"ok\":true}");
}

void sendError(const String& errorMessage) {
  JsonDocument doc;
  doc["ok"] = false;
  doc["error"] = errorMessage;
  String out;
  serializeJson(doc, out);
  sendJson(400, out);
}

void handleOptions() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.send(204);
}

void handleRoot() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send_P(200, "text/html; charset=utf-8", WEB_PAGE_HTML);
}

void handleGetSettings() {
  sendJson(200, buildSettingsJson());
}

void handlePostSettings() {
  String errorMessage;
  if (!applySettingsJson(server.arg("plain"), errorMessage)) {
    sendError(errorMessage);
    return;
  }
  sendOk();
}

void handleGetStatus() {
  sendJson(200, buildStatusJson());
}

void handleStop() {
  stopActiveRun();
  sendOk();
}

void handleTimerStart() {
  startTimer();
  sendOk();
}

void handleTestStart() {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    sendError("invalid-json");
    return;
  }

  LightModeConfig mode;
  String errorMessage;
  JsonObjectConst modeObj = doc.as<JsonObjectConst>();
  if (doc["type"].isNull() && doc["mode"].is<JsonObjectConst>()) {
    modeObj = doc["mode"].as<JsonObjectConst>();
  }
  if (!modeFromJson(modeObj, mode, errorMessage)) {
    sendError(errorMessage);
    return;
  }
  startLightTest(mode);
  sendOk();
}

void handleTestStop() {
  stopActiveRun();
  sendOk();
}

void setupHttp() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/settings", HTTP_OPTIONS, handleOptions);
  server.on("/api/settings", HTTP_GET, handleGetSettings);
  server.on("/api/settings", HTTP_POST, handlePostSettings);
  server.on("/api/status", HTTP_OPTIONS, handleOptions);
  server.on("/api/status", HTTP_GET, handleGetStatus);
  server.on("/api/stop", HTTP_OPTIONS, handleOptions);
  server.on("/api/stop", HTTP_POST, handleStop);
  server.on("/api/timer/start", HTTP_OPTIONS, handleOptions);
  server.on("/api/timer/start", HTTP_POST, handleTimerStart);
  server.on("/api/test/start", HTTP_OPTIONS, handleOptions);
  server.on("/api/test/start", HTTP_POST, handleTestStart);
  server.on("/api/test/stop", HTTP_OPTIONS, handleOptions);
  server.on("/api/test/stop", HTTP_POST, handleTestStop);
  server.begin();
}

// --- BLE ---

String wrapBleSuccess(const String& requestId, const String& payloadJson) {
  // payloadJson already a JSON value/object/array; embed raw
  String out = "{\"requestId\":\"";
  out += requestId;
  out += "\",\"ok\":true,\"payload\":";
  out += payloadJson.length() ? payloadJson : "null";
  out += "}";
  return out;
}

String wrapBleError(const String& requestId, const String& errorMessage) {
  JsonDocument doc;
  doc["requestId"] = requestId;
  doc["ok"] = false;
  doc["error"] = errorMessage;
  String out;
  serializeJson(doc, out);
  return out;
}

void notifyBleString(BLECharacteristic* characteristic, const String& value) {
  if (characteristic == nullptr || !bleClientConnected) {
    return;
  }
  characteristic->setValue(value.c_str());
  characteristic->notify();
}

void sendBleResponseChunked(const String& fullJson) {
  if (!bleClientConnected || bleResponseChar == nullptr) {
    return;
  }

  if (fullJson.length() <= BLE_CHUNK_SIZE) {
    notifyBleString(bleResponseChar, fullJson);
    return;
  }

  const int chunkCount = (fullJson.length() + BLE_CHUNK_SIZE - 1) / BLE_CHUNK_SIZE;
  for (int i = 0; i < chunkCount; i++) {
    const int start = i * BLE_CHUNK_SIZE;
    String piece = fullJson.substring(start, start + BLE_CHUNK_SIZE);
    JsonDocument doc;
    doc["chunkIndex"] = i;
    doc["chunkCount"] = chunkCount;
    doc["data"] = piece;
    String out;
    serializeJson(doc, out);
    notifyBleString(bleResponseChar, out);
    delay(20);
  }
}

bool parseModePayload(JsonVariantConst payload, LightModeConfig& mode, String& errorMessage) {
  if (payload.is<JsonObjectConst>()) {
    return modeFromJson(payload.as<JsonObjectConst>(), mode, errorMessage);
  }
  errorMessage = "payload-missing";
  return false;
}

void handleBleCommandJson(const String& body) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    pendingBleResponse = wrapBleError("", "invalid-json");
    bleResponseReady = true;
    return;
  }

  String requestId = doc["requestId"] | "";
  String command = doc["command"] | "";

  if (command == "getSettings") {
    pendingBleResponse = wrapBleSuccess(requestId, buildSettingsJson());
  } else if (command == "getStatus") {
    pendingBleResponse = wrapBleSuccess(requestId, buildStatusJson());
  } else if (command == "getWifi") {
    JsonDocument wifiDoc;
    wifiDoc["ssid"] = wifiSsid;
    wifiDoc["password"] = wifiPass;
    String wifiJson;
    serializeJson(wifiDoc, wifiJson);
    pendingBleResponse = wrapBleSuccess(requestId, wifiJson);
  } else if (command == "setWifi") {
    String errorMessage;
    JsonObjectConst payload = doc["payload"].as<JsonObjectConst>();
    if (payload.isNull()) {
      pendingBleResponse = wrapBleError(requestId, "payload-missing");
    } else {
      String nextSsid = payload["ssid"] | "";
      String nextPass = payload["password"] | "";
      if (!setWifiCredentials(nextSsid, nextPass, errorMessage)) {
        pendingBleResponse = wrapBleError(requestId, errorMessage);
      } else {
        saveSettings();
        if (wifiNeedsReconnect) {
          wifiNeedsReconnect = false;
          tryConnectWifi(true);
          trySyncNtp(true);
        }
        pendingBleResponse = wrapBleSuccess(requestId, "null");
      }
    }
  } else if (command == "setSettings") {
    String errorMessage;
    JsonVariantConst payload = doc["payload"];
    if (payload.isNull()) {
      pendingBleResponse = wrapBleError(requestId, "payload-missing");
    } else if (!applySettingsFromDoc(payload, errorMessage)) {
      pendingBleResponse = wrapBleError(requestId, errorMessage);
    } else {
      saveSettings();
      if (wifiNeedsReconnect) {
        wifiNeedsReconnect = false;
        tryConnectWifi(true);
        trySyncNtp(true);
      }
      pendingBleResponse = wrapBleSuccess(requestId, "null");
    }
  } else if (command == "stop" || command == "stopTest") {
    stopActiveRun();
    pendingBleResponse = wrapBleSuccess(requestId, "null");
  } else if (command == "startTimer") {
    startTimer();
    pendingBleResponse = wrapBleSuccess(requestId, "null");
  } else if (command == "startTest") {
    LightModeConfig mode;
    String errorMessage;
    if (!parseModePayload(doc["payload"], mode, errorMessage)) {
      pendingBleResponse = wrapBleError(requestId, errorMessage);
    } else {
      startLightTest(mode);
      pendingBleResponse = wrapBleSuccess(requestId, "null");
    }
  } else {
    pendingBleResponse = wrapBleError(requestId, "unknown-command");
  }

  bleResponseReady = true;
}

class CommandCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) override {
    String value = characteristic->getValue().c_str();
    if (value.length() == 0) {
      return;
    }
    handleBleCommandJson(value);
  }
};

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* server) override {
    bleClientConnected = true;
  }

  void onDisconnect(BLEServer* server) override {
    bleClientConnected = false;
    server->startAdvertising();
  }
};

class SecurityCallbacks : public BLESecurityCallbacks {
  uint32_t onPassKeyRequest() override {
    return static_cast<uint32_t>(atoi(BLUETOOTH_PIN));
  }

  void onPassKeyNotify(uint32_t pass_key) override {
    Serial.printf("BLE passkey notify: %06" PRIu32 "\n", pass_key);
  }

  bool onConfirmPIN(uint32_t pass_key) override {
    (void)pass_key;
    return true;
  }

  bool onSecurityRequest() override {
    return true;
  }

#if defined(CONFIG_BLUEDROID_ENABLED)
  void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) override {
    (void)cmpl;
  }
#endif

#if defined(CONFIG_NIMBLE_ENABLED)
  void onAuthenticationComplete(ble_gap_conn_desc* desc) override {
    (void)desc;
  }
#endif
};

void setupBluetooth() {
  const String name = bluetoothDeviceName();
  BLEDevice::init(name.c_str());
  BLEDevice::setSecurityCallbacks(new SecurityCallbacks());

  // ESP32 Arduino 3.x BLESecurity: static API + setPassKey(static, pin).
  BLESecurity::setAuthenticationMode(true, true, true);
  BLESecurity::setCapability(ESP_IO_CAP_OUT);
  BLESecurity::setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  BLESecurity::setPassKey(true, static_cast<uint32_t>(atoi(BLUETOOTH_PIN)));

  bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new ServerCallbacks());

  BLEService* service = bleServer->createService(BLE_SERVICE_UUID);

  bleCommandChar = service->createCharacteristic(
      BLE_COMMAND_UUID,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  bleCommandChar->setCallbacks(new CommandCallbacks());

  bleResponseChar = service->createCharacteristic(
      BLE_RESPONSE_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  bleResponseChar->addDescriptor(new BLE2902());

  bleStatusChar = service->createCharacteristic(
      BLE_STATUS_UUID,
      BLECharacteristic::PROPERTY_NOTIFY);
  bleStatusChar->addDescriptor(new BLE2902());

  service->start();

  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(BLE_SERVICE_UUID);
  advertising->setScanResponse(true);
  advertising->start();
}

void processBleTick() {
  if (bleResponseReady) {
    bleResponseReady = false;
    sendBleResponseChunked(pendingBleResponse);
  }

  if (!bleClientConnected || bleStatusChar == nullptr) {
    return;
  }

  const unsigned long nowMs = millis();
  if ((nowMs - lastStatusNotifyMs) < STATUS_NOTIFY_INTERVAL_MS) {
    return;
  }
  lastStatusNotifyMs = nowMs;
  notifyBleString(bleStatusChar, buildStatusJson());
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("ambient-morning-alarm " + firmwareVersionString());
  Serial.println("BLE name: " + bluetoothDeviceName());

  setupLedPwm();
  loadSettings();

  tryConnectWifi(true);
  trySyncNtp(true);
  setupHttp();
  setupBluetooth();

  if (wifiConnected) {
    Serial.print("WiFi IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi ne podklyuchen, BLE dostupen");
  }
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    wifiConnected = false;
    tryConnectWifi(false);
  } else {
    wifiConnected = true;
  }

  trySyncNtp(false);
  server.handleClient();
  updateLightEngine();
  checkAlarms();
  processBleTick();
}
