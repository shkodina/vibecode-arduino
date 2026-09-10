# Task 01: Android BLE application

> **For agentic workers:** выполнять в отдельной сессии. Перед стартом прочитай
> `../concept-plan.md` и `../../ambient-morning-alarm/tasks/03-bluetooth.md`.

## Цель

Создать Android-приложение для настройки светового будильника по BLE. Оно
должно поддерживать функционал веб-интерфейса: будильники, таймер, тест
свечения, статус и остановку активного запуска.

## Ожидаемые файлы

Если проекта ещё нет, создать минимальный Android Gradle project:

- `ambient-morning-alarm--apk/settings.gradle`
- `ambient-morning-alarm--apk/build.gradle`
- `ambient-morning-alarm--apk/app/build.gradle`
- `ambient-morning-alarm--apk/app/src/main/AndroidManifest.xml`
- `ambient-morning-alarm--apk/app/src/main/java/.../MainActivity.kt`
- `ambient-morning-alarm--apk/app/src/main/java/.../ble/AlarmBleClient.kt`
- `ambient-morning-alarm--apk/app/src/main/java/.../model/Models.kt`
- `ambient-morning-alarm--apk/app/src/main/java/.../protocol/AlarmProtocol.kt`
- `ambient-morning-alarm--apk/app/src/main/res/values/strings.xml`
- `ambient-morning-alarm--apk/app/src/main/res/values/colors.xml`

Пакет приложения: `local.piper.ambientmorningalarm`.

## Минимальный SDK

Рекомендуется:

```gradle
minSdk 23
targetSdk 35
```

Если установленный Android Gradle Plugin требует другой `targetSdk`, выбрать
совместимое значение и указать его в README.

## Разрешения

В `AndroidManifest.xml` добавить только Bluetooth-разрешения:

```xml
<uses-permission android:name="android.permission.BLUETOOTH" android:maxSdkVersion="30" />
<uses-permission android:name="android.permission.BLUETOOTH_ADMIN" android:maxSdkVersion="30" />
<uses-permission android:name="android.permission.BLUETOOTH_SCAN" />
<uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />
```

Не добавлять интернет, геолокацию, камеру, контакты, файлы или уведомления,
если они не стали строго необходимы.

## BLE константы

```kotlin
const val DEVICE_NAME = "piper-light-alarm-0000001"
val SERVICE_UUID = UUID.fromString("7c1b0000-7df0-4b6f-bc6f-a110c0000001")
val COMMAND_UUID = UUID.fromString("7c1b0001-7df0-4b6f-bc6f-a110c0000001")
val RESPONSE_UUID = UUID.fromString("7c1b0002-7df0-4b6f-bc6f-a110c0000001")
val STATUS_NOTIFY_UUID = UUID.fromString("7c1b0003-7df0-4b6f-bc6f-a110c0000001")
```

## Модели

Реализовать модели, соответствующие JSON прошивки:

```kotlin
data class LightModeConfig(
    val type: String,
    val startBrightness: Int,
    val finishBrightness: Int,
    val rampSeconds: Int,
    val darkSeconds: Int,
    val totalSeconds: Int
)

data class AlarmConfig(
    val id: Int,
    val enabled: Boolean,
    val hour: Int,
    val minute: Int,
    val weekdaysMask: Int,
    val mode: LightModeConfig
)

data class TimerConfig(
    val hours: Int,
    val minutes: Int,
    val mode: LightModeConfig
)

data class DeviceSettings(
    val alarms: List<AlarmConfig>,
    val timer: TimerConfig
)

data class ActiveRun(
    val type: String,
    val alarmId: Int?,
    val mode: String,
    val startedAt: String?,
    val remainingSeconds: Int,
    val brightness: Int
)

data class DeviceStatus(
    val currentTime: String,
    val firmwareVersion: String,
    val ntpSynced: Boolean,
    val wifiConnected: Boolean,
    val activeRun: ActiveRun?
)
```

## Протокол команд

`AlarmProtocol` должен уметь собрать команды:

- `getSettings`;
- `setSettings`;
- `getStatus`;
- `stop`;
- `startTimer`;
- `startTest`;
- `stopTest`.

Каждая команда содержит `requestId`, чтобы сопоставлять ответы:

