# lolin-wc-sounds--apk

Android APK для управления модулями `lolin-wc-sounds` напрямую из телефона,
без FastAPI control plane на компьютере.

## Что делает

- сканирует локальную подсеть по HTTP port 80 и узнаёт модуль по
  `GET /api/status` с ключами `playing`, `sd_ok`, `volume`, `ip`, `motion`,
  `file`, `directory`;
- сохраняет найденные IP локально в AsyncStorage и проверяет их при старте;
- показывает список модулей, online/offline, играет ли сейчас, громкость,
  текущий файл и остаток секунд;
- выбранный модуль: `play`, `stop`, статус, применение громкости,
  `reload`, `reboot`;
- редактирует `config.json`: WiFi, NTP, motion-параметры, watchdog,
  расписание `playback.schedule`;
- добавляет, удаляет и редактирует периоды расписания;
- работает с SD через API прошивки: `GET /api/files`, `POST /api/mkdir`,
  `DELETE /api/delete`, `POST /api/upload`;
- выбирает аудиофайл на телефоне, транслитерирует имя и перед загрузкой
  конвертирует через `ffmpeg-kit-react-native` в WAV PCM 16-bit 16 kHz mono.

## Экраны

- **Модули**: CIDR подсеть, поиск, ручное добавление IP, сохранённые модули.
- **Пульт**: что играет сейчас, движение/PIR/SD/WiFi/NTP, play/stop/volume.
- **Расписание**: CRUD периодов `start` / `end` / `directory` / `volume` /
  random/shuffle/repeat/loop.
- **Файлы**: папки SD, создание папки, удаление файлов, выбор аудио,
  конвертация и upload.
- **Настройки**: WiFi, NTP, motion, watchdog, сохранение/restore config.

## Сборка APK через Docker

```bash
cd /home/blobby/work_dir/vibecode-arduino/lolin-wc-sounds--apk
chmod +x build-apk.sh
./build-apk.sh
```

Готовый файл:

```text
out/wc-sounds.apk
```

Сборка повторяет подход из `../ambient-morning-alarm--apk/`: Docker,
`expo prebuild --clean`, Gradle release, локальный keystore и `docker cp`
результата в `out/`.

## Важные ограничения

- Приложение сканирует подсеть из поля CIDR. Автоопределение подсети телефона
  не используется, дефолт как у control plane: `192.168.88.0/24`.
- HTTP к ESP8266 идёт по cleartext, поэтому в `app.json` включён
  `usesCleartextTraffic`.
- Смена WiFi записывает `/config.json` на SD, но сам модуль подключится к
  новой сети только после reboot, как описано в прошивке.
