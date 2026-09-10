# Task 02: Docker APK build

> **For agentic workers:** выполнять после или параллельно с
> `01-android-app.md`. Главный результат — воспроизводимая Docker-сборка APK
> на Ubuntu 22.04 и копирование готового файла в директорию проекта.

## Цель

Сделать Dockerfile и bash-скрипт, который собирает Android APK внутри
контейнера, генерирует или переиспользует ключ подписи и копирует APK наружу.

## Ожидаемые файлы

- Создать: `ambient-morning-alarm--apk/Dockerfile`
- Создать: `ambient-morning-alarm--apk/build-apk.sh`
- Создать или изменить: `ambient-morning-alarm--apk/.gitignore`
- Создать или изменить: `ambient-morning-alarm--apk/readme.md`
- Использовать проект из: `ambient-morning-alarm--apk/app`

## Выходной файл

Готовый APK складывать сюда:

```text
ambient-morning-alarm--apk/out/piper-light-alarm.apk
```

Если собирается debug-вариант, допустимо имя:

```text
ambient-morning-alarm--apk/out/piper-light-alarm-debug.apk
```

Но README должен явно сказать, какой файл ставить на телефон.

## Dockerfile

Базовый образ:

```dockerfile
FROM ubuntu:22.04
```

Установить:

- `openjdk-17-jdk`;
- `curl`;
- `unzip`;
- `git`;
- `bash`;
- Android command line tools;
- Android SDK platform, build-tools и platform-tools;
- Gradle wrapper использовать из проекта, если он есть.

Переменные:

```dockerfile
ENV ANDROID_HOME=/opt/android-sdk
ENV ANDROID_SDK_ROOT=/opt/android-sdk
ENV PATH="${ANDROID_HOME}/cmdline-tools/latest/bin:${ANDROID_HOME}/platform-tools:${PATH}"
```

Установка SDK должна принимать licenses:

```bash
yes | sdkmanager --licenses
```

Минимально установить:

```bash
sdkmanager "platform-tools" "platforms;android-35" "build-tools;35.0.0"
```

Если Android Gradle Plugin требует другую версию SDK, синхронизировать
Dockerfile и `app/build.gradle`.

## Ключ подписи

Требование: обновления приложения должны ставиться поверх старых сборок.

Рекомендуемое решение:

- ключ хранить в Docker volume или в локальной папке `keystore/`, которая
  игнорируется git;
- если ключа нет, скрипт генерирует его через `keytool`;
- alias: `piper-light-alarm`;
- пароль можно использовать локальный и несекретный для личного приложения,
  например `piper-light-alarm`;
- не коммитить `.jks`.

Команда генерации:

```bash
keytool -genkeypair \
  -v \
  -keystore keystore/piper-light-alarm.jks \
  -alias piper-light-alarm \
  -keyalg RSA \
  -keysize 2048 \
  -validity 10000 \
  -storepass piper-light-alarm \
  -keypass piper-light-alarm \
  -dname "CN=Piper Light Alarm, OU=Local, O=Local, L=Home, S=Home, C=RU"
```

Gradle signingConfig должен читать путь и пароль из переменных окружения или
`gradle.properties`, созданного скриптом внутри контейнера.

## `build-apk.sh`

Скрипт должен:

1. `cd "$(dirname "$0")"`.
2. Собрать Docker image, например `ambient-morning-alarm-apk-builder`.
3. Создать `out/` и `keystore/`.
4. Запустить контейнер в sleep-режиме:

```bash
docker run -d --name ambient-morning-alarm-apk-build \
  -v "$PWD":/work \
  -w /work \
  ambient-morning-alarm-apk-builder \
  sleep infinity
```

5. В контейнере сгенерировать ключ, если его нет.
6. В контейнере запустить сборку:

```bash
docker exec ambient-morning-alarm-apk-build ./gradlew assembleRelease
```

7. Скопировать APK из контейнера:

```bash
docker cp ambient-morning-alarm-apk-build:/work/app/build/outputs/apk/release/app-release.apk \
  out/piper-light-alarm.apk
```

8. Остановить и удалить контейнер через `trap`, чтобы он не оставался после
   ошибки.

Если используется volume mount проекта, `docker cp` формально не нужен, но
оставить его в скрипте, потому что исходное требование просит копировать файл
docker-командой.

## `.gitignore`

Минимум:

```gitignore
.gradle/
build/
app/build/
out/
keystore/
*.jks
*.apk
local.properties
```

## README

`readme.md` должен содержать:

1. Что делает приложение.
2. Как оно подключается к будильнику.
3. Какие Bluetooth permissions используются.
4. Как собрать APK через Docker:

```bash
cd ambient-morning-alarm--apk
./build-apk.sh
```

5. Где лежит готовый APK.
6. Как установить APK на телефон вручную.
7. Почему ключ хранится локально и не попадает в git.
8. Как пересобрать обновление так, чтобы Android принял его как то же
   приложение.

## Критерии готовности

- `docker build` проходит на Ubuntu 22.04.
- `./build-apk.sh` создаёт APK в `out/`.
- Повторный запуск использует тот же `.jks`, если он уже был создан.
- APK подписан и устанавливается на телефон.
- `.jks`, `out/` и build outputs не попадают в git.
- README объясняет сборку без обращения к исходному prompt.

## Проверка

```bash
cd ambient-morning-alarm--apk
chmod +x build-apk.sh
./build-apk.sh
ls -lh out/
```

Проверить подпись:

```bash
apksigner verify --verbose out/piper-light-alarm.apk
```

Проверить permissions:

```bash
aapt dump permissions out/piper-light-alarm.apk
```

Ожидаемо APK не должен запрашивать интернет, геолокацию, камеру, контакты,
файлы или уведомления.