```json
{
  "requestId": "42",
  "command": "getStatus"
}
```

Ответ:

```json
{
  "requestId": "42",
  "ok": true,
  "payload": {}
}
```

Если BLE-ответ приходит фрагментами, собрать их до полного JSON. Формат
chunking должен совпасть с прошивкой, если он будет добавлен в
`../../ambient-morning-alarm/tasks/03-bluetooth.md`.

## BLE клиент

`AlarmBleClient` должен:

1. Проверять Bluetooth permissions.
2. Сканировать BLE устройства.
3. Фильтровать по имени `piper-light-alarm-0000001`.
4. Подключаться к GATT.
5. Искать service UUID.
6. Подписываться на `status notify`.
7. Писать команды в `command`.
8. Читать или получать notify из `response`.
9. Переподключаться по кнопке пользователя, не бесконечным фоном.

Состояния клиента:

- `NoPermission`;
- `Scanning`;
- `DeviceNotFound`;
- `Connecting`;
- `Connected`;
- `Disconnected`;
- `Error(message)`.

## UI

Можно использовать обычные Android Views или Jetpack Compose. Выбирать тот
вариант, который проще собрать в Docker.

Общий стиль:

- тёмный фон;
- оранжевые акценты;
- крупные поля для телефона;
- без внешнего интернета в рантайме.

Навигация:

- `Будильники`;
- `Таймер`;
- `Тест`.

Верхняя панель на всех экранах:

- текущее время устройства;
- версия;
- значок/текст подключения;
- баннер ошибки, если подключение отсутствует или команда не удалась;
- блок активного запуска с кнопкой `Остановить`, если `activeRun != null`.

## Экран будильников

Показать 10 карточек:

- номер `0..9`;
- switch `enabled`;
- часы и минуты;
- чекбоксы дней недели `Пн Вт Ср Чт Пт Сб Вс`;
- radio/segmented control `ramp` или `pulse`;
- параметры режима;
- кнопка `Сохранить`.

Сохранение отправляет весь `DeviceSettings`, а не только один будильник.

## Экран таймера

Поля:

- часы ожидания;
- минуты ожидания;
- `startBrightness`;
- `finishBrightness`;
- `rampSeconds`;
- `darkSeconds`;
- `totalSeconds`.

Кнопки:

- `Сохранить`;
- `Запустить таймер`;
- `Остановить`.

## Экран теста

Поля:

- режим `ramp` или `pulse`;
- `startBrightness`;
- `finishBrightness`;
- `rampSeconds`;
- `darkSeconds`;
- `totalSeconds`.

Кнопки:

- `Старт`;
- `Стоп`.

Тест не должен сохранять параметры в настройки, если пользователь не нажал
отдельную кнопку сохранения.

## Валидация

Перед отправкой:

- часы `0..23`;
- минуты `0..59`;
- яркость `0..100`;
- `finishBrightness >= startBrightness`;
- `rampSeconds > 0`;
- `totalSeconds > 0`;
- для `pulse`: `darkSeconds > 0`;
- для `ramp`: `darkSeconds = 0`.

При ошибке показывать текст рядом с полем или общим баннером.

## Критерии готовности

- APK собирается локально через Gradle.
- Приложение запрашивает только Bluetooth-разрешения.
- При отсутствии подключения открывается экран будильников и сверху виден
  баннер ошибки.
- При активном будильнике на старте показывается блок активного запуска и
  кнопка остановки.
- Настройки читаются из устройства и сохраняются обратно.
- Таймер запускается из приложения.
- Тест свечения стартует и останавливается.
- Потеря соединения не стирает форму.

## Проверка

Локальная сборка:

```bash
cd ambient-morning-alarm--apk
./gradlew assembleDebug
```

Проверка разрешений:

```bash
aapt dump permissions app/build/outputs/apk/debug/app-debug.apk
```

Ожидаемо в списке только Bluetooth-разрешения из этого файла.

Ручная проверка на Redmi Note 12:

1. Установить APK.
2. Выдать Bluetooth-разрешение.
3. Подключиться к `piper-light-alarm-0000001`.
4. Проверить чтение статуса.
5. Изменить будильник и сохранить.
6. Запустить тест свечения.
7. Остановить свечение.

