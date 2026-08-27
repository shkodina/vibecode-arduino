#!/bin/bash
#
# Сборка и заливка прошивки. Пароль от wifi берётся из файла .env,
# в код он не попадает, поэтому .env лежит в .gitignore.
#
# Как пользоваться:
#   ./sborka.sh           - только собрать
#   ./sborka.sh zalit     - собрать и залить в плату
#
# Настройки лежат в файле .env рядом со скриптом.
# Пример настроек - в файле .env.primer

# Останавливаемся при любой ошибке, чтобы не заливать битую прошивку.
set -e

# Идём в папку, где лежит сам скрипт. Так его можно запускать откуда угодно.
cd "$(dirname "$0")"

PLATA="esp8266:esp8266:nodemcuv2"

# --- шаг 1: читаем настройки ---

if [ ! -f .env ]; then
    echo "OSHIBKA: net faila .env"
    echo "Skopiruy primer i vpishi svoi dannye:"
    echo "    cp .env.primer .env"
    exit 1
fi

# Загружаем переменные из .env в окружение.
set -a
source .env
set +a

# --- шаг 2: проверяем, что всё заполнено ---

if [ -z "$WIFI_SSID" ]; then
    echo "OSHIBKA: v .env ne zapolnen WIFI_SSID"
    exit 1
fi

if [ -z "$WIFI_PASS" ]; then
    echo "OSHIBKA: v .env ne zapolnen WIFI_PASS"
    exit 1
fi

# Режим по умолчанию - обычный линукс.
if [ -z "$REZHIM" ]; then
    REZHIM="linux"
fi

# --- шаг 3: собираем строку с настройками для компилятора ---
# Кавычки вокруг значений нужны, потому что в коде это строки.
# Экранирование выглядит страшно, но это как раз то, ради чего
# и сделан этот скрипт: руками такое каждый раз набирать неудобно.
NASTROYKI_KOMPILYATORA="compiler.cpp.extra_flags=-DWIFI_SSID=\"$WIFI_SSID\" -DWIFI_PASS=\"$WIFI_PASS\""

echo "Set: $WIFI_SSID"
echo "Parol: (skryt, ${#WIFI_PASS} simvolov)"
echo "Plata: $PLATA"
echo "Rezhim: $REZHIM"
echo

# --- шаг 4: сборка и заливка ---

if [ "$REZHIM" = "wsl" ]; then
    # В WSL порт не виден, поэтому работаем виндовым arduino-cli.
    # Скетч надо положить на диск C: с сетевыми путями виндовые
    # программы работают плохо.

    if [ -z "$WINDOWS_CLI" ]; then
        WINDOWS_CLI="/mnt/c/Program Files/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe"
    fi

    if [ -z "$WINDOWS_PAPKA" ]; then
        WINDOWS_PAPKA="/mnt/c/Users/$USER/arduino-wsl/lolin-web-pulse"
    fi

    # Виндовый путь к той же папке, он нужен самому arduino-cli.exe
    if [ -z "$WINDOWS_PUT" ]; then
        WINDOWS_PUT="C:\\Users\\$USER\\arduino-wsl\\lolin-web-pulse"
    fi

    echo "Kopiruyu sketch na disk C..."
    mkdir -p "$WINDOWS_PAPKA"
    cp lolin-web-pulse.ino "$WINDOWS_PAPKA/"

    echo "Sobirayu..."
    cd /mnt/c
    "$WINDOWS_CLI" compile --fqbn "$PLATA" \
        --build-property "$NASTROYKI_KOMPILYATORA" \
        "$WINDOWS_PUT"

    if [ "$1" = "zalit" ]; then
        if [ -z "$PORT" ]; then
            echo "OSHIBKA: v .env ne zapolnen PORT (naprimer COM4)"
            exit 1
        fi

        echo "Zalivayu v platu na port $PORT..."
        "$WINDOWS_CLI" upload -p "$PORT" --fqbn "$PLATA" "$WINDOWS_PUT"
    fi

else
    # Обычный режим: линуксовый arduino-cli, порт вида /dev/ttyUSB0
    echo "Sobirayu..."
    arduino-cli compile --fqbn "$PLATA" \
        --build-property "$NASTROYKI_KOMPILYATORA" \
        .

    if [ "$1" = "zalit" ]; then
        if [ -z "$PORT" ]; then
            echo "OSHIBKA: v .env ne zapolnen PORT (naprimer /dev/ttyUSB0)"
            exit 1
        fi

        echo "Zalivayu v platu na port $PORT..."
        arduino-cli upload -p "$PORT" --fqbn "$PLATA" .
    fi
fi

echo
echo "Gotovo."
