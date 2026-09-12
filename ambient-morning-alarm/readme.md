# ambient-morning-alarm

Световой будильник на Tenstar Robot ESP32-C3 Super Mini. В назначенное время
включает 12-вольтовую ленту через IRF520, поддерживает обратный таймер и даёт
одинаковые настройки через веб-страницу, HTTP JSON API и Bluetooth LE.

## Для агента — читай это первым

Этот `readme.md` — единственный вход в проект. Его специально держат полным,
чтобы в следующей сессии **не открывать `.ino`, `web_page.h` и `tasks/`
«на разведку»**.

Правила:

1. Сначала этот файл целиком.
2. Исходники открывай только чтобы править конкретное место, которое уже
   названо ниже.
3. `concept-plan.md` и `tasks/` — исходное ТЗ. Они могут отставать от
   прошивки. Истина по текущему поведению — этот readme и код после него.
4. Соседний `../ambient-morning-alarm--apk/` не трогай, пока об этом не
   попросили. Общий JSON-контракт один, но APK пока не знает новых полей
   pulse (см. «Известные ограничения»).
5. После доработки сразу допиши сюда: версию, API, поля JSON, поведение
   веба, грабли. Иначе следующий агент снова полезет во все файлы.

Текущая прошивка: **00.00.004**. BLE-имя: `piper-light-alarm-0000004`.

Что сделано в 00.00.004:

- режим `strobe`: вспышка/пауза зашиты (`STROBE_ON_MS` / `STROBE_OFF_MS` = 100,
  яркость `STROBE_BRIGHTNESS` = 100). Снаружи только `totalSeconds`;
- в вебе у strobe прячутся лишние поля, в тесте режим выбран по умолчанию.

Что сделано в 00.00.003:

- у `pulse` появились `glowSeconds` (свечение) и `fadeSeconds` (затухание);
- в баннере веба показывается ближайший будильник из `GET /api/status`;
- каждый будильник сохраняется отдельно через `POST /api/alarm`;
- заголовок «Будильник N» оранжевый, пока чекбокс «включён» стоит.

## Куда смотреть, если уже нужно править код

| Задача | Файл | Что там |
| --- | --- | --- |
| Версия, пины, дефолты, NTP, лимиты | `config.h` | Константы. bump `FIRMWARE_VERSION_PATCH` вместе с BLE-именем. |
| Свет, будильники, NVS, HTTP, BLE | `ambient-morning-alarm.ino` | Всё ядро в одном скетче. |
| HTML/CSS/JS страницы | `web_page.h` | `WEB_PAGE_HTML` в `PROGMEM`, без CDN. |
| Сборка и заливка | `sborka.sh` + `.env` | WiFi не в исходниках. |
| Заводской WiFi | `.env.primer` → `.env` | `.env` в gitignore. |
| Исходное ТЗ человека | `prompt` | Зачем проект вообще. |
| Android | `../ambient-morning-alarm--apk/` | Отдельный проект. |

В `.ino` полезные якоря:

- `LightModeConfig` / `computeLightBrightness` — цикл ramp/pulse/strobe;
- `findNextAlarm` / `buildStatusJson` — ближайший будильник в статусе;
- `alarmFromJson` / `updateAlarm` / `handlePostAlarm` — патч одного будильника;
- `handleBleCommandJson` — BLE-команды, включая `setAlarm`.

## Железо

- Tenstar Robot ESP32-C3 Super Mini
- модуль IRF520 MOSFET
- DC-DC 12 В -> 5 В
- светодиодная лента 12 В
- внешний блок питания 12 В

PWM: GPIO4 (на шёлке платы «4») → `SIG` IRF520. Частота 5 кГц, 8 бит.
Встроенный синий LED платы (GPIO8, active LOW) зеркалит яркость: если тест
идёт, синий тоже горит, даже без ленты.

## Схема подключения

```text
                         Блок питания 12 В
                   +-------------+-------------+
                   |                           |
              +----v----+                 +----v------------------+
              | DC-DC   |                 | IRF520 MOSFET module  |
              | 12 -> 5 |                 | VIN+ / V+  <- +12 В   |
              +----+----+                 | VIN- / GND <- GND     |
                   | 5 В                  | OUT+       -> лента + |
          +--------v---------+            | OUT-       -> лента - |
          | ESP32-C3 Super   |            | SIG        <- GPIO4   |
          | Mini             |            | GND        <- общий GND
          | 5V/VBUS <- 5 В   |            +-----------------------+
          | GND     <- GND --+
          | GPIO4   -> SIG   |
          +------------------+
```

