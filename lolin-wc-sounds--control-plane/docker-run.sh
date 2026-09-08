#!/usr/bin/env bash
# Сборка и запуск control plane в Docker.
#   ./docker-run.sh

set -euo pipefail
cd "$(dirname "$0")"

DOCKER="sudo docker"
whoami | grep -q root && DOCKER="docker"

COMPOSE_FILE="$PWD/docker-compose/compose.yaml"
PROJECT="lolin-wc-sounds-cp"

echo "$DOCKER build --network=host -t lolin-wc-sounds-cp-base:latest -f Dockerfile.base ."
$DOCKER build --network=host -t lolin-wc-sounds-cp-base:latest -f Dockerfile.base .

echo "$DOCKER build --network=host -t lolin-wc-sounds-cp:latest -f Dockerfile ."
$DOCKER build --network=host -t lolin-wc-sounds-cp:latest -f Dockerfile .

echo "$DOCKER compose -p $PROJECT -f $COMPOSE_FILE up -d --no-build"
$DOCKER compose -p "$PROJECT" -f "$COMPOSE_FILE" up -d --no-build

$DOCKER compose -p "$PROJECT" -f "$COMPOSE_FILE" ps
