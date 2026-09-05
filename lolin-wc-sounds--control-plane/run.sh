#!/bin/bash
# Запуск control plane. Из любой папки:
#   ./run.sh
#   /home/blobby/work_dir/vibecode-arduino/lolin-wc-sounds--control-plane/run.sh

set -e
cd "$(dirname "$0")"
exec python3 -m wc_control
