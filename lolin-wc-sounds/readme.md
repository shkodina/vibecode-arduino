# lolin-wc-sounds

Модуль для туалета / коридора: датчик движения включает музыку с SD-карты
через мини-колонки. Плата — **Lolin NodeMCU v3 (ESP8266)**, как в
`lolin-web-pulse`.

Управление с компьютера — соседний проект
`../lolin-wc-sounds--control-plane/` (скан LAN, тёмный UI, ffmpeg).
Контракт API и грабли загрузки описаны и там, и здесь: не восстанавливать
поведение по скетчу, если можно прочитать этот файл.

Живой экземпляр (2026-09): DHCP `192.168.88.28`, заливка USB **COM4**
из WSL (`REZHIM=wsl` в `.env`). WiFi-пароль **не** в прошивке — только
в `/config.json` на карте.

## Что делает

1. При старте читает `/config.json` с SD-карты (FAT32).
2. Подключается к WiFi из конфига. Если сети нет — поднимает точку доступа
  `WC-Sounds` / пароль `wcsounds1`.
3. Раз в час (настройка `ntp.update_interval`) синхронизирует часы с NTP.
4. По срабатыванию HC-SR501 выбирает период расписания и играет WAV
  из указанной папки.
5. Если движения нет `motion.timeout_seconds` секунд — останавливает звук.
6. Веб на порту 80:
  - `/` — страница настроек и загрузки файлов;
  - `/api/...` — управление, в том числе reload и reboot;
  - `/swagger/index.html` — описание API.
7. Watchdog ~8 с (`ESP.wdtEnable(8000)`). Кормится в `loop`, при раздаче
  I2S и **в каждом куске** `handleUpload` WRITE (`ESP.wdtFeed` + `yield`).
  Без кормёжки на upload плата ребутится посреди большого WAV: веб видит
  обрыв, потом ~8–10 с тишины, затем модуль снова в DHCP. Кнопка
  «Перезагрузить» / `POST /api/reboot` специально зависает в `while(true)`,
  чтобы сработал тот же watchdog. `POST /api/reload` только перечитывает
  `/config.json` с карты, WiFi не рвёт.

Формат звука **только** WAV PCM **16-bit, 16 kHz, mono**. Другие файлы
прошивка не играет. Загрузка через веб тоже проверяет заголовок WAV и
отказывается, если частота / битность / каналы другие.

