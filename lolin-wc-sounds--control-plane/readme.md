# lolin-wc-sounds — control plane

Веб-оболочка для модулей **lolin-wc-sounds** (ESP8266 в туалете / коридоре).
Сама плата играет WAV с SD по датчику движения; это приложение ищет платы
в LAN и даёт тот же API, что у модуля, в тёмном интерфейсе.

Прошивка и железо: соседняя папка `../lolin-wc-sounds/` — читать её
`readme.md` раньше, чем этот файл. Здесь только control plane.

Исходное ТЗ лежит в файле `task`.

## Зачем

ESP8266 раздаёт убогий HTML на порту 80. Несколько модулей в сети,
кириллические имена файлов, чужой mp3 вместо WAV 16 kHz mono — с телефона
у плиты это неудобно. Control plane:

1. Сканирует подсеть из `config.yaml` по порту 80.
2. Узнаёт «это наш модуль» по JSON `GET /api/status`.
3. Проксирует play / stop / volume / config / files / mkdir / upload / delete / reload / reboot.
4. Перед загрузкой анализирует файл и при необходимости гоняет ffmpeg.

## Архитектура

```text
браузер  -->  FastAPI :8000  -->  http://МОДУЛЬ:80/api/...
                 |                    ESP8266 + SD
                 +-- config.yaml
                 +-- ffmpeg / ffprobe (локально)
```

- Python 3.10, пакеты: FastAPI, uvicorn, httpx, PyYAML, python-multipart.
- Статика без сборки: `static/index.html`, `app.css`, `app.js`.
- Логика без UI: `wc_control/` (настройки, скан, WAV, транслит).
- Тесты: `pytest tests/` из этой папки.

Принципы:

- Control plane **не хранит** WiFi-пароли модулей и не шьёт прошивку.
  Пароли живут в `/config.json` на SD платы.
- Проксировать можно только IP из `scan.subnet` (плюс `scan.hosts` и
  сохранённый список `modules`). Чужой адрес — 403.
- Модуль узнаётся **не по title HTML**, а по набору полей статуса:
  `playing`, `sd_ok`, `volume`, `ip`, `motion`, `file`, `directory`.
  Случайный http на :80 так не пройдёт.
- Имена на карте — только латиница. Кириллицу в UI тупо транслитерируем
  (`журчание ручья в лесу.mp3` → `zhurchanie-ruchya-v-lesu.wav`).
- Формат модуля неприкосновенен: WAV PCM **16-bit, 16 kHz, mono**.
  ffmpeg: `-ar 16000 -ac 1 -c:a pcm_s16le`.

## Запуск

Нужны системные `ffmpeg` и `ffprobe`. Зависимости Python:

```bash
python3 -m pip install --user -r requirements.txt
cd /home/blobby/work_dir/vibecode-arduino/lolin-wc-sounds--control-plane
./run.sh
```

В консоли печатается кликабельная ссылка:

```text
http://localhost:8000
```

Слушает `listen.host` / `listen.port` из yaml (по умолчанию 127.0.0.1:8000).
После правок Python процесс надо перезапустить: uvicorn без `--reload`.
JS/CSS подхватываются с диска после обновления страницы.

Тесты:

```bash
python3 -m pytest tests/ -q
```

## config.yaml

Лежит **в корне этой папки**, не на SD модуля.

```yaml
listen:
  host: 127.0.0.1
  port: 8000
scan:
  subnet: 192.168.88.0/24   # пул для полного скана
  port: 80
  timeout_seconds: 0.45
  concurrency: 80
  hosts: []                 # лишние IP вне подсети, если надо
modules:                    # результат последнего полного скана
  - host: 192.168.88.28
    port: 80
```

- Полный скан («Сканировать сеть») опрашивает все адреса подсети и
  **перезаписывает** блок `modules`, комментарии listen/scan сохраняет.
- При старте UI сначала `GET /api/known`: только сохранённые IP.
  Живой — оранжевая лампочка, молчит — серая. Полный скан не обязателен.
- Текущая домашняя сеть — `192.168.88.0/24`. Сменилась — правьте subnet.

