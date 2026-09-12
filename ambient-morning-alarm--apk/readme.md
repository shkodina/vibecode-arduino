# ambient-morning-alarm--apk

Android-приложение (React Native / Expo Dev Client) для настройки светового
будильника `ambient-morning-alarm` по BLE с телефона, в том числе Redmi Note 12.

## Что делает

- ищет BLE-устройства с префиксом имени `piper-light-alarm-`;
- подключается к GATT service прошивки;
- показывает статус, активный запуск и кнопку «Остановить»;
- экраны: **Будильники**, **Таймер**, **Тест**, **WiFi**;
- команды: `getSettings`, `setSettings`, `getWifi`, `setWifi`, `getStatus`,
  `stop`, `startTimer`, `startTest`, `stopTest`.

Облако, push, геолокация «для себя» и управление через интернет не используются.

## Разрешения

В `app.json` явно прописаны только Bluetooth-права:

- `BLUETOOTH` / `BLUETOOTH_ADMIN` (для Android ≤ 11)
- `BLUETOOTH_SCAN`
- `BLUETOOTH_CONNECT`

Камера, файлы, фон-локация и уведомления заблокированы через `blockedPermissions`.

Фактически в собранном APK Expo / React Native всё равно протаскивают лишнее
(проверено `aapt dump permissions` 2026-09-12):

- `ACCESS_COARSE_LOCATION`, `ACCESS_FINE_LOCATION` — типичный хвост BLE-плагинов
  и старых Android; на Redmi Note 12 (Android 13+) для scan не нужны
- `INTERNET` — дефолт RN/Expo Dev Client, приложение облаком не пользуется
- `SYSTEM_ALERT_WINDOW`, `VIBRATE` — тоже от Expo Dev Client

Интернет, геолокацию «для себя» и управление через сеть приложение не делает.

## Стек

- Expo SDK 57 + `expo-dev-client`
- React Native `0.86.3`, React `19.2.3`
- `react-native-ble-plx`
- TypeScript
- тёмный UI с оранжевыми акцентами

Пакет: `local.piper.ambientmorningalarm`.

## Поиск устройства

Приложение сканирует BLE и подключается к первому устройству с префиксом
имени `piper-light-alarm-` (например `piper-light-alarm-0000002`).

## Сборка APK через Docker

На Ubuntu 22.04 с установленным Docker:

```bash
cd ambient-morning-alarm--apk
chmod +x build-apk.sh
./build-apk.sh
```

Скрипт:

1. собирает Docker-образ;
2. запускает контейнер в `sleep infinity`;
3. ставит npm-зависимости;
4. делает `expo prebuild` для Android;
5. создаёт или переиспользует ключ в `keystore/piper-light-alarm.jks`;
6. собирает release APK через Gradle;
7. копирует APK командой `docker cp` в `out/piper-light-alarm.apk`;
8. удаляет контейнер.

Готовый файл для установки:

```text
out/piper-light-alarm.apk
```

Размер последней рабочей сборки ~67 MB, подпись APK Signature Scheme v2
(`apksigner verify` проходит). Скрипт после `docker cp` оставляет файл от root,
если Docker вызывался через `sudo` — тогда `chown` себе.

`build-apk.sh` для не-root вызывает `sudo docker`. Нужны Docker на хосте и
либо группа `docker`, либо passwordless sudo.

## Android SDK, которые реально собираются

Источник правды — `app.json` → `expo-build-properties`. Скрипт делает
`expo prebuild --clean`, поэтому правки в сгенерированном `android/` сгорят.

| поле | значение | почему так |
|---|---|---|
| `minSdkVersion` | **24** | RN 0.86 / Hermes: при 23 CMake падает `No compatible library found for //ReactAndroid/hermestooling` (`User has minSdkVersion 23 but library was built for 24`) |
| `targetSdkVersion` | **35** | можно не поднимать вместе с compileSdk |
| `compileSdkVersion` | **36** | `androidx.core:core:1.17.0` и `core-ktx:1.17.0` требуют compileSdk ≥ 36. При 35 падает `:app:checkReleaseAarMetadata` |