Источник wav: [https://zvukogram.com/category/penie-ptits/](https://zvukogram.com/category/penie-ptits/) — конвертировать
в 16 kHz 16-bit mono перед копированием на карту.

## Железо


| Модуль                                | Назначение           |
| ------------------------------------- | -------------------- |
| Lolin NodeMCU v3 (ESP-12E)            | мозг, WiFi, веб      |
| HC-SR501 (в ТЗ HC-SR510 — тот же PIR) | движение             |
| MAX98357A I2S D-class                 | усилитель на колонки |
| SPI Mini SD reader                    | карта 8 ГБ FAT32     |
| мини-колонки                          | выход MAX98357       |




### Питание

Блок **5 В / 1 А** (зарядка телефона) на пин **VIN** (он же VU на части плат)
и **GND**. Этого хватает:

- сама ESP питается через свой стабилизатор 5 В → 3.3 В;
- HC-SR501 хочет 5 В (ему мало 3.3 В);
- MAX98357 нормально ест 5 В, колонки громче, чем от 3.3 В;
- SD mini-reader тоже **5 В на VCC** (VIN платы). На модуле свой
  стабилизатор: если дать 3.3 В, на саму карту доходит ~2 В и
  `SD.begin` не видит карту. Линии данных при этом остаются 3.3 В,
  на GPIO ESP сажать 5 В не нужно. Проверено на железе: с 3V3
  карта молчала, с VIN — завелась.

1 А на пик MAX98357 вдвоём с WiFi — впритык, но для мини-колонок обычно
хватает. Если усилитель хрипит при громкости 100 — взять БП 5 В 2 А.

**Не** сажать 5 В на пин `3V3`. **Не** кормить ESP только с USB, если
колонки уже висят на VIN: USB часто слабый.

### Схема выводов (принятый вариант)

ESP8266 умеет I2S только на фиксированных GPIO. Их нельзя переставить.

```text
Питание 5V 1A ----+---- VIN (NodeMCU)
                  |
                  +---- VCC HC-SR501
                  |
                  +---- VIN MAX98357
                  |
                  +---- VCC Mini SD

GND блока --------+---- GND NodeMCU ---- GND всех модулей

NodeMCU v3                 HC-SR501
D2  (GPIO4)  -------------- OUT
VIN ---------------------- VCC
GND ---------------------- GND

NodeMCU v3                 MAX98357A
D8  (GPIO15) ------------- BCLK
D4  (GPIO2)  ------------- LRC / WS
RX  (GPIO3)  ------------- DIN
VIN ---------------------- VIN
GND ---------------------- GND
SD  MAX98357 ------------- не подключать
                           (висеть в воздухе = стерео→моно.
                            Если посадить на GND — чип в shutdown,
                            звука не будет)
GAIN --------------------- не подключать (оставить по умолчанию)

Колонки вешать МЕЖДУ + и − выхода MAX98357. Минус выхода — это
не земля: оба контакта дифференциальные (BTL). Ни + ни − на GND
платы не сажать. Наушники с общим GND на «минус» не подойдут:
общий рукав джека часто сидит на земле и коротит половину моста.
Нужна катушка 4–8 Ом (мини-колонки из комплекта).

NodeMCU v3                 Mini SD SPI
D0  (GPIO16) ------------- CS
D7  (GPIO13) ------------- MOSI
D6  (GPIO12) ------------- MISO
D5  (GPIO14) ------------- SCK
VIN ---------------------- VCC   (именно 5 В, не 3V3)
GND ---------------------- GND
```





Микропаузы возможны, если карта медленная. Для 16 kHz mono это обычно
не слышно.

## Карта памяти

- Формат **FAT32**, не exFAT. Размер блока (кластер / единица
  распределения) — **4096 байт (4 КБ)**. Если форматтер на 8 ГБ
  предлагает только 8 КБ — тоже сойдёт. 32 КБ и 64 КБ не ставить.
- В корне: `config.json` (пример лежит рядом с прошивкой).
- Папка со звуками, например `/birds_sounds/*.wav`.
- Папки на живой карте: `/data` (птицы), `/water` (ручей и т.п.).
  Имена через API: только `a-z`, цифры, `-`, `_`, `.`, `/`. Заглавные,
  пробел, кириллица, `..`, `//`, хвост `/` — ошибка 400, в теле правила
  латиницей (`PATH_RULES` в скетче). Корень `/` разрешён **только**
  `GET /api/files`. Остальное (`mkdir`, `upload`, `delete`) корень
  не принимают.
- Новую папку создаёт `POST /api/mkdir` (один сегмент, родитель уже
  должен быть). Загрузка wav папки сама не создаёт. Windows-папку
  `System Volume Information` API не отдаст: в имени пробелы.



### config.json

Поля, которые реально читает прошивка:

```json
{
  "wifi": { "ssid": "...", "password": "..." },
  "ntp": {
    "server": "pool.ntp.org",
    "timezone_offset": 3,
    "update_interval": 3600
  },
  "server": { "port": 80 },
  "motion": {
    "timeout_seconds": 30,
    "cooldown_seconds": 5
  },
  "playback": {
    "schedule": [
      {
        "start": "00:00",
        "end": "06:00",
        "directory": "/birds_sounds",
        "volume": 60,
        "random_on_startup": true,
        "shuffle": false,
        "repeat_selected": true,
        "loop_directory": false
      }
    ]
  }
}
```

Смысл полей расписания:


| Поле                | Смысл                                           |
| ------------------- | ----------------------------------------------- |
| `start` / `end`     | локальное время периода, `24:00` = конец суток  |
| `directory`         | папка на карте                                  |
| `volume`            | 0–100                                           |
| `random_on_startup` | при новом срабатывании PIR взять случайный файл |
| `shuffle`           | следующий трек тоже случайный                   |
| `repeat_selected`   | крутить один выбранный файл, пока есть движение |
| `loop_directory`    | если файлы кончились — начать папку сначала     |


`motion.timeout_seconds` — тишина после последнего HIGH с PIR.
`motion.cooldown_seconds` — пауза, чтобы датчик не дёргал play каждый
миллисекунд.

Часовой пояс — число часов, не строка `Europe/Moscow`: ESP8266 так проще.
Для Москвы `3`, для зимнего перехода не заморачиваемся.

Старый `config.yaml` — исходник для людей. На карту кладётся **json**.
Пересобрать json из yaml:

```bash
yq -o json config.yaml > config.json
```

В yaml нет `motion` и `timezone_offset` — в json они уже есть. Править
лучше json, yaml можно потом подтянуть руками.

## Веб и API

После DHCP смотри IP в Serial **до** первого play (115200 бод), либо в
списке клиентов роутера.


| Адрес                                         | Что                                |
| --------------------------------------------- | ---------------------------------- |
| `http://IP/`                                  | настройки, громкость, загрузка wav |
| `http://IP/swagger/index.html`                | описание API                       |
| `GET /api/status`                             | время, играет ли, файл, IP         |
| `POST /api/play`                              | включить                           |
| `POST /api/stop`                              | выключить                          |
| `POST /api/volume?value=70`                   | громкость сейчас (в файл не пишет) |
| `GET /api/config`                             | текущий конфиг                     |
| `POST /api/config`                            | тело JSON, пишет `/config.json`    |
| `POST /api/reload`                            | перечитать `/config.json`, WiFi не рвать |
| `POST /api/reboot`                            | ответ ok, затем зависание → watchdog reset |
| `GET /api/files?path=/`                       | одна папка, не рекурсивно: `{path, entries:[{name,type,size?}]}` |
| `POST /api/mkdir?path=/birds_sounds`          | создать один каталог, родитель уже есть |
| `POST /api/upload?path=/data/a.wav`           | multipart поле `file` = сырой WAV; папку не создаёт |
| `DELETE /api/delete?path=/data/a.wav`         | удалить файл (не config.json) |


Как узнать, что на :80 именно этот модуль, а не чужой веб: `GET /api/status`
обязан вернуть JSON со всеми полями `playing`, `sd_ok`, `volume`, `ip`,
`motion`, `file`, `directory` (плюс time, time_ok, wifi_sta). Так делает
control plane. Не опираться на HTML title.

Upload — не «сырой POST body», а обработчик ESP8266WebServer
`server.on("/api/upload", POST, done, handleUpload)`: браузер шлёт
`FormData` с полем `file`. Первые 44 байта файла проверяются как RIFF/WAVE
PCM 16 kHz 16-bit mono. Иначе 400.

`GET /api/files` не дерево: один уровень. Control plane сам обходит папки.


Смена WiFi через веб применяется в памяти, но **переподключение к новой
сети — после перезагрузки** платы (кнопка RST или `POST /api/reboot`).
Так проще и нет обрыва в середине сохранения.

`POST /api/reload` только перечитывает карту. Watchdog кормится в `loop`,
при I2S и при upload WRITE. Если прошивка залипнет — плата сама
перезапустится примерно через 8 с. Кнопка «Перезагрузить» специально
зависает, чтобы сработал тот же watchdog.

Загрузка чужого wav: если не 16-bit / не 16 kHz / не mono — ответ 400
с текстом вроде:

`Oshibka: nuzhen WAV 16-bit, 16kHz, mono. Tekushchiy fayl: 16-bit, 44100Hz, 2 kanalov`

## Библиотеки и сборка

Ядро: `esp8266:esp8266:nodemcuv2` (NodeMCU 1.0 / v3).

Нужны библиотеки:

- `ArduinoJson` версии **6.x** (Benoit Blanchon) — разбор config.json.
Не ставить 7.x: синтаксис другой, скетч не соберётся.
- `SD`, `SPI`, `ESP8266WiFi`, `ESP8266WebServer` — входят в ядро;
- I2S — заголовки `i2s.h` / `i2s_reg.h` из ядра ESP8266.

Установка (как в соседнем проекте):

```bash
arduino-cli config init
arduino-cli config add board_manager.additional_urls \
  https://arduino.esp8266.com/stable/package_esp8266com_index.json
arduino-cli core update-index
arduino-cli core install esp8266:esp8266
arduino-cli lib install ArduinoJson
```

Собрать:

```bash
cd /home/blobby/work_dir/vibecode-arduino/lolin-wc-sounds
cp .env.primer .env    # если ещё нет: REZHIM=wsl, PORT=COM4
./sborka.sh            # только собрать
./sborka.sh zalit      # собрать и залить через виндовый arduino-cli.exe
```

Пароль WiFi **не** зашивается компилятором (в отличие от lolin-web-pulse).
Он лежит на карте в `config.json`. Карту можно править с компьютера.

Из WSL COM-порт платы виден Windows, не Linux. Скрипт копирует `.ino` на
диск C (`C:\Users\<user>\arduino-wsl\lolin-wc-sounds`) и зовёт
`arduino-cli.exe`. На этой машине заливка шла на **COM4** (ещё бывают
COM10/COM11 — не путать). После upload плата ресетится RTS и снова
берёт DHCP; IP часто тот же.

## Конвертация wav

ffmpeg:

```bash
ffmpeg -i ptitsy.mp3 -ar 16000 -ac 1 -sample_fmt s16 bird01.wav
```

Потом файл в папку на карте (`/data`, `/water`, …) или через `/api/upload`.
Control plane делает ту же команду, если выбран mp3/ogg/чужой wav.

## Control plane

Папка `../lolin-wc-sounds--control-plane/`. Не дублировать веб модуля —
там прокси и ffmpeg. Меняя поля статуса или правила путей, править оба
readme и `WC_STATUS_KEYS` в control plane, иначе скан «не видит» плату.

## Что можно доработать позже

- Локальный NTP вместо `pool.ntp.org` (поле `ntp.server` уже есть).
- MP3: на ESP8266 тяжело, в конфиге поле `format` нарочно не используем.
- Статический IP (`server.host` из старого yaml пока игнорируется, DHCP).
- Отдельный UART-лог, если RX занят I2S слишком сильно.
- Светодиод «играет / ошибка SD».



## Файлы в папке


| Файл                  | Зачем                         |
| --------------------- | ----------------------------- |
| `description`         | исходное ТЗ                   |
| `lolin-wc-sounds.ino` | прошивка                      |
| `config.json`         | пример на SD                  |
| `config.yaml`         | человеческий черновик конфига |
| `.env` / `.env.primer`| PORT и REZHIM для sborka.sh   |
| `sborka.sh`           | compile / upload              |
| `readme.md`           | этот текст                    |