Общая земля обязательна: GND блока 12 В, GND DC-DC, GND ESP32-C3 и GND
логической части IRF520 должны быть соединены. ESP32 не питает ленту, только
даёт PWM на `SIG` IRF520.

## Что делает прошивка после старта

1. Гасит ленту и поднимает PWM.
2. Читает из Preferences (NVS) все сохранённые настройки: WiFi, будильники,
   таймер. Если NVS пустая — берёт заводские значения, для WiFi это флаги
   сборки `WIFI_SSID` / `WIFI_PASS`, и сразу пишет их в NVS.
3. Подключается к WiFi из NVS (не из исходников).
4. Синхронизирует время по NTP (`pool.ntp.org`, повтор раз в час).
   Часовой пояс сейчас жёстко `GMT+3`, DST выключен.
5. Поднимает HTTP на порту 80.
6. Рекламирует BLE-имя из версии, сейчас `piper-light-alarm-0000004`.
7. В цикле обслуживает веб, BLE, будильники и режимы света.

После потери питания конфигурация сохраняется. Активный запуск света в момент
отключения **не** продолжается: после включения лента гаснет и ждёт следующего
срабатывания по сохранённому расписанию.

Если WiFi не поднялся, прошивка не зависает: BLE и локальная логика продолжают
работать, NTP остаётся несинхронизированным до появления сети.

Новый будильник **не прерывает** уже идущий запуск (alarm / timer / test /
ожидание таймера). В ту же минуту тот же будильник второй раз не стартует.

## Режимы свечения

Яркость — проценты `0..100`. Для ramp/pulse финиш не может быть меньше старта.
`totalSeconds` всегда `> 0`. Для ramp/pulse ещё `rampSeconds` `> 0`.

### ramp

Линейный розжиг от `startBrightness` до `finishBrightness` за `rampSeconds`,
удержание финиша до `totalSeconds`, затем выключение.

При разборе JSON для ramp поля `glowSeconds`, `fadeSeconds`, `darkSeconds`
обнуляются.

### pulse

Цикл повторяется, пока не кончится `totalSeconds`, затем выключение:

1. **Розжиг** `rampSeconds` — от старта до финиша.
2. **Свечение** `glowSeconds` — удержание финишной яркости. `0` = фазу пропустить.
3. **Затухание** `fadeSeconds` — линейно с финиша до 0. `0` = фазу пропустить.
4. **Темнота** `darkSeconds` — яркость 0. Для pulse должно быть `> 0`.

Старые записи NVS без `glowSeconds` / `fadeSeconds` читаются как `0` и ведут
себя как старый pulse: розжиг, сразу темнота.

Заводской дефолт **нового** pulse (таймер, не сохранённые будильники):
розжиг 5 с, свечение 5 с, затухание 5 с, темнота 5 с, всего 600 с.

### strobe

Жёсткий мигающий сигнал. Тайминги **не** принимаются из JSON — они в `config.h`:

- `STROBE_ON_MS` = 100 — вспышка
- `STROBE_OFF_MS` = 100 — пауза
- `STROBE_BRIGHTNESS` = 100

Снаружи задаётся только `totalSeconds` (как долго стробоскопить), затем выключение.
Остальные поля mode при разборе обнуляются, яркость ставится в 100.

Веб прячет поля розжига/пульсации и показывает подсказку с зашитыми таймингами.
В блоке теста режим `strobe` выбран по умолчанию (10 с), чтобы подобрать длительность.

## Модель данных

Один JSON на веб, HTTP и BLE. WiFi в том же объекте настроек.

```json
{
  "wifi": { "ssid": "Home", "password": "secret" },
  "alarms": [
    {
      "id": 0,
      "enabled": true,
      "hour": 7,
      "minute": 30,
      "weekdaysMask": 31,
      "mode": {
        "type": "ramp",
        "startBrightness": 5,
        "finishBrightness": 100,
        "rampSeconds": 1200,
        "glowSeconds": 0,
        "fadeSeconds": 0,
        "darkSeconds": 0,
        "totalSeconds": 1800
      }
    }
  ],
  "timer": {
    "hours": 0,
    "minutes": 30,
    "mode": {
      "type": "pulse",
      "startBrightness": 5,
      "finishBrightness": 100,
      "rampSeconds": 5,
      "glowSeconds": 5,
      "fadeSeconds": 5,
      "darkSeconds": 5,
      "totalSeconds": 600
    }
  }
}
```

