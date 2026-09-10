# Task 04: build scripts and project README

> **For agentic workers:** можно выполнять в отдельной сессии после появления
> базовых файлов прошивки или параллельно, если имена файлов уже согласованы.

## Цель

Сделать сборку и заливку прошивки через bash-скрипт, хранить WiFi-секреты в
`.env`, подготовить `.env.primer`, обновить `.gitignore` и написать `readme.md`
так, чтобы в следующий раз проект можно было продолжить без повторного
исследования.

## Ожидаемые файлы

- Создать: `ambient-morning-alarm/sborka.sh`
- Создать: `ambient-morning-alarm/.env.primer`
- Создать или изменить: `ambient-morning-alarm/.gitignore`
- Создать: `ambient-morning-alarm/readme.md`
- Проверить наличие: `ambient-morning-alarm/ambient-morning-alarm.ino`
- Проверить наличие: `ambient-morning-alarm/config.h`

## Соседние образцы

- `../silomer-stanovaya/sborka.sh`: лучший образец для WiFi через
  `compiler.cpp.extra_flags`.
- `../lolin-wc-sounds/sborka.sh`: образец WSL-копирования sketch в Windows
  директорию.
- `../silomer-stanovaya/readme.md`: стиль подробного README со схемой,
  режимами и командами.

## Arduino CLI настройки

Плата по умолчанию:

```bash
PLATA_PO_UMOLCHANIYU="esp32:esp32:esp32c3"
```

Если конкретная Tenstar Robot ESP32-C3 Super Mini требует другой FQBN,
переопределять его в `.env` через `PLATA=...`.

Перед первой сборкой на Linux/WSL нужны команды:

```bash
arduino-cli config init
arduino-cli config add board_manager.additional_urls \
  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32
```

## `.env.primer`

Файл должен содержать:

```bash
# Скопируй в .env и впиши свои значения.
# .env не должен попадать в git.

WIFI_SSID=MoyaWiFiSet
WIFI_PASS=moy-skrytyy-parol

# Для WSL обычно COM-порт Windows, например COM4.
# Для linux-режима обычно /dev/ttyACM0 или /dev/ttyUSB0.
PORT=COM4

# wsl или linux
REZHIM=wsl

# Обычно менять не нужно.
# PLATA=esp32:esp32:esp32c3
# WINDOWS_CLI=/mnt/c/Program Files/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe
# WINDOWS_PAPKA=/mnt/c/Users/USERNAME/arduino-wsl/ambient-morning-alarm
# WINDOWS_PUT=C:\Users\USERNAME\arduino-wsl\ambient-morning-alarm
```

## `sborka.sh`

Скрипт должен:

1. `cd "$(dirname "$0")"`.
2. Проверить наличие `.env`.
3. Загрузить `.env` через `set -a; source .env; set +a`.
4. Проверить `WIFI_SSID` и `WIFI_PASS`.
5. Сформировать:

```bash
NASTROYKI_KOMPILYATORA="compiler.cpp.extra_flags=-DWIFI_SSID=\"$WIFI_SSID\" -DWIFI_PASS=\"$WIFI_PASS\""
```

6. Для `REZHIM=wsl`:
   - скопировать `ambient-morning-alarm.ino`, `config.h` и все локальные
     `.h`/`.cpp` файлы в Windows-папку;
   - вызвать Windows `arduino-cli.exe compile`;
   - при аргументе `zalit` вызвать `upload -p "$PORT"`.
7. Для `REZHIM=linux`:
   - вызвать `arduino-cli compile --fqbn "$PLATA" --build-property "$NASTROYKI_KOMPILYATORA" .`;
   - при аргументе `zalit` вызвать upload.
8. Не печатать пароль, только длину строки.

## `.gitignore`

Минимум:

```gitignore
.env
*.bin
*.elf
*.map
build/
```

Если в корне репозитория уже есть `.gitignore`, не удалять его правила.

## README структура

`readme.md` должен содержать:

1. Название и короткую цель.
2. Список железа.
3. Схему подключения из `concept-plan.md`.
4. Объяснение общей земли и роли IRF520.
5. Что делает прошивка после старта.
6. Режимы свечения `ramp` и `pulse`.
7. Веб-интерфейс и URL.
8. HTTP API с маршрутами.
9. Bluetooth имя, PIN и BLE service.
10. Формат версии `00.00.001`.
11. Установка Arduino CLI и ESP32 core.
12. Создание `.env` из `.env.primer`.
13. Команды сборки и заливки.
14. Что проверять после заливки.
15. Список дальнейших доработок.

## Критерии готовности

- `./sborka.sh` собирает прошивку без ручной передачи WiFi-флагов.
- `./sborka.sh zalit` заливает прошивку на порт из `.env`.
- При отсутствии `.env` скрипт показывает понятную инструкцию.
- При отсутствии `WIFI_SSID` или `WIFI_PASS` скрипт завершает работу с ошибкой.
- `.env` игнорируется.
- README позволяет понять проект без чтения исходного prompt.

## Проверка

```bash
cd ambient-morning-alarm
cp .env.primer .env
# вписать реальные WIFI_SSID, WIFI_PASS и PORT
./sborka.sh
./sborka.sh zalit
```

Дополнительно проверить, что секреты не попали в исходники:

```bash
grep -R "moy-skrytyy-parol\\|реальный-пароль" .
```

Ожидаемо пароль должен встречаться только в `.env`, который не попадает в git.

