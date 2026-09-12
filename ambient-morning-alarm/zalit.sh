#!/bin/bash
#
# Сборка и заливка ambient-morning-alarm на ESP32-C3 Super Mini.
#
#   ./zalit.sh          собрать и залить
#   ./zalit.sh uzhe     залить уже собранный bin (без компиляции)
#
# PORT из .env: COM5 — через Windows esptool (WSL USB CDC так не шьётся).
# Serial Monitor должен быть закрыт, иначе PermissionError.

set -euo pipefail

cd "$(dirname "$0")"

PLATA_PO_UMOLCHANIYU="esp32:esp32:esp32c3:PartitionScheme=min_spiffs"
BUILD_DIR="${BUILD_DIR:-/tmp/ama-build}"
BIN_NAME="ambient-morning-alarm.ino.merged.bin"

if [ ! -f .env ]; then
    echo "OSHIBKA: net faila .env"
    echo "    cp .env.primer .env"
    exit 1
fi

set -a
# shellcheck disable=SC1091
source .env
set +a

if [ -z "${PLATA:-}" ]; then
    PLATA="$PLATA_PO_UMOLCHANIYU"
fi

if [ -z "${WIFI_SSID:-}" ] || [ -z "${WIFI_PASS:-}" ]; then
    echo "OSHIBKA: v .env nuzhny WIFI_SSID i WIFI_PASS"
    exit 1
fi

if [ -z "${PORT:-}" ]; then
    echo "OSHIBKA: v .env ne zapolnen PORT (naprimer COM5)"
    exit 1
fi

NASTROYKI_KOMPILYATORA="compiler.cpp.extra_flags=-DWIFI_SSID=\"$WIFI_SSID\" -DWIFI_PASS=\"$WIFI_PASS\""

sobrat() {
    echo "Sobirayu ($PLATA)..."
    mkdir -p "$BUILD_DIR"
    arduino-cli compile --fqbn "$PLATA" \
        --build-property "$NASTROYKI_KOMPILYATORA" \
        --output-dir "$BUILD_DIR" \
        .
}

zalit_windows_com() {
    local win_user="${WINDOWS_USER:-$USER}"
    local win_dir="/mnt/c/Users/${win_user}/arduino-wsl/ambient-morning-alarm/out"
    local win_put="C:\\\\Users\\\\${win_user}\\\\arduino-wsl\\\\ambient-morning-alarm\\\\out\\\\${BIN_NAME}"

    mkdir -p "$win_dir"
    cp "$BUILD_DIR/$BIN_NAME" "$win_dir/"
    echo "Kopiruyu $BIN_NAME -> $win_dir"
    echo "Zalivayu esptool -> $PORT (Serial Monitor zakroy)..."

    if ! powershell.exe -NoProfile -Command \
        "python -m esptool --chip esp32c3 --port $PORT --baud 460800 --before default-reset --after hard-reset write-flash -z --flash-mode dio --flash-freq 80m --flash-size 4MB 0x0 $win_put"; then
        echo
        echo "OSHIBKA zalivki. Chasto COM zanyat Serial Monitor / PermissionError."
        echo "Zakroy monitor porta, vyderni-vstav USB, povtori ./zalit.sh uzhe"
        exit 1
    fi
}

zalit_linux() {
    echo "Zalivayu arduino-cli -> $PORT..."
    arduino-cli upload -p "$PORT" --fqbn "$PLATA" --input-dir "$BUILD_DIR" .
}

if [ "${1:-}" != "uzhe" ]; then
    sobrat
else
    if [ ! -f "$BUILD_DIR/$BIN_NAME" ]; then
        echo "OSHIBKA: net $BUILD_DIR/$BIN_NAME — snachala ./zalit.sh bez uzhe"
        exit 1
    fi
    echo "Beru uzhe sobrannyy bin: $BUILD_DIR/$BIN_NAME"
fi

case "$PORT" in
    COM*|com*)
        zalit_windows_com
        ;;
    *)
        zalit_linux
        ;;
esac

echo
echo "Gotovo. PORT=$PORT"
