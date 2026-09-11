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

На Android 12+ приложение запрашивает:

- `BLUETOOTH_SCAN`
- `BLUETOOTH_CONNECT`

На Android до 12 для BLE scan системе может понадобиться `ACCESS_FINE_LOCATION`
(только для сканирования, не для трекинга). Redmi Note 12 обычно на Android 13+,
там location для scan не нужен.

## Стек

- Expo SDK 57 + `expo-dev-client`
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

Нужны Android SDK и устройство/эмулятор с BLE.

## Связь с прошивкой

Контракт UUID и JSON — в `../ambient-morning-alarm/tasks/03-bluetooth.md` и
`../ambient-morning-alarm/readme.md`.
