#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

IMAGE_NAME="lolin-wc-sounds-apk-builder"
CONTAINER_NAME="lolin-wc-sounds-apk-build"
OUT_DIR="out"
KEYSTORE_DIR="keystore"
KEYSTORE_FILE="${KEYSTORE_DIR}/wc-sounds.jks"
ALIAS="wc-sounds"
STORE_PASS="wc-sounds-local"
KEY_PASS="wc-sounds-local"

if [[ "$(id -u)" -eq 0 ]]; then
  DOCKER=(docker)
else
  DOCKER=(sudo docker)
fi

if ! "${DOCKER[@]}" version >/dev/null 2>&1; then
  echo "Docker недоступен (${DOCKER[*]}). Установи Docker и повтори." >&2
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

echo "==> npm install"
"${DOCKER[@]}" exec "${CONTAINER_NAME}" bash -lc 'npm install'

echo "==> Patching ffmpeg package metadata for RN 0.86 old architecture"
"${DOCKER[@]}" exec "${CONTAINER_NAME}" bash -lc \
  "node - <<'JS'
const fs = require('fs');
const path = 'node_modules/@nikhil-cephei/ffmpeg-kit-react-native/package.json';
const pkg = JSON.parse(fs.readFileSync(path, 'utf8'));
delete pkg.codegenConfig;
fs.writeFileSync(path, JSON.stringify(pkg, null, 2) + '\n');
const gradlePath = 'node_modules/@nikhil-cephei/ffmpeg-kit-react-native/android/build.gradle';
let gradle = fs.readFileSync(gradlePath, 'utf8');
gradle = gradle.replace(
  \"java.srcDirs += isNewArchitectureEnabled() ? ['src/newarch'] : ['src/oldarch']\",
  \"java.srcDirs += ['src/oldarch']\"
);
fs.writeFileSync(gradlePath, gradle);
JS"

echo "==> expo prebuild (android)"
"${DOCKER[@]}" exec "${CONTAINER_NAME}" bash -lc \
  'CI=1 npx expo prebuild --platform android --clean'

echo "==> Disabling React Native New Architecture for ffmpeg native module"
"${DOCKER[@]}" exec "${CONTAINER_NAME}" bash -lc \
  "python3 - <<'PY'
from pathlib import Path
path = Path('android/gradle.properties')
text = path.read_text()
text = text.replace('newArchEnabled=true', 'newArchEnabled=false')
path.write_text(text)
cmake = Path('android/app/build/generated/autolinking/src/main/jni/Android-autolinking.cmake')
if cmake.exists():
    text = cmake.read_text()
    lines = [
        line for line in text.splitlines()
        if 'ffmpeg-kit-react-native' not in line
        and 'FFmpegKitReactNativeSpec' not in line
    ]
    cmake.write_text('\n'.join(lines) + '\n')
PY"

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
    -dname 'CN=WC Sounds, OU=Local, O=Local, L=Home, S=Home, C=RU'"
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
APK_OUT="${OUT_DIR}/wc-sounds.apk"

echo "==> docker cp -> ${APK_OUT}"
"${DOCKER[@]}" cp "${CONTAINER_NAME}:${APK_SRC}" "${APK_OUT}"

echo "==> Done"
ls -lh "${APK_OUT}"
