#ifndef AMBIENT_MORNING_ALARM_CONFIG_H
#define AMBIENT_MORNING_ALARM_CONFIG_H

// Версия прошивки: статус "00.00.001", BLE-имя "piper-light-alarm-0000001".
#define FIRMWARE_VERSION_MAJOR 0
#define FIRMWARE_VERSION_MINOR 0
#define FIRMWARE_VERSION_PATCH 3

#define BLUETOOTH_PIN "8888"
#define BLUETOOTH_NAME_PREFIX "piper-light-alarm-"

#define ALARM_COUNT 10
#define HTTP_PORT 80

// На шёлке платы «4» = GPIO4. SIG модуль IRF520 сюда.
#define LED_PWM_PIN 4
#define LED_PWM_CHANNEL 0
#define LED_PWM_FREQUENCY 5000
#define LED_PWM_RESOLUTION_BITS 8

// Встроенный синий LED Super Mini (active LOW) — зеркало для проверки, что тест идёт.
#define ONBOARD_LED_PIN 8
#define ONBOARD_LED_ACTIVE_LOW 1

#define NTP_SERVER "pool.ntp.org"
#define NTP_SYNC_INTERVAL_SECONDS 3600
#define GMT_OFFSET_SEC (3 * 3600)
#define DAYLIGHT_OFFSET_SEC 0

#define WIFI_CONNECT_TIMEOUT_MS 20000
#define WIFI_RETRY_INTERVAL_MS 30000

#define PREFERENCES_NAMESPACE "alarm"
#define PREFERENCES_KEY_SETTINGS "settings"
#define PREFERENCES_KEY_WIFI_SSID "wifiSsid"
#define PREFERENCES_KEY_WIFI_PASS "wifiPass"

// Ограничения WiFi (стандарт IEEE 802.11).
#define WIFI_SSID_MAX_LEN 32
#define WIFI_PASS_MAX_LEN 63

#define STATUS_NOTIFY_INTERVAL_MS 2000
#define BLE_CHUNK_SIZE 180

// Дефолт ramp: медленный розжиг ~20 мин, всего 30 мин.
#define DEFAULT_RAMP_START_BRIGHTNESS 5
#define DEFAULT_RAMP_FINISH_BRIGHTNESS 100
#define DEFAULT_RAMP_SECONDS 1200
#define DEFAULT_RAMP_TOTAL_SECONDS 1800

// Дефолт pulse для таймера и тестовых значений.
#define DEFAULT_PULSE_START_BRIGHTNESS 5
#define DEFAULT_PULSE_FINISH_BRIGHTNESS 100
#define DEFAULT_PULSE_RAMP_SECONDS 5
#define DEFAULT_PULSE_GLOW_SECONDS 5
#define DEFAULT_PULSE_FADE_SECONDS 5
#define DEFAULT_PULSE_DARK_SECONDS 5
#define DEFAULT_PULSE_TOTAL_SECONDS 600

// Дефолтные будильники: выключены, 07:00, Пн-Пт (биты 0..4).
#define DEFAULT_ALARM_HOUR 7
#define DEFAULT_ALARM_MINUTE 0
#define DEFAULT_ALARM_WEEKDAYS_MASK 0b0011111

#define DEFAULT_TIMER_HOURS 0
#define DEFAULT_TIMER_MINUTES 30

#endif