В `tasks/01-android-app.md` ещё написано `minSdk 23` / `targetSdk 35` — для
текущего стека это уже неверно, смотри `app.json`.

`Dockerfile` ставит `platforms;android-35`, `build-tools;35.0.0`, NDK
`27.1.12297006`. Gradle при сборке сам докачивает недостающее в контейнер:

- Android SDK Platform / Build-Tools **36**
- CMake **3.22.1**

Лицензии SDK принимаются (`yes | sdkmanager --licenses`). Если Gradle пишет
про license — пересобери образ, не чини руками в `android/`.

## Что ломалось при первой сборке

1. **`minSdkVersion: 23`** — `:app:configureCMakeRelWithDebInfo` и
   `:expo-modules-core:configureCMakeRelWithDebInfo` на `hermestooling`.
   Фикс: `24` в `app.json`.
2. **`compileSdkVersion: 35`** — `:app:checkReleaseAarMetadata` из-за
   androidx.core 1.17. Фикс: `36` в `app.json`. Образ Docker ради этого
   пересобирать не обязательно: платформу 36 Gradle подтянет сам.
3. Expo ругается, что Node **20.18.1** в Dockerfile старше требуемого
   `>= 20.19.4`. Prebuild при этом проходит. Имеет смысл поднять
   `NODE_VERSION` в Dockerfile при следующей правке образа.
4. `npx expo prebuild --non-interactive` пишет
   `--non-interactive is not supported, use $CI=1 instead`. Сборка идёт дальше.
   Если prebuild начнёт спрашивать stdin — в `docker exec` добавить `CI=1`.
5. `docker build` без Buildx: `The legacy builder is deprecated`. На результат
   не влияет.
6. CMake пишет `Hard link ... failed. Doing a slower copy instead` — из-за
   bind-mount `/work`. Это не ошибка.

Первый `docker build` долгий (JDK, Node, cmdline-tools, NDK ~700 MB, platform
35). Первый Gradle ещё ~15–25 мин (Gradle 9.3.1 wrapper, нативка на несколько
ABI). Повторный запуск с уже собранным образом `ambient-morning-alarm-apk-builder`
гораздо быстрее, но `prebuild --clean` каждый раз заново генерирует `android/`
и заново компилирует native.

## Проверка APK

Из образа сборщика (утилиты лежат в `$ANDROID_HOME/build-tools/35.0.0` или
`36.0.0`, не в `$PATH`):

```bash
docker run --rm \
  -v "$PWD/out":/out \
  ambient-morning-alarm-apk-builder \
  bash -lc 'export PATH="$ANDROID_HOME/build-tools/35.0.0:$PATH"
    aapt dump permissions /out/piper-light-alarm.apk
    apksigner verify --verbose /out/piper-light-alarm.apk'
```

Ожидаемо: `Verifies`, v2 scheme, пакет `local.piper.ambientmorningalarm`.

## Установка на телефон

1. Скопируй `out/piper-light-alarm.apk` на Redmi Note 12.
2. Разреши установку из неизвестных источников для файлового менеджера.
3. Установи APK.
4. При первом запуске выдай Bluetooth-разрешения.
5. Нажми «Подключить» — приложение найдёт `piper-light-alarm-*`.
6. Если телефон спросит PIN BLE — `8888` (из `config.h` прошивки).

## Ключ подписи

Ключ лежит локально в `keystore/` и в git не попадает. Пароль локальный:
`piper-light-alarm`. Пока используется тот же `.jks`, Android принимает
обновления поверх уже установленного приложения.

## Локальная разработка

```bash
npm install
npx expo prebuild --platform android
npx expo run:android
```

Нужны Android SDK (compileSdk 36, minSdk 24) и устройство/эмулятор с BLE.
Локальный `android/` после `prebuild --clean` в Docker лучше не править вручную.

## Связь с прошивкой

Контракт UUID и JSON — в `../ambient-morning-alarm/tasks/03-bluetooth.md` и
`../ambient-morning-alarm/readme.md`.
