# ambient-morning-alarm

Световой будильник на Tenstar Robot ESP32-C3 Super Mini. В назначенное время
включает 12-вольтовую ленту через IRF520, поддерживает обратный таймер и даёт
одинаковые настройки через веб-страницу, HTTP JSON API и Bluetooth LE.

## Железо

- Tenstar Robot ESP32-C3 Super Mini
- модуль IRF520 MOSFET
- DC-DC 12 В -> 5 В
- светодиодная лента 12 В
- внешний блок питания 12 В

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
даёт PWM на `SIG` IRF520. Пин PWM задаётся в `config.h` как `LED_PWM_PIN`
(по умолчанию GPIO4).

## Что делает прошивка после старта

1. Гасит ленту и поднимает PWM.
2. Читает из Preferences (NVS) все сохранённые настройки: WiFi, будильники,
   таймер. Если NVS пустая — берёт заводские значения, для WiFi это флаги
   сборки `WIFI_SSID` / `WIFI_PASS`, и сразу пишет их в NVS.
3. Подключается к WiFi из NVS (не из исходников).
4. Синхронизирует время по NTP.
5. Поднимает HTTP на порту 80.
6. Рекламирует BLE-имя `piper-light-alarm-0000002`.
7. В цикле обслуживает веб, BLE, будильники и режимы света.

После потери питания конфигурация сохраняется. Активный запуск света в момент
отключения не продолжается: после включения лента гаснет и ждёт следующего
срабатывания по сохранённому расписанию.

Если WiFi не поднялся, прошивка не зависает: BLE и локальная логика продолжают
работать, NTP остаётся несинхронизированным до появления сети. WiFi SSID и
пароль можно сменить через веб, API или BLE — они пишутся в NVS, затем
устройство переподключается.

## Режимы свечения

- `ramp` — линейный розжиг от `startBrightness` до `finishBrightness` за
  `rampSeconds`, удержание до `totalSeconds`, затем выключение.
- `pulse` — розжиг за `rampSeconds`, затем темнота `darkSeconds`, цикл
  повторяется до `totalSeconds`, затем выключение.

Яркость задаётся в процентах `0..100`.

## Веб-интерфейс

Открой `http://IP_УСТРОЙСТВА/` с телефона или компьютера в той же WiFi-сети.

Страница тёмная, чёрно-оранжевая, без внешних CDN. На ней:

- текущее время, WiFi, NTP, версия;
- блок WiFi SSID/пароль (сохраняется в NVS);
- активный запуск и кнопка «Стоп»;
- 10 будильников;
- обратный таймер;
- тест свечения.

## HTTP API

- `GET /api/settings` — WiFi, будильники и таймер
- `POST /api/settings` — сохранить настройки (включая WiFi в NVS)
- `GET /api/status` — время, версия, NTP, WiFi, активный запуск
- `POST /api/stop` — остановить свет / ожидание таймера
- `POST /api/timer/start` — запустить обратный таймер
- `POST /api/test/start` — тест режима, тело = объект mode
- `POST /api/test/stop` — остановить тест

Ошибки:

```json
{"ok":false,"error":"opisanie"}
```

## Bluetooth LE

ESP32-C3 не умеет Bluetooth Classic SPP, поэтому канал сделан как BLE GATT.

- Имя: `piper-light-alarm-0000001` для версии `00.00.001`
- PIN / passkey: `8888` из `config.h`
- Service: `7c1b0000-7df0-4b6f-bc6f-a110c0000001`
- Command write: `7c1b0001-7df0-4b6f-bc6f-a110c0000001`
- Response notify/read: `7c1b0002-7df0-4b6f-bc6f-a110c0000001`
- Status notify: `7c1b0003-7df0-4b6f-bc6f-a110c0000001`

Команды: `getSettings`, `setSettings`, `getWifi`, `setWifi`, `getStatus`,
`stop`, `startTimer`, `startTest`, `stopTest`.

`getSettings` / `setSettings` включают объект:

```json
"wifi": { "ssid": "...", "password": "..." }
```

Отдельно через BLE:

```json
{ "requestId": "8", "command": "setWifi", "payload": { "ssid": "Home", "password": "secret" } }
```

WiFi-учётные данные хранятся в Preferences и переживают потерю питания.

Фактическое поведение pairing зависит от телефона и BLE stack ESP32 Arduino.
Прошивка запрашивает bonding/passkey `8888`; если телефон покажет окно PIN —
вводи `8888`. Если pairing прошёл без PIN, команды всё равно принимаются по
GATT write после connect.

Большие ответы режутся на чанки `{chunkIndex, chunkCount, data}`.

## Версия

```text
00.00.002
```

Формат статуса: две цифры major, две minor, три patch. BLE-имя — те же цифры
без точек: `piper-light-alarm-0000002`.

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

Создай `.env`:

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
(~1.9 MB под приложение: WiFi + BLE + веб иначе не влезают в default 1.2 MB).

## Что проверить после заливки

1. Serial: версия и BLE-имя.
2. WiFi IP в логе.
3. `curl http://IP/api/status`
4. Открыть веб-страницу, сохранить будильник, перезагрузить плату.
5. Запустить тест `ramp` и `pulse`, затем стоп.
6. Найти устройство BLE scanner'ом по имени `piper-light-alarm-0000001`.

## Файлы

```text
ambient-morning-alarm/
  ambient-morning-alarm.ino   ядро, веб/API, BLE, свет
  config.h                    константы и пины
  web_page.h                  HTML страницы
  sborka.sh                   сборка/заливка
  .env.primer                 пример .env
  .gitignore
  concept-plan.md
  tasks/                      подзадачи для агентов
  prompt                      исходное ТЗ
  readme.md                   этот файл
```

Рядом лежит `../ambient-morning-alarm--apk/` — Android-приложение.

## Дальнейшие доработки

- Android APK по `../ambient-morning-alarm--apk/tasks/`
- часовой пояс / DST без жёсткого `GMT_OFFSET_SEC`
- OTA обновление прошивки
- сохранение незавершённого теста при потере BLE
- отдельный индикатор «таймер ждёт» на веб-странице
- калибровка кривой яркости под конкретную ленту
