#!/bin/bash
#
# Сборка и заливка nano-pulse-light.
#
# Как пользоваться:
#   ./sborka.sh           - только собрать
#   ./sborka.sh zalit     - собрать и залить в плату
#
# Настройки лежат в файле .env рядом со скриптом.
# Пример настроек - в файле .env.primer

set -e

cd "$(dirname "$0")"

# Дешёвые Nano с CH340 чаще всего со старым загрузчиком.
# Если заливка не идёт (avrdude timeout) - в .env поставь:
#   PLATA=arduino:avr:nano:cpu=atmega328
PLATA_PO_UMOLCHANIYU="arduino:avr:nano:cpu=atmega328old"

if [ -f .env ]; then
    set -a
    source .env
    set +a
fi

if [ -z "$PLATA" ]; then
    PLATA="$PLATA_PO_UMOLCHANIYU"
fi

if [ -z "$REZHIM" ]; then
    REZHIM="wsl"
fi

echo "Plata: $PLATA"
echo "Rezhim: $REZHIM"
echo

if [ "$REZHIM" = "wsl" ]; then
    if [ -z "$WINDOWS_CLI" ]; then
        WINDOWS_CLI="/mnt/c/Program Files/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe"
    fi

    if [ -z "$WINDOWS_PAPKA" ]; then
        WINDOWS_PAPKA="/mnt/c/Users/$USER/arduino-wsl/nano-pulse-light"
    fi

    if [ -z "$WINDOWS_PUT" ]; then
        WINDOWS_PUT="C:\\Users\\$USER\\arduino-wsl\\nano-pulse-light"
    fi

    echo "Kopiruyu sketch na disk C..."
    mkdir -p "$WINDOWS_PAPKA"
    cp nano-pulse-light.ino "$WINDOWS_PAPKA/"
    cp config.h "$WINDOWS_PAPKA/"

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