Ограничения:

- будильников 10, `id` = `0..9`;
- `hour` `0..23`, `minute` `0..59`;
- `weekdaysMask`: bit 0 = понедельник, bit 6 = воскресенье
  (`31` = Пн–Пт, это заводской дефолт);
- дни в `tm_wday` (0=вс) переводятся в эту маску функцией `weekdayBitFromTm`.

NVS: namespace `alarm`. Ключ `settings` — весь JSON строкой. SSID/пароль
ещё раз отдельно (`wifiSsid` / `wifiPass`), чтобы старые сохранения без
объекта `wifi` не затирали сеть.

## Статус устройства

`GET /api/status` и BLE `getStatus`:

```json
{
  "currentTime": "2026-09-12T10:15:00",
  "firmwareVersion": "00.00.004",
  "ntpSynced": true,
  "wifiConnected": true,
  "wifiSsid": "Home",
  "pwmPin": 4,
  "pwmBrightness": 0,
  "activeRun": null,
  "nextAlarm": {
    "id": 0,
    "hour": 7,
    "minute": 0,
    "inSeconds": 75600,
    "at": "2026-09-13T07:00:00"
  }
}
```

- `activeRun` = `null`, если ничего не светится и таймер не ждёт.
- Иначе `type`: `alarm` | `timer` | `test`. Для alarm есть `alarmId`.
- `nextAlarm` = `null`, если NTP нет или нет включённых будильников с
  ненулевой маской дней. Берётся ближайшее **будущее** срабатывание
  (текущая минута уже считается прошедшей).

## Веб-интерфейс

`http://IP:80/` — тёмная чёрно-оранжевая страница, без CDN.

- шапка: время, версия, WiFi, NTP, SSID, GPIO PWM, текущая яркость;
- **баннер**: в покое ближайший будильник
  (`Ближайший: Будильник 0 в 07:00, через 2 ч 15 мин`).
  Сообщения «сохранено» / ошибки на несколько секунд перекрывают его,
  потом баннер снова показывает ближайший;
- карточка активного запуска и «Стоп»;
- WiFi SSID/пароль;
- 10 будильников: время, дни, режим (`ramp` / `pulse` / `strobe`), поля pulse
  включая свечение и затухание. Для strobe видно только длительность.
  У каждого кнопка «Сохранить будильник N» → `POST /api/alarm` (другие
  несохранённые карточки не перечитываются).
  Общая кнопка «Сохранить настройки» по-прежнему пишет WiFi + все будильники +
  таймер;
- заголовок «Будильник N» оранжевый, пока стоит «включён» (меняется сразу
  по чекбоксу, до сохранения);
- таймер и тест свечения.

## HTTP API

Успех мутаций: `{"ok":true}`. Ошибка: `{"ok":false,"error":"..."}`.
CORS: `GET, POST, PATCH, OPTIONS`.

- `GET /` — HTML
- `GET /api/settings` — WiFi, будильники, таймер
- `POST /api/settings` — сохранить всё (включая WiFi в NVS, затем
  переподключение если SSID/пароль сменились)
- `POST /api/alarm` и `PATCH /api/alarm` — один будильник, тело:

  ```json
  {
    "id": 0,
    "enabled": true,
    "hour": 7,
    "minute": 0,
    "weekdaysMask": 31,
    "mode": { "type": "pulse", "startBrightness": 5, "finishBrightness": 100,
              "rampSeconds": 5, "glowSeconds": 5, "fadeSeconds": 5,
              "darkSeconds": 5, "totalSeconds": 600 }
  }
  ```

  Для `strobe` достаточно `type` и `totalSeconds`; вспышка/пауза/яркость
  зашиты в константы. Пример: `{ "type": "strobe", "totalSeconds": 10 }`.

- `GET /api/status` — см. выше, плюс `nextAlarm`
- `POST /api/stop` — остановить свет / ожидание таймера
- `POST /api/timer/start` — старт обратного таймера из сохранённых настроек
- `POST /api/test/start` — тело = объект mode (или `{ "mode": {...} }`)
- `POST /api/test/stop` — стоп теста

Коды ошибок разбора: `invalid-json`, `alarms-missing`, `timer-missing`,
`mode-missing`, `alarm-id-out-of-range`, `brightness-out-of-range`,
`finish-less-than-start`, `duration-must-be-positive`,
`pulse-dark-must-be-positive`, `wifi-ssid-invalid`, …

