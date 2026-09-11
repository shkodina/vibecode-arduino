#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

IMAGE_NAME="ambient-morning-alarm-apk-builder"
CONTAINER_NAME="ambient-morning-alarm-apk-build"
OUT_DIR="out"
KEYSTORE_DIR="keystore"
KEYSTORE_FILE="${KEYSTORE_DIR}/piper-light-alarm.jks"
ALIAS="piper-light-alarm"
STORE_PASS="piper-light-alarm"
KEY_PASS="piper-light-alarm"

if command -v docker >/dev/null 2>&1; then
  DOCKER=(docker)
elif command -v podman >/dev/null 2>&1; then
  DOCKER=(podman)
else
  echo "Docker/Podman не найден. Установи Docker на Ubuntu 22.04 и повтори." >&2
  exit 1
fi

cleanup() {
  "${DOCKER[@]}" rm -f "${CONTAINER_NAME}" >/dev/null 2>&1 || true
}
trap cleanup EXIT

mkdir -p "${OUT_DIR}" "${KEYSTORE_DIR}"

echo "==> Building image ${IMAGE_NAME}"
"${DOCKER[@]}" build -t "${IMAGE_NAME}" .

cleanup
echo "==> Starting container ${CONTAINER_NAME}"
"${DOCKER[@]}" run -d --name "${CONTAINER_NAME}" \
  -v "$PWD":/work \
  -w /work \
  "${IMAGE_NAME}" \
  sleep infinity

echo "==> npm ci"
"${DOCKER[@]}" exec "${CONTAINER_NAME}" bash -lc 'npm ci'

echo "==> expo prebuild (android)"
"${DOCKER[@]}" exec "${CONTAINER_NAME}" bash -lc \
  'npx expo prebuild --platform android --non-interactive --clean'

if [[ ! -f "${KEYSTORE_FILE}" ]]; then
  echo "==> Generating keystore ${KEYSTORE_FILE}"
  "${DOCKER[@]}" exec "${CONTAINER_NAME}" bash -lc "keytool -genkeypair \
    -v \
    -keystore ${KEYSTORE_FILE} \
    -alias ${ALIAS} \
    -keyalg RSA \
    -keysize 2048 \
    -validity 10000 \
    -storepass ${STORE_PASS} \
    -keypass ${KEY_PASS} \
    -dname 'CN=Piper Light Alarm, OU=Local, O=Local, L=Home, S=Home, C=RU'"
else
  echo "==> Reusing keystore ${KEYSTORE_FILE}"
fi

echo "==> Writing android/keystore.properties"
"${DOCKER[@]}" exec "${CONTAINER_NAME}" bash -lc "cat > android/keystore.properties <<EOF
storePassword=${STORE_PASS}
keyPassword=${KEY_PASS}
keyAlias=${ALIAS}
storeFile=/work/${KEYSTORE_FILE}
EOF"

echo "==> Patching Gradle signing"
"${DOCKER[@]}" exec "${CONTAINER_NAME}" bash -lc 'python3 scripts/patch-android-signing.py'

echo "==> Gradle assembleRelease"
"${DOCKER[@]}" exec "${CONTAINER_NAME}" bash -lc 'cd android && ./gradlew assembleRelease --no-daemon'

APK_SRC="/work/android/app/build/outputs/apk/release/app-release.apk"
APK_OUT="${OUT_DIR}/piper-light-alarm.apk"

echo "==> docker cp -> ${APK_OUT}"
"${DOCKER[@]}" cp "${CONTAINER_NAME}:${APK_SRC}" "${APK_OUT}"

echo "==> Done"
ls -lh "${APK_OUT}"
