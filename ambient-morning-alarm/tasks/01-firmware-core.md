# Task 01: firmware core

> **For agentic workers:** можно выполнять в отдельной сессии. Перед стартом
> прочитай `../concept-plan.md` и исходный `../prompt`.

## Цель

Создать основу прошивки для Tenstar Robot ESP32-C3 Super Mini: конфигурация,
WiFi, синхронизация времени, модель будильников и таймера, планировщик
срабатываний и PWM-управление 12-вольтовой лентой через IRF520.

## Ожидаемые файлы

- Создать: `ambient-morning-alarm/ambient-morning-alarm.ino`
- Создать: `ambient-morning-alarm/config.h`
- При необходимости создать: `ambient-morning-alarm/storage.h`
- Не трогать: Android-папку `ambient-morning-alarm--apk`

## Входные требования

- WiFi SSID и пароль не хранить в исходниках.
- Значения WiFi приходят через `WIFI_SSID` и `WIFI_PASS` на этапе сборки.
- Остальные быстрые настройки хранить в `config.h`.
- Версия статуса: `00.00.001`.
- Bluetooth-имя для этой версии: `piper-light-alarm-0000001`.
- NTP-сервер и период синхронизации брать из `config.h`.
- HTTP-порт по умолчанию `80`.
- Количество будильников по умолчанию `10`.

## Рекомендуемые библиотеки

- `WiFi.h`
- `WebServer.h`
- `time.h`
- `Preferences.h`

Для ESP32-C3 в Arduino core использовать LEDC PWM. Не использовать
ESP8266-специфичные библиотеки из соседних проектов.

## Контракт типов

Реализовать эти структуры или эквивалент с теми же полями:

```cpp
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
  ACTIVE_TEST
};
```

## Публичные функции для соседних задач

Веб/API и Bluetooth-задачи должны иметь возможность вызвать:

```cpp
void loadSettings();
void saveSettings();
AlarmConfig getAlarm(uint8_t id);
bool updateAlarm(const AlarmConfig& alarm);
TimerConfig getTimerConfig();
bool updateTimerConfig(const TimerConfig& timer);
bool startTimer();
bool startLightTest(const LightModeConfig& mode);
void stopActiveRun();
String buildStatusJson();
String buildSettingsJson();
bool applySettingsJson(const String& body, String& errorMessage);
```

Если реализация выберет другие имена, обязательно обновить task-файлы
`02-web-api.md` и `03-bluetooth.md`.

## Шаги реализации

1. Создать `config.h` с константами:
   - `FIRMWARE_VERSION_MAJOR`, `FIRMWARE_VERSION_MINOR`,
     `FIRMWARE_VERSION_PATCH`.
   - `BLUETOOTH_PIN`.
   - `ALARM_COUNT`.
   - `HTTP_PORT`.
   - `LED_PWM_PIN`.
   - `LED_PWM_CHANNEL`, `LED_PWM_FREQUENCY`, `LED_PWM_RESOLUTION_BITS`.
   - `NTP_SERVER`.
   - `NTP_SYNC_INTERVAL_SECONDS`.
   - дефолтные параметры `ramp` и `pulse`.
2. В `ambient-morning-alarm.ino` добавить compile-time проверки:
   - если `WIFI_SSID` не задан, сборка падает через `#error`;
   - если `WIFI_PASS` не задан, сборка падает через `#error`.
3. Реализовать подключение к WiFi:
   - старт через `WiFi.begin(WIFI_SSID, WIFI_PASS)`;
   - таймаут подключения взять из `config.h`;
   - если подключение не удалось, не зависать навсегда, а продолжить с
     `wifiConnected=false`, чтобы Bluetooth оставался доступным.
4. Реализовать NTP:
   - `configTime` с сервером из `config.h`;
   - хранить флаг последней успешной синхронизации;
   - повторять попытку не чаще `NTP_SYNC_INTERVAL_SECONDS`.
5. Создать дефолтные 10 будильников:
   - `id` от `0` до `9`;
   - все выключены;
   - время `07:00`;
   - дни недели `0b0111110` или другой явно описанный дефолт;
   - режим `ramp`.
6. Реализовать хранение настроек через `Preferences`:
   - namespace `alarm`;
   - сохранять весь JSON настроек одной строкой или отдельные поля;
   - при пустом хранилище использовать дефолты.
7. Реализовать планировщик:
   - проверять срабатывания без `delay`;
   - один будильник не должен стартовать повторно в ту же минуту;
   - дни недели учитывать по маске, понедельник = bit 0.
8. Реализовать движок света:
   - `ramp`: линейный рост яркости от старта до финиша за `rampSeconds`,
     затем удержание до `totalSeconds`, потом выключение;
   - `pulse`: рост от старта до финиша за `rampSeconds`, затем выключение
     на `darkSeconds`, повторять до `totalSeconds`;
   - по завершении всегда выставлять PWM `0`.
9. Реализовать `stopActiveRun()`:
   - сбрасывает активный запуск;
   - гасит ленту;
   - не меняет сохранённые настройки.
10. Реализовать `buildStatusJson()` и `buildSettingsJson()` на базе того же
    контракта, что описан в `../concept-plan.md`.

## Критерии готовности

- Скетч компилируется для ESP32-C3 через Arduino CLI.
- Без `WIFI_SSID` или `WIFI_PASS` сборка падает с понятной ошибкой.
- При наступлении времени включённого будильника запускается выбранный режим.
- По окончании `totalSeconds` лента гаснет.
- `stopActiveRun()` гасит ленту из любого активного режима.
- JSON настроек и статуса стабилен для веб/API, Bluetooth и Android.

## Проверка

После появления `sborka.sh` из `04-build-docs.md`:

```bash
cd ambient-morning-alarm
./sborka.sh
```

Ручная проверка на плате:

1. Залить прошивку.
2. Поставить ближайший будильник на текущий день.
3. Убедиться, что лента стартует в нужную минуту.
4. Убедиться, что стоп гасит ленту.
5. Проверить оба режима на коротких длительностях.