## Как устроен UI

Слева карточки модулей. Клик по **онлайн** открывает панель. Офлайн
не открывается.

Настройки модуля = поля `GET/POST /api/config` (wifi, ntp, motion,
расписание кусками, не сырым JSON). Play/stop/volume/reload/reboot —
как у платы. Volume «применить сейчас» не пишет config.

Файлы: дерево от `/`, справа содержимое выбранной папки. «Обновить»
заново читает карту и возвращает на `/`. Прошивка `GET /api/files`
**не рекурсивная** — дерево собирает браузер, ходя по папкам.
Папки с пробелом в имени (`System Volume Information`) API отвергает —
в дереве их пропускаем.

Загрузка:

1. Выбор файла → `POST /api/analyze`.
2. Не WAV 16/16/mono → кнопка ffmpeg, `POST /api/convert`.
3. Успех: в `<input type=file>` подставляется уже WAV (DataTransfer),
   путь на карте — транслит, папка = выбранная в дереве.
4. «Загрузить на модуль» → `POST /api/modules/{ip}/upload?path=...`
   multipart поле `file` (так ждёт ESP8266WebServer).

HTTP-заголовок `Content-Disposition` **нельзя** заполнять кириллицей
(latin-1) — из-за этого convert отдавал 500, хотя ffmpeg уже отработал.
В заголовке всегда транслит.

## API самого control plane

| Метод | Путь | Зачем |
| ----- | ---- | ----- |
| GET | `/` | UI |
| GET | `/api/meta` | подсеть, число адресов |
| GET | `/api/known` | проверка модулей из yaml |
| POST | `/api/scan` | полный скан + запись yaml |
| GET/POST | `/api/modules/{ip}/...` | прокси API платы |
| POST | `/api/analyze` | разобрать аудио |
| POST | `/api/convert` | ffmpeg → WAV модуля |

Таймаут к плате: 15 с на обычные запросы, **180 с** на upload
(SD на ESP8266 медленная).

## Контракт с прошивкой (не ломать)

Идентификация: `GET http://IP/api/status` → JSON со всеми ключами из
`wc_control/scanner.py` (`WC_STATUS_KEYS`).

Пути на SD: только `a-z 0-9 - _ . /`, от корня, без хвоста `/`,
без `..` и `//`. Корень `/` можно только у `GET /api/files`.
`POST /api/mkdir` создаёт **один** последний сегмент, родитель уже есть.
Upload папку сам не создаёт.

Upload: `POST /api/upload?path=/water/a.wav`, тело — multipart `file`,
содержимое — WAV PCM 16 kHz 16-bit mono. Прошивка смотрит первые 44 байта
как RIFF. Иначе 400 с транслитом ошибки латиницей.

Watchdog платы ~8 с. Старые прошивки во время длинного upload не кормили
WDT и уходили в reboot (у control plane это выглядело как 502, потом
статус тоже 502, через ~10 с модуль оживал). В прошивке от 2026-09-05
в `handleUpload` WRITE есть `ESP.wdtFeed()` + `yield()`. Без этой
прошивки большие wav лучше не лить по вебу.

Смена WiFi в config применяется сразу в RAM, **к новой сети плата
подключится только после reboot**.

## Живой экземпляр (на момент записи)

- Модуль: `192.168.88.28`, порт 80.
- USB-заливка из WSL: `REZHIM=wsl`, `PORT=COM4` в `../lolin-wc-sounds/.env`.
- MAC с последней заливки: `48:3f:da:62:c6:e3`.
- На карте есть `/data` (птицы) и `/water`.

## Файлы


| Путь | Зачем |
| ---- | ----- |
| `task` | исходное ТЗ |
| `config.yaml` | listen, скан, найденные модули |
| `requirements.txt` | pip |
| `wc_control/` | сервер и логика |
| `static/` | UI |
| `tests/` | pytest |
| `run.sh` | запуск: `./run.sh` |
| `run.py` | то же, если удобнее python |

Не класть сюда `.venv` и `__pycache__` в git, если заведёте репозиторий.
Паролей в yaml нет и не должно быть.
