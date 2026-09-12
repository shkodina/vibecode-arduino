#ifndef AMBIENT_MORNING_ALARM_CONFIG_H
#define AMBIENT_MORNING_ALARM_CONFIG_H

// Версия прошивки: статус "00.00.001", BLE-имя "piper-light-alarm-0000001".
#define FIRMWARE_VERSION_MAJOR 0
#define FIRMWARE_VERSION_MINOR 0
#define FIRMWARE_VERSION_PATCH 15

#define BLUETOOTH_PIN "8888"
#define BLUETOOTH_NAME_PREFIX "piper-light-alarm-"

#define ALARM_COUNT 10
#define BLE_INIT_STACK_SIZE 16384
#define HTTP_TASK_STACK_SIZE 8192
#define HTTP_PORT 80

// Режим радио при старте (INPUT_PULLUP, замыкание на GND).
// Super Mini: ряд GND–5–6–7–8–10. Не GPIO8 (LED) и не GPIO9 (BOOT).
// GPIO5 → GND = только BLE. GPIO6 → GND = только WiFi.
// Ни одной / обе = только WiFi (одновременный WiFi+BLE на C3 не тянет).
#define RADIO_SELECT_BLE_PIN 5
#define RADIO_SELECT_WIFI_PIN 6

// Холодный старт с БП: WiFi не сразу.
#define RADIO_START_DELAY_MS 3000

// На шёлке платы «4» = GPIO4. SIG MOSFET (LR7843) сюда.
// Логический MOSFET с 3.3 В: глаз насыщается уже к ~10% линейного duty.
// Процент в UI — воспринимаемая яркость:
// duty = (p/100)^gamma * visualDuty * maxDuty.
// 500 Гц / 12 бит — импульс на малых duty длиннее, чем при 5 кГц.
#define LED_PWM_PIN 4
#define LED_PWM_CHANNEL 0
#define LED_PWM_FREQUENCY 500
#define LED_PWM_RESOLUTION_BITS 12
#define LED_PWM_GAMMA 2.0f
#define LED_PWM_VISUAL_DUTY 0.12f

// Встроенный синий LED Super Mini (active LOW) — зеркало для проверки, что тест идёт.
#define ONBOARD_LED_PIN 8
#define ONBOARD_LED_ACTIVE_LOW 1

#define NTP_SERVER "pool.ntp.org"
#define NTP_SYNC_INTERVAL_SECONDS 3600
#define GMT_OFFSET_SEC (3 * 3600)
#define DAYLIGHT_OFFSET_SEC 0

#define WIFI_CONNECT_TIMEOUT_MS 20000
#define WIFI_RETRY_INTERVAL_MS 30000

#define DEFAULT_WATCHDOG_SECONDS 30
#define WATCHDOG_SECONDS_MIN 5
#define WATCHDOG_SECONDS_MAX 120

#define PREFERENCES_NAMESPACE "alarm"
#define PREFERENCES_KEY_SETTINGS "settings"
#define PREFERENCES_KEY_WIFI_SSID "wifiSsid"
#define PREFERENCES_KEY_WIFI_PASS "wifiPass"
#define PREFERENCES_KEY_WATCHDOG "watchdogSec"

// Ограничения WiFi (стандарт IEEE 802.11).
#define WIFI_SSID_MAX_LEN 32
#define WIFI_PASS_MAX_LEN 63

#define STATUS_NOTIFY_INTERVAL_MS 10000
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

// Стробоскоп: вспышка/пауза зашиты, снаружи только totalSeconds.
#define STROBE_ON_MS 100
#define STROBE_OFF_MS 100
#define STROBE_BRIGHTNESS 100
#define DEFAULT_STROBE_TOTAL_SECONDS 10

// Дефолтные будильники: выключены, 07:00, Пн-Пт (биты 0..4).
#define DEFAULT_ALARM_HOUR 7
#define DEFAULT_ALARM_MINUTE 0
#define DEFAULT_ALARM_WEEKDAYS_MASK 0b0011111

#define DEFAULT_TIMER_HOURS 0
#define DEFAULT_TIMER_MINUTES 30

#endif
