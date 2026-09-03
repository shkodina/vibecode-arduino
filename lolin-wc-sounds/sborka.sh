#!/bin/bash
#
# Сборка прошивки lolin-wc-sounds.
# WiFi пароль НЕ зашивается здесь: он лежит в config.json на SD-карте.
#
#   ./sborka.sh         - только собрать
#   ./sborka.sh zalit   - собрать и залить
#
# Настройки порта - в файле .env (см. .env.primer).

set -e
cd "$(dirname "$0")"

PLATA="esp8266:esp8266:nodemcuv2"
SKETCH="lolin-wc-sounds.ino"

if [ ! -f .env ]; then
    echo "OSHIBKA: net faila .env"
    echo "Skopiruy primer:"
    echo "    cp .env.primer .env"
    exit 1
fi

set -a
source .env
set +a

if [ -z "$REZHIM" ]; then
    REZHIM="linux"
fi

echo "Plata: $PLATA"
echo "Rezhim: $REZHIM"
echo

if [ "$REZHIM" = "wsl" ]; then
    if [ -z "$WINDOWS_CLI" ]; then
        WINDOWS_CLI="/mnt/c/Program Files/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe"
    fi
    if [ -z "$WINDOWS_PAPKA" ]; then
        WINDOWS_PAPKA="/mnt/c/Users/$USER/arduino-wsl/lolin-wc-sounds"
    fi
    if [ -z "$WINDOWS_PUT" ]; then
        WINDOWS_PUT="C:\\Users\\$USER\\arduino-wsl\\lolin-wc-sounds"
    fi

    echo "Kopiruyu sketch na disk C..."
    mkdir -p "$WINDOWS_PAPKA"
    cp "$SKETCH" "$WINDOWS_PAPKA/"

    echo "Sobirayu..."
    cd /mnt/c
    "$WINDOWS_CLI" compile --fqbn "$PLATA" "$WINDOWS_PUT"

    if [ "$1" = "zalit" ]; then
        if [ -z "$PORT" ]; then
            echo "OSHIBKA: v .env ne zapolnen PORT (naprimer COM4)"
            exit 1
        fi
        echo "Zalivayu v platu na port $PORT..."
        "$WINDOWS_CLI" upload -p "$PORT" --fqbn "$PLATA" "$WINDOWS_PUT"
    fi
else
    echo "Sobirayu..."
    arduino-cli compile --fqbn "$PLATA" .

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