## Bluetooth LE

ESP32-C3 не умеет Bluetooth Classic SPP, канал — BLE GATT.

- Имя: `piper-light-alarm-` + major/minor/patch без точек. Для `00.00.004`
  это `piper-light-alarm-0000004`
- PIN / passkey: `8888`
- Service: `7c1b0000-7df0-4b6f-bc6f-a110c0000001`
- Command write: `7c1b0001-7df0-4b6f-bc6f-a110c0000001`
- Response notify/read: `7c1b0002-7df0-4b6f-bc6f-a110c0000001`
- Status notify каждые 2 с: `7c1b0003-7df0-4b6f-bc6f-a110c0000001`

Команды: `getSettings`, `setSettings`, `setAlarm`, `getWifi`, `setWifi`,
`getStatus`, `stop`, `startTimer`, `startTest`, `stopTest`.

`setAlarm` — тот же объект будильника, что `POST /api/alarm`, в `payload`.

```json
{ "requestId": "8", "command": "setWifi", "payload": { "ssid": "Home", "password": "secret" } }
```

Pairing зависит от телефона и стека ESP32 Arduino. Прошивка просит bonding
и passkey `8888`. Если окно PIN не показалось, GATT write после connect всё
равно принимается.

Большие ответы режутся на `{chunkIndex, chunkCount, data}`, кусок 180 байт.

## Версия

```text
00.00.004
```

Формат статуса: две цифры major, две minor, три patch. BLE-имя — те же цифры
без точек. Меняешь версию в `config.h` — поправь и этот readme.

## Установка инструментов

```bash
arduino-cli config init
arduino-cli config add board_manager.additional_urls \
  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install "ArduinoJson"
```

## Пароль WiFi

Пароля в исходниках нет. При первой прошивке значения из `.env` попадают в
сборку как заводской дефолт и сразу сохраняются в NVS. Дальше рабочие SSID и
пароль живут только в энергонезависимой памяти и меняются через веб / API /
BLE, без перепрошивки.

```bash
cp .env.primer .env
nano .env
```

Скрипт передаёт `WIFI_SSID` и `WIFI_PASS` компилятору через
`compiler.cpp.extra_flags`. Файл `.env` в git не попадает.

## Сборка и заливка

```bash
./sborka.sh           # только собрать
./sborka.sh zalit     # собрать и залить
```

В `.env`:

- `REZHIM=linux` — `arduino-cli` в Linux/WSL
- `REZHIM=wsl` — Windows `arduino-cli.exe` и COM-порт Windows

Плата по умолчанию: `esp32:esp32:esp32c3:PartitionScheme=min_spiffs`
(~1.9 MB под приложение: WiFi + BLE + веб не влезают в default 1.2 MB).

Без `WIFI_SSID` / `WIFI_PASS` скетч не собирается (`#error`).

## Что проверить после заливки

1. Serial: версия `00.00.004` и BLE-имя `piper-light-alarm-0000004`.
2. WiFi IP в логе.
3. `curl http://IP/api/status` — есть `nextAlarm` или `null`.
4. Веб: баннер ближайшего, оранжевый заголовок у включённого, сохранить
   один будильник, перезагрузить плату.
5. Тест `strobe` (по умолчанию 10 с), затем `pulse` со свечением и затуханием, затем стоп.
6. BLE scanner: `piper-light-alarm-0000004`.

## Известные ограничения

- Прошивка 00.00.004 залита 2026-09-12 на COM5 (esptool, Hash verified).
- Android APK ещё без `glowSeconds` / `fadeSeconds` / `strobe`. Если приложение
  сделает полный `setSettings` со старой схемой, прошивка запишет неизвестный
  `type` как `ramp` и обнулит свечение/затухание.
- Часовой пояс зашит `GMT+3`.
- Кривая яркости линейная, без калибровки под ленту.
- Незавершённый тест/запуск после ребута не восстанавливается.

## Дальнейшие доработки

- подтянуть Android под новые поля pulse, `strobe`, `setAlarm` / `nextAlarm`
- часовой пояс / DST без жёсткого `GMT_OFFSET_SEC`
- OTA обновление прошивки
- сохранение незавершённого теста при потере BLE
- отдельный индикатор «таймер ждёт» на веб-странице
- калибровка кривой яркости под конкретную ленту
