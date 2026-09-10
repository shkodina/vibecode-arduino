#ifndef CONFIG_H
#define CONFIG_H

// Все быстрые настройки прошивки. Скетч трогать не обязательно.
// WiFi STA ssid/пароль в исходники НЕ пишутся — приходят из .env через sborka.sh.

// --- пины (Lolin NodeMCU v3 / ESP-12E) ---

// HX711: DT и SCK. Не использовать D8 (GPIO15) — мешает загрузке.
const int PIN_HX711_DT = D2;   // GPIO4
const int PIN_HX711_SCK = D1;  // GPIO5

// Переключатель на 3 контакта (SPDT):
//   контакт 2 (общий) -> PIN_REZHIM
//   контакт 1         -> 3V3   (UseExistedWiFi, уровень HIGH)
//   контакт 3         -> GND   (StandAlone, уровень LOW)
// Пин читается с INPUT_PULLUP: без переключателя/при обрыве будет UseExistedWiFi.
const int PIN_REZHIM = D5;     // GPIO14
const int UROVEN_STA = HIGH;   // UseExistedWiFi: клиент существующей сети
const int UROVEN_AP = LOW;     // StandAlone: своя точка доступа

// --- точка доступа StandAlone ---

const char* AP_SSID = "lolinsilomer";
const char* AP_PASS = "lolinsilomer";

// --- заводские значения конфигурации (EEPROM) ---

const float ZAVOD_TRIGGER_KG = 5.0f;       // кг, порог начала измерения
const unsigned long ZAVOD_PERIOD_SEC = 10; // сек, длина периода измерения
const unsigned long ZAVOD_WDT_SEC = 30;    // сек, софтовый вачдог
const char* ZAVOD_DEVICE_NAME = "silomer-stanovaya";

// STA ssid/пароль по умолчанию — из WIFI_SSID / WIFI_PASS на этапе сборки.
// Если в EEPROM уже есть сохранённые — используются они.

// --- тензодатчик / HX711 ---

// Сколько сырых отсчётов усреднять за одно чтение веса.
const int HX711_SAMPLES = 3;

// Как часто обновлять вес в loop (мс).
const unsigned long PERIOD_VESA_MS = 200;

// Сколько ждать новый отсчёт HX711 в API tare/calibrate.
const unsigned long HX711_READY_TIMEOUT_MS = 1200;

// Калибровка: вес_кг = (сырое - offset) / SCALE.
// Было 420.0f, но на текущем железе 3 кг показывались как 33.7 кг.
// 420 * 33.7 / 3.0 = 4718 — новый стартовый коэффициент для этого стенда.
const float ZAVOD_HX711_SCALE = 4718.0f;
const long ZAVOD_HX711_OFFSET = 0;

// Номинал датчика. Нужен для UI/API и контроля здравого смысла настроек.
const float ZAVOD_SENSOR_MAX_KG = 1000.0f;

// Автоматический tare при старте. По умолчанию выключен: если при перезагрузке
// на датчике висит груз, auto-tare сделал бы этот груз новым нулём.
const bool HX711_TARE_PRI_STARTE = false;

// --- измерение ---

// Если вес упал ниже триггера на столько мс — период считается завершённым.
const unsigned long PAUZA_KONCA_IZMERENIYA_MS = 1500;

// --- веб / websocket ---

const int HTTP_PORT = 80;
const int WS_PORT = 81;                 // ws://IP:81/api/ws
const unsigned long WS_PUSH_MS = 250;    // как часто слать вес в websocket

// --- EEPROM ---

const int EEPROM_SIZE = 512;
const uint32_t EEPROM_MAGIC = 0x51C0FE02UL;

// Лимиты строк в конфиге.
const int MAX_SSID_LEN = 32;
const int MAX_PASS_LEN = 64;
const int MAX_NAME_LEN = 32;

// --- отладка ---

const unsigned long SERIAL_BAUD = 115200;
const unsigned long PERIOD_OTLADKI_MS = 2000;

#endif
