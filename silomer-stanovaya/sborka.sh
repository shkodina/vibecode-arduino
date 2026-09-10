#!/bin/bash
#
# Сборка и заливка прошивки силомера.
#
# Как пользоваться:
#   ./sborka.sh           - только собрать
#   ./sborka.sh zalit     - собрать и залить в плату
#
# WiFi-настройки берутся из .env рядом со скриптом и в исходники не попадают.

set -e

cd "$(dirname "$0")"

PLATA_PO_UMOLCHANIYU="esp8266:esp8266:nodemcuv2"

if [ ! -f .env ]; then
    echo "OSHIBKA: net faila .env"
    echo "Skopiruy primer i vpishi svoi dannye:"
    echo "    cp .env.primer .env"
    exit 1
fi

set -a
source .env
set +a

if [ -z "$PLATA" ]; then
    PLATA="$PLATA_PO_UMOLCHANIYU"
fi

if [ -z "$REZHIM" ]; then
    REZHIM="wsl"
fi

if [ -z "$WIFI_SSID" ]; then
    echo "OSHIBKA: v .env ne zapolnen WIFI_SSID"
    exit 1
fi

if [ -z "$WIFI_PASS" ]; then
    echo "OSHIBKA: v .env ne zapolnen WIFI_PASS"
    exit 1
fi

NASTROYKI_KOMPILYATORA="compiler.cpp.extra_flags=-DWIFI_SSID=\"$WIFI_SSID\" -DWIFI_PASS=\"$WIFI_PASS\""

echo "Set: $WIFI_SSID"
echo "Parol: (skryt, ${#WIFI_PASS} simvolov)"
echo "Plata: $PLATA"
echo "Rezhim: $REZHIM"
echo

if [ "$REZHIM" = "wsl" ]; then
    if [ -z "$WINDOWS_CLI" ]; then
        WINDOWS_CLI="/mnt/c/Program Files/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe"
    fi

    if [ -z "$WINDOWS_PAPKA" ]; then
        WINDOWS_PAPKA="/mnt/c/Users/$USER/arduino-wsl/silomer-stanovaya"
    fi

    if [ -z "$WINDOWS_PUT" ]; then
        WINDOWS_PUT="C:\\Users\\$USER\\arduino-wsl\\silomer-stanovaya"
    fi

    echo "Kopiruyu sketch na disk C..."
    mkdir -p "$WINDOWS_PAPKA"
    cp silomer-stanovaya.ino "$WINDOWS_PAPKA/"
    cp config.h "$WINDOWS_PAPKA/"

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
