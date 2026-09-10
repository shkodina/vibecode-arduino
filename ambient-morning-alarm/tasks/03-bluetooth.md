# Task 03: Bluetooth control protocol

> **For agentic workers:** можно выполнять в отдельной сессии. Главная цель —
> сделать Bluetooth-канал с тем же смыслом команд, что и HTTP API.

## Цель

Добавить Bluetooth-управление для Android-приложения: подключение по имени
`piper-light-alarm-0000001`, PIN из `config.h`, чтение и запись настроек,
получение статуса, остановка активного свечения, запуск таймера и теста
свечения.

## Ожидаемые файлы

- Изменить: `ambient-morning-alarm/ambient-morning-alarm.ino`
- Изменить: `ambient-morning-alarm/config.h`
- При необходимости создать: `ambient-morning-alarm/bluetooth_protocol.h`
- При необходимости создать: `ambient-morning-alarm/bluetooth_protocol.cpp`

## Важное ограничение ESP32-C3

ESP32-C3 поддерживает BLE, но не поддерживает Bluetooth Classic SPP. Поэтому
делать канал как BLE GATT service, а не как Serial Bluetooth.

Если выбранная Arduino core версия не даёт удобного BLE API для ESP32-C3,
зафиксировать это в `readme.md` и выбрать библиотеку `NimBLE-Arduino`, но не
тащить лишние зависимости без необходимости.

## BLE профиль

Сервис:

```text
service UUID:        7c1b0000-7df0-4b6f-bc6f-a110c0000001
command UUID:        7c1b0001-7df0-4b6f-bc6f-a110c0000001
response UUID:       7c1b0002-7df0-4b6f-bc6f-a110c0000001
status notify UUID:  7c1b0003-7df0-4b6f-bc6f-a110c0000001
```

Характеристики:

- `command`: write, JSON-команды от приложения.
- `response`: read/notify, ответ на последнюю команду.
- `status notify`: notify, периодическая публикация статуса раз в 2 секунды.

## Имя и PIN

В `config.h`:

```cpp
#define BLUETOOTH_PIN "8888"
#define BLUETOOTH_NAME_PREFIX "piper-light-alarm-"
```

Функция имени должна вернуть `piper-light-alarm-0000001` для версии
`00.00.001`.

BLE pairing с PIN зависит от библиотеки и версии Android. Если PIN нельзя
жёстко потребовать на уровне BLE без ухудшения совместимости, включить bonding
и passkey там, где это поддержано, а в `readme.md` честно описать фактическое
поведение.

## Формат команд

Все команды идут JSON строкой в `command`.

Получить настройки:

```json
{
  "requestId": "1",
  "command": "getSettings"
}
```

Сохранить настройки:

```json
{
  "requestId": "2",
  "command": "setSettings",
  "payload": {
    "wifi": {
      "ssid": "MoyaWiFiSet",
      "password": "moy-skrytyy-parol"
    },
    "alarms": [],
    "timer": {}
  }
}
```

Получить WiFi:

```json
{
  "requestId": "8",
  "command": "getWifi"
}
```

Сохранить WiFi (пишет в NVS и переподключается):

```json
{
  "requestId": "9",
  "command": "setWifi",
  "payload": {
    "ssid": "MoyaWiFiSet",
    "password": "moy-skrytyy-parol"
  }
}
```

Получить статус:

```json
{
  "requestId": "3",
  "command": "getStatus"
}
```

Остановить активное свечение:

```json
{
  "requestId": "4",
  "command": "stop"
}
```

Запустить таймер:

```json
{
  "requestId": "5",
  "command": "startTimer"
}
```

Запустить тест свечения:

```json
{
  "requestId": "6",
  "command": "startTest",
  "payload": {
    "type": "pulse",
    "startBrightness": 5,
    "finishBrightness": 100,
    "rampSeconds": 5,
    "darkSeconds": 5,
    "totalSeconds": 60
  }
}
```

Остановить тест:

```json
{
  "requestId": "7",
  "command": "stopTest"
}
```

## Формат ответа

Успех:

```json
{
  "requestId": "1",
  "ok": true,
  "payload": {}
}
```

Ошибка:

```json
{
  "requestId": "1",
  "ok": false,
  "error": "unknown-command"
}
```

Для `getSettings` поле `payload` равно JSON из `buildSettingsJson()`.
Для `getStatus` поле `payload` равно JSON из `buildStatusJson()`.

## Зависимости от firmware core

Использовать функции:

```cpp
String buildStatusJson();
String buildSettingsJson();
bool applySettingsJson(const String& body, String& errorMessage);
bool startTimer();
bool startLightTest(const LightModeConfig& mode);
void stopActiveRun();
```

Bluetooth не должен иметь отдельную модель настроек. Он только принимает
команду, вызывает функции ядра и возвращает результат.

## Ограничения размера сообщений

BLE characteristic может иметь ограниченный MTU. Если полный JSON настроек не
помещается в одно уведомление:

1. включить MTU negotiation;
2. добавить chunking:
   - `chunkIndex`;
   - `chunkCount`;
   - `data`;
3. в Android-задаче использовать тот же chunking.

Не менять JSON-контракт настроек ради BLE. Лучше фрагментировать транспорт.

## Критерии готовности

- Устройство рекламируется как `piper-light-alarm-0000001`.
- Android может найти BLE service по UUID.
- Команды: `getSettings`, `setSettings`, `getWifi`, `setWifi`, `getStatus`,
  `stop`, `startTimer`, `startTest`, `stopTest`.
- `getStatus` возвращает тот же смысл данных, что `GET /api/status`.
- `getSettings` возвращает тот же смысл данных, что `GET /api/settings`, включая `wifi`.
- `setSettings` / `setWifi` сохраняют WiFi в NVS и переживают потерю питания.
- `stop`, `startTimer`, `startTest`, `stopTest` вызывают те же функции, что
  HTTP API.
- При неверной команде возвращается JSON с `ok=false`.

## Проверка

После сборки и заливки:

1. Найти устройство телефоном или BLE scanner.
2. Проверить имя `piper-light-alarm-0000001`.
3. Подключиться к service UUID.
4. Отправить `getStatus`.
5. Изменить один будильник через `setSettings`.
6. Изменить WiFi через `setWifi`, убедиться что после reboot берётся из NVS.
7. Перечитать настройки через веб и убедиться, что изменения общие.
8. Запустить `startTest`, убедиться, что лента светится.
9. Отправить `stop`, убедиться, что лента погасла.

