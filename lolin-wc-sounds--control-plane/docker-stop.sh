#!/usr/bin/env bash
# Останов Docker-решения control plane.
#   ./docker-stop.sh

set -euo pipefail
cd "$(dirname "$0")"

DOCKER="sudo docker"
whoami | grep -q root && DOCKER="docker"

COMPOSE_FILE="$PWD/docker-compose/compose.yaml"
PROJECT="lolin-wc-sounds-cp"

echo "$DOCKER compose -p $PROJECT -f $COMPOSE_FILE down"
$DOCKER compose -p "$PROJECT" -f "$COMPOSE_FILE" down
