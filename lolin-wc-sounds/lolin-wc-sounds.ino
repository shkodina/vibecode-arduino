/*
  lolin-wc-sounds
  Lolin NodeMCU v3 (ESP8266): PIR + SD + MAX98357.

  Dvizhenie -> muzyka s karty po raspisaniyu iz config.json.
  Veb na porte 80, API na /api, swagger na /swagger/index.html.

  Formata zvuka: WAV PCM 16-bit, 16 kHz, mono.
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SD.h>
#include <SPI.h>
#include <time.h>
#include <ArduinoJson.h>
#include <i2s.h>
#include <i2s_reg.h>

// ---------- pinout NodeMCU v3 ----------
// PIR HC-SR501
const int PIN_PIR = D2;        // GPIO4

// MAX98357 I2S (zhestko na ESP8266)
// BCLK = GPIO15 (D8)
// WS/LRC = GPIO2 (D4)
// DATA = GPIO3 (RX)

// SD SPI
const int PIN_SD_CS = D0;      // GPIO16
const int PIN_SD_MOSI = D7;    // GPIO13
const int PIN_SD_MISO = D6;    // GPIO12
const int PIN_SD_SCK = D5;     // GPIO14

const int SAMPLE_RATE = 16000;
const int WAV_BITS = 16;
const int WAV_CHANNELS = 1;
const int BUFFER_BYTES = 4096;
const int MAX_SCHEDULE = 8;
const int MAX_FILES_IN_DIR = 40;
const int MAX_PATH = 64;
const char *CONFIG_PATH = "/config.json";
const char *FALLBACK_SSID = "WC-Sounds";
const char *FALLBACK_PASS = "wcsounds1";

ESP8266WebServer server(80);

struct Period {
  int startMin;
  int endMin;
  char directory[MAX_PATH];
  int volume;
  bool randomOnStartup;
  bool shuffle;
  bool repeatSelected;
  bool loopDirectory;
};

struct Config {
  char wifiSsid[33];
  char wifiPass[65];
  char ntpServer[64];
  int timezoneOffsetHours;
  unsigned long ntpUpdateSeconds;
  int httpPort;
  int motionTimeoutSec;
  int motionCooldownSec;
  Period periods[MAX_SCHEDULE];
  int periodCount;
};

Config cfg;
bool configOk = false;
bool sdOk = false;
bool wifiStaOk = false;
bool timeOk = false;
unsigned long lastNtpMs = 0;
unsigned long lastMotionMs = 0;
unsigned long lastTriggerMs = 0;
bool playing = false;
bool playRequested = false;
bool stopRequested = false;
int currentVolume = 100;
int currentPeriodIndex = -1;
char currentDir[MAX_PATH] = "/birds_sounds";
char currentFile[MAX_PATH] = "";
File wavFile;
int wavDataBytesLeft = 0;
uint8_t audioBufA[BUFFER_BYTES];
uint8_t audioBufB[BUFFER_BYTES];
int bufALen = 0;
int bufBLen = 0;
bool useBufA = true;
int playPos = 0;
char fileList[MAX_FILES_IN_DIR][MAX_PATH];
int fileCount = 0;
int fileIndex = 0;
String uploadTargetPath;
bool uploadOk = false;
String uploadError;
int uploadBytes = 0;
uint8_t wavHeaderBuf[44];
int wavHeaderGot = 0;
bool wavHeaderChecked = false;

// ---------- pomoshniki ----------

int parseHhMm(const char *s) {
  if (s == NULL || strlen(s) < 4) {
    return 0;
  }
  int h = atoi(s);
  const char *colon = strchr(s, ':');
  int m = 0;
  if (colon != NULL) {
    m = atoi(colon + 1);
  }
  if (h >= 24 && m == 0) {
    return 24 * 60;
  }
  return h * 60 + m;
}

void minutesToHhMm(int minutes, char *out, int outSize) {
  if (minutes >= 24 * 60) {
    snprintf(out, outSize, "24:00");
    return;
  }
  snprintf(out, outSize, "%02d:%02d", minutes / 60, minutes % 60);
}

void logMsg(const char *text) {
  Serial.println(text);
}

int clampVolume(int v) {
  if (v < 0) {
    return 0;
  }
  if (v > 100) {
    return 100;
  }
  return v;
}

int16_t scaleSample(int16_t sample, int volume) {
  long scaled = ((long)sample * volume) / 100;
  if (scaled > 32767) {
    scaled = 32767;
  }
  if (scaled < -32768) {
    scaled = -32768;
  }
  return (int16_t)scaled;
}

bool pathLooksSafe(const String &path) {
  if (!path.startsWith("/")) {
    return false;
  }
  if (path.indexOf("..") >= 0) {
    return false;
  }
  if (path.length() < 2 || path.length() >= MAX_PATH) {
    return false;
  }
  return true;
}

bool endsWithIgnoreCase(const String &value, const char *suffix) {
  int n = value.length();
  int m = strlen(suffix);
  if (n < m) {
    return false;
  }
  for (int i = 0; i < m; i++) {
    char a = value.charAt(n - m + i);
    char b = suffix[i];
    if (a >= 'A' && a <= 'Z') {
      a = a - 'A' + 'a';
    }
    if (b >= 'A' && b <= 'Z') {
      b = b - 'A' + 'a';
    }
    if (a != b) {
      return false;
    }
  }
  return true;
}

// ---------- config ----------

void setDefaultConfig() {
  strncpy(cfg.wifiSsid, "", sizeof(cfg.wifiSsid));
  strncpy(cfg.wifiPass, "", sizeof(cfg.wifiPass));
  strncpy(cfg.ntpServer, "pool.ntp.org", sizeof(cfg.ntpServer));
  cfg.timezoneOffsetHours = 3;
  cfg.ntpUpdateSeconds = 3600;
  cfg.httpPort = 80;
  cfg.motionTimeoutSec = 30;
  cfg.motionCooldownSec = 5;
  cfg.periodCount = 1;
  cfg.periods[0].startMin = 0;
  cfg.periods[0].endMin = 24 * 60;
  strncpy(cfg.periods[0].directory, "/birds_sounds", sizeof(cfg.periods[0].directory));
  cfg.periods[0].volume = 100;
  cfg.periods[0].randomOnStartup = true;
  cfg.periods[0].shuffle = false;
  cfg.periods[0].repeatSelected = true;
  cfg.periods[0].loopDirectory = false;
}

bool parseConfigJson(const String &jsonText) {
  DynamicJsonDocument doc(4096);
  DeserializationError err = deserializeJson(doc, jsonText);
  if (err) {
    Serial.print("JSON oshibka: ");
    Serial.println(err.c_str());
    return false;
  }

  setDefaultConfig();

  if (doc["wifi"]["ssid"].is<const char *>()) {
    strncpy(cfg.wifiSsid, doc["wifi"]["ssid"], sizeof(cfg.wifiSsid) - 1);
  }
  if (doc["wifi"]["password"].is<const char *>()) {
    strncpy(cfg.wifiPass, doc["wifi"]["password"], sizeof(cfg.wifiPass) - 1);
  }

  if (doc["ntp"]["server"].is<const char *>()) {
    strncpy(cfg.ntpServer, doc["ntp"]["server"], sizeof(cfg.ntpServer) - 1);
  }
  if (doc["ntp"]["timezone_offset"].is<int>()) {
    cfg.timezoneOffsetHours = doc["ntp"]["timezone_offset"];
  }
  if (doc["ntp"]["update_interval"].is<int>()) {
    cfg.ntpUpdateSeconds = doc["ntp"]["update_interval"];
  }

  if (doc["server"]["port"].is<int>()) {
    cfg.httpPort = doc["server"]["port"];
  }
  if (doc["motion"]["timeout_seconds"].is<int>()) {
    cfg.motionTimeoutSec = doc["motion"]["timeout_seconds"];
  }
  if (doc["motion"]["cooldown_seconds"].is<int>()) {
    cfg.motionCooldownSec = doc["motion"]["cooldown_seconds"];
  }

  JsonArray schedule = doc["playback"]["schedule"].as<JsonArray>();
  cfg.periodCount = 0;
  if (!schedule.isNull()) {
    for (JsonObject item : schedule) {
      if (cfg.periodCount >= MAX_SCHEDULE) {
        break;
      }
      Period &p = cfg.periods[cfg.periodCount];
      p.startMin = parseHhMm(item["start"] | "00:00");
      p.endMin = parseHhMm(item["end"] | "24:00");
      const char *dir = item["directory"] | "/birds_sounds";
      strncpy(p.directory, dir, sizeof(p.directory) - 1);
      p.volume = clampVolume(item["volume"] | 100);
      p.randomOnStartup = item["random_on_startup"] | true;
      p.shuffle = item["shuffle"] | false;
      p.repeatSelected = item["repeat_selected"] | true;
      p.loopDirectory = item["loop_directory"] | false;
      cfg.periodCount++;
    }
  }
  if (cfg.periodCount == 0) {
    setDefaultConfig();
  }
  return true;
}

String buildConfigJson() {
  DynamicJsonDocument doc(4096);
  doc["wifi"]["ssid"] = cfg.wifiSsid;
  doc["wifi"]["password"] = cfg.wifiPass;
  doc["ntp"]["server"] = cfg.ntpServer;
  doc["ntp"]["timezone_offset"] = cfg.timezoneOffsetHours;
  doc["ntp"]["update_interval"] = cfg.ntpUpdateSeconds;
  doc["server"]["port"] = cfg.httpPort;
  doc["motion"]["timeout_seconds"] = cfg.motionTimeoutSec;
  doc["motion"]["cooldown_seconds"] = cfg.motionCooldownSec;
  doc["logging"]["level"] = "INFO";
  doc["playback"]["type"] = "files";
  JsonArray schedule = doc["playback"].createNestedArray("schedule");
  for (int i = 0; i < cfg.periodCount; i++) {
    JsonObject item = schedule.createNestedObject();
    char startBuf[8];
    char endBuf[8];
    minutesToHhMm(cfg.periods[i].startMin, startBuf, sizeof(startBuf));
    minutesToHhMm(cfg.periods[i].endMin, endBuf, sizeof(endBuf));
    item["start"] = startBuf;
    item["end"] = endBuf;
    item["directory"] = cfg.periods[i].directory;
    item["volume"] = cfg.periods[i].volume;
    item["random_on_startup"] = cfg.periods[i].randomOnStartup;
    item["format"] = "wav";
    item["shuffle"] = cfg.periods[i].shuffle;
    item["repeat_selected"] = cfg.periods[i].repeatSelected;
    item["loop_directory"] = cfg.periods[i].loopDirectory;
  }
  String out;
  serializeJsonPretty(doc, out);
  return out;
}

bool saveConfigToSd() {
  if (!sdOk) {
    return false;
  }
  if (SD.exists(CONFIG_PATH)) {
    SD.remove(CONFIG_PATH);
  }
  File f = SD.open(CONFIG_PATH, FILE_WRITE);
  if (!f) {
    logMsg("Ne smog zapisat config.json");
    return false;
  }
  String json = buildConfigJson();
  f.print(json);
  f.close();
  return true;
}

bool loadConfigFromSd() {
  if (!sdOk) {
    return false;
  }
  File f = SD.open(CONFIG_PATH, FILE_READ);
  if (!f) {
    logMsg("config.json ne nayden, pishu shablon");
    setDefaultConfig();
    saveConfigToSd();
    return true;
  }
  String json = f.readString();
  f.close();
  return parseConfigJson(json);
}

// ---------- vremya ----------

void syncNtp() {
  configTime(cfg.timezoneOffsetHours * 3600, 0, cfg.ntpServer);
  time_t now = time(nullptr);
  int tries = 0;
  while (now < 1700000000 && tries < 20) {
    delay(250);
    now = time(nullptr);
    tries++;
  }
  timeOk = (now >= 1700000000);
  lastNtpMs = millis();
  if (timeOk) {
    logMsg("Vremya NTP obnovleno");
  } else {
    logMsg("NTP poka ne otvetil, prodolzhayu bez tochnogo vremeni");
  }
}

int currentMinutesOfDay() {
  time_t now = time(nullptr);
  if (now < 1700000000) {
    return -1;
  }
  struct tm t;
  localtime_r(&now, &t);
  return t.tm_hour * 60 + t.tm_min;
}

int findPeriodIndex(int minutes) {
  if (minutes < 0) {
    return 0;
  }
  for (int i = 0; i < cfg.periodCount; i++) {
    int start = cfg.periods[i].startMin;
    int end = cfg.periods[i].endMin;
    if (end <= start) {
      if (minutes >= start || minutes < end) {
        return i;
      }
    } else if (minutes >= start && minutes < end) {
      return i;
    }
  }
  return 0;
}

String currentTimeString() {
  time_t now = time(nullptr);
  if (now < 1700000000) {
    return "unknown";
  }
  struct tm t;
  localtime_r(&now, &t);
  char buf[20];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
           t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
           t.tm_hour, t.tm_min, t.tm_sec);
  return String(buf);
}

// ---------- SD i spisok faylov ----------

void closeWav() {
  if (wavFile) {
    wavFile.close();
  }
  wavDataBytesLeft = 0;
  bufALen = 0;
  bufBLen = 0;
  playPos = 0;
}

bool skipWavHeader(File &f, int *dataSize) {
  uint8_t hdr[12];
  if (f.read(hdr, 12) != 12) {
    return false;
  }
  if (memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) {
    return false;
  }
  bool fmtOk = false;
  *dataSize = 0;
  while (f.available()) {
    uint8_t chunk[8];
    if (f.read(chunk, 8) != 8) {
      return false;
    }
    uint32_t size = (uint32_t)chunk[4]
                  | ((uint32_t)chunk[5] << 8)
                  | ((uint32_t)chunk[6] << 16)
                  | ((uint32_t)chunk[7] << 24);
    if (memcmp(chunk, "fmt ", 4) == 0) {
      uint8_t fmt[16];
      if (size < 16) {
        return false;
      }
      if (f.read(fmt, 16) != 16) {
        return false;
      }
      if (size > 16) {
        f.seek(f.position() + (size - 16));
      }
      uint16_t audioFormat = fmt[0] | (fmt[1] << 8);
      uint16_t channels = fmt[2] | (fmt[3] << 8);
      uint32_t rate = fmt[4] | (fmt[5] << 8) | (fmt[6] << 16) | (fmt[7] << 24);
      uint16_t bits = fmt[14] | (fmt[15] << 8);
      fmtOk = (audioFormat == 1 && channels == WAV_CHANNELS && rate == (uint32_t)SAMPLE_RATE && bits == WAV_BITS);
    } else if (memcmp(chunk, "data", 4) == 0) {
      *dataSize = (int)size;
      return fmtOk && *dataSize > 0;
    } else {
      f.seek(f.position() + size);
    }
  }
  return false;
}

void collectWavFiles(const char *dirPath) {
  fileCount = 0;
  File dir = SD.open(dirPath);
  if (!dir || !dir.isDirectory()) {
    logMsg("Papka zvukov ne naydena");
    return;
  }
  while (fileCount < MAX_FILES_IN_DIR) {
    File entry = dir.openNextFile();
    if (!entry) {
      break;
    }
    String name = String(entry.name());
    entry.close();
    if (endsWithIgnoreCase(name, ".wav")) {
      if (!name.startsWith("/")) {
        snprintf(fileList[fileCount], MAX_PATH, "%s/%s", dirPath, name.c_str());
      } else {
        strncpy(fileList[fileCount], name.c_str(), MAX_PATH - 1);
      }
      fileCount++;
    }
  }
  dir.close();
}

bool openWavPath(const char *path) {
  closeWav();
  wavFile = SD.open(path, FILE_READ);
  if (!wavFile) {
    Serial.print("Ne otkryl wav: ");
    Serial.println(path);
    return false;
  }
  if (!skipWavHeader(wavFile, &wavDataBytesLeft)) {
    logMsg("WAV ne 16bit 16kHz mono, propuskayu");
    closeWav();
    return false;
  }
  strncpy(currentFile, path, sizeof(currentFile) - 1);
  bufALen = wavFile.read(audioBufA, BUFFER_BYTES);
  if (bufALen > wavDataBytesLeft) {
    bufALen = wavDataBytesLeft;
  }
  wavDataBytesLeft -= bufALen;
  bufBLen = 0;
  if (wavDataBytesLeft > 0) {
    bufBLen = wavFile.read(audioBufB, BUFFER_BYTES);
    if (bufBLen > wavDataBytesLeft) {
      bufBLen = wavDataBytesLeft;
    }
    wavDataBytesLeft -= bufBLen;
  }
  useBufA = true;
  playPos = 0;
  return bufALen > 0;
}

bool chooseNextFile(bool forceNew) {
  collectWavFiles(currentDir);
  if (fileCount <= 0) {
    return false;
  }
  Period p = cfg.periods[currentPeriodIndex < 0 ? 0 : currentPeriodIndex];
  if (p.shuffle || (forceNew && p.randomOnStartup)) {
    fileIndex = random(0, fileCount);
  } else if (forceNew) {
    fileIndex = 0;
  } else {
    fileIndex++;
    if (fileIndex >= fileCount) {
      if (p.loopDirectory) {
        fileIndex = 0;
      } else {
        fileIndex = fileCount - 1;
        return false;
      }
    }
  }
  return openWavPath(fileList[fileIndex]);
}

void applyPeriod(int index, bool restartFiles) {
  if (index < 0 || index >= cfg.periodCount) {
    index = 0;
  }
  currentPeriodIndex = index;
  strncpy(currentDir, cfg.periods[index].directory, sizeof(currentDir) - 1);
  currentVolume = cfg.periods[index].volume;
  if (restartFiles) {
    chooseNextFile(true);
  }
}

void startPlayback() {
  applyPeriod(findPeriodIndex(currentMinutesOfDay()), true);
  if (fileCount <= 0) {
    playing = false;
    logMsg("Net wav-faylov dlya vosproizvedeniya");
    return;
  }
  i2s_begin();
  i2s_set_rate(SAMPLE_RATE);
  playing = true;
  playRequested = false;
  logMsg("Igraem");
}

void stopPlayback() {
  playing = false;
  stopRequested = false;
  closeWav();
  i2s_end();
  logMsg("Stop");
}

void feedI2s() {
  if (!playing) {
    return;
  }
  uint8_t *buf = useBufA ? audioBufA : audioBufB;
  int len = useBufA ? bufALen : bufBLen;
  if (len <= 0) {
    Period p = cfg.periods[currentPeriodIndex < 0 ? 0 : currentPeriodIndex];
    if (p.repeatSelected && currentFile[0] != 0) {
      if (!openWavPath(currentFile)) {
        stopPlayback();
      }
    } else if (!chooseNextFile(false)) {
      stopPlayback();
    }
    return;
  }

  while (playPos + 1 < len) {
    int16_t sample = buf[playPos] | (buf[playPos + 1] << 8);
    sample = scaleSample(sample, currentVolume);
    uint32_t stereo = ((uint16_t)sample) | (((uint16_t)sample) << 16);
    if (!i2s_write_sample_nb(stereo)) {
      break;
    }
    playPos += 2;
  }

  if (playPos + 1 >= len) {
    playPos = 0;
    if (useBufA) {
      bufALen = 0;
      if (wavDataBytesLeft > 0 && wavFile) {
        bufALen = wavFile.read(audioBufA, BUFFER_BYTES);
        if (bufALen > wavDataBytesLeft) {
          bufALen = wavDataBytesLeft;
        }
        wavDataBytesLeft -= bufALen;
      }
    } else {
      bufBLen = 0;
      if (wavDataBytesLeft > 0 && wavFile) {
        bufBLen = wavFile.read(audioBufB, BUFFER_BYTES);
        if (bufBLen > wavDataBytesLeft) {
          bufBLen = wavDataBytesLeft;
        }
        wavDataBytesLeft -= bufBLen;
      }
    }
    useBufA = !useBufA;
  }
}

// ---------- proverka WAV pri zagruzke ----------

bool checkWavHeaderBytes(const uint8_t *data, int len, String &errorText) {
  if (len < 44) {
    errorText = "Slishkom korotkiy fayl, eto ne WAV";
    return false;
  }
  if (memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WAVE", 4) != 0) {
    errorText = "Eto ne WAV (net RIFF/WAVE v zagolovke)";
    return false;
  }
  const uint8_t *fmt = NULL;
  for (int i = 12; i + 8 < len; i++) {
    if (memcmp(data + i, "fmt ", 4) == 0) {
      fmt = data + i + 8;
      break;
    }
  }
  if (fmt == NULL || fmt + 16 > data + len) {
    errorText = "V WAV net bloka fmt";
    return false;
  }
  uint16_t audioFormat = fmt[0] | (fmt[1] << 8);
  uint16_t channels = fmt[2] | (fmt[3] << 8);
  uint32_t rate = fmt[4] | (fmt[5] << 8) | (fmt[6] << 16) | (fmt[7] << 24);
  uint16_t bits = fmt[14] | (fmt[15] << 8);
  if (audioFormat != 1 || channels != WAV_CHANNELS || rate != (uint32_t)SAMPLE_RATE || bits != WAV_BITS) {
    errorText = "Oshibka: nuzhen WAV 16-bit, 16kHz, mono. Tekushchiy fayl: ";
    errorText += String(bits);
    errorText += "-bit, ";
    errorText += String(rate);
    errorText += "Hz, ";
    errorText += String(channels);
    errorText += " kanalov";
    return false;
  }
  return true;
}

// ---------- wifi ----------

void startWifi() {
  if (strlen(cfg.wifiSsid) > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg.wifiSsid, cfg.wifiPass);
    Serial.print("Podklyuchayus k wifi: ");
    Serial.println(cfg.wifiSsid);
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 40) {
      delay(250);
      Serial.print(".");
      tries++;
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
      wifiStaOk = true;
      Serial.print("Wifi podklyuchen. IP: ");
      Serial.println(WiFi.localIP());
      return;
    }
    logMsg("Wifi STA ne podnyalsya, delayu tochku dostupa");
  }
  wifiStaOk = false;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(FALLBACK_SSID, FALLBACK_PASS);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
}

// ---------- html ----------

const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>WC Sounds</title>
<style>
body { font-family: sans-serif; max-width: 860px; margin: 16px auto; padding: 0 12px; }
label { display:block; margin-top:8px; }
input, select, button, textarea { font-size: 16px; }
.card { border:1px solid #ccc; padding:12px; margin:12px 0; }
pre { background:#f4f4f4; padding:8px; overflow:auto; }
</style>
</head>
<body>
<h1>WC Sounds</h1>
<p id="status">Загрузка...</p>
<div class="card">
  <button onclick="api('/api/play','POST')">Играть</button>
  <button onclick="api('/api/stop','POST')">Стоп</button>
  <label>Громкость <input id="vol" type="number" min="0" max="100" value="100"></label>
  <button onclick="setVolume()">Сохранить громкость</button>
</div>
<div class="card">
  <h2>Настройки</h2>
  <label>WiFi SSID <input id="ssid"></label>
  <label>WiFi пароль <input id="pass" type="password"></label>
  <label>NTP сервер <input id="ntp"></label>
  <label>Часовой пояс, часы <input id="tz" type="number"></label>
  <label>Интервал NTP, сек <input id="ntpint" type="number"></label>
  <label>Таймаут движения, сек <input id="mtime" type="number"></label>
  <label>Пауза между срабатываниями, сек <input id="mcool" type="number"></label>
  <h3>Расписание (JSON)</h3>
  <textarea id="schedule" rows="14" style="width:100%"></textarea>
  <p><button onclick="saveCfg()">Сохранить config.json</button></p>
</div>
<div class="card">
  <h2>Файлы</h2>
  <p><input id="upfile" type="file" accept=".wav"></p>
  <label>Путь на карте, например /birds_sounds/bird.wav
    <input id="uppath" value="/birds_sounds/bird.wav">
  </label>
  <button onclick="uploadFile()">Загрузить WAV 16kHz 16bit mono</button>
  <pre id="files"></pre>
</div>
<p><a href="/swagger/index.html">Swagger API</a></p>
<script>
async function loadStatus(){
  const s = await (await fetch('/api/status')).json();
  document.getElementById('status').textContent =
    'Время: '+s.time+' | играет: '+s.playing+' | файл: '+(s.file||'-')+' | IP: '+s.ip;
}
async function loadCfg(){
  const c = await (await fetch('/api/config')).json();
  document.getElementById('ssid').value = c.wifi.ssid||'';
  document.getElementById('pass').value = c.wifi.password||'';
  document.getElementById('ntp').value = c.ntp.server||'';
  document.getElementById('tz').value = c.ntp.timezone_offset||0;
  document.getElementById('ntpint').value = c.ntp.update_interval||3600;
  document.getElementById('mtime').value = c.motion.timeout_seconds||30;
  document.getElementById('mcool').value = c.motion.cooldown_seconds||5;
  document.getElementById('vol').value = (c.playback.schedule[0]||{}).volume||100;
  document.getElementById('schedule').value = JSON.stringify(c.playback.schedule, null, 2);
}
async function loadFiles(){
  const f = await (await fetch('/api/files?path=/')).json();
  document.getElementById('files').textContent = JSON.stringify(f, null, 2);
}
async function api(url, method){
  await fetch(url,{method});
  loadStatus();
}
async function setVolume(){
  const v = document.getElementById('vol').value;
  await fetch('/api/volume?value='+encodeURIComponent(v), {method:'POST'});
  loadStatus();
}
async function saveCfg(){
  const body = {
    wifi:{ssid:document.getElementById('ssid').value, password:document.getElementById('pass').value},
    ntp:{
      server:document.getElementById('ntp').value,
      timezone_offset:Number(document.getElementById('tz').value),
      update_interval:Number(document.getElementById('ntpint').value)
    },
    server:{port:80},
    motion:{
      timeout_seconds:Number(document.getElementById('mtime').value),
      cooldown_seconds:Number(document.getElementById('mcool').value)
    },
    logging:{level:'INFO'},
    playback:{type:'files', schedule: JSON.parse(document.getElementById('schedule').value)}
  };
  const r = await fetch('/api/config', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify(body)});
  alert(await r.text());
  loadStatus();
}
async function uploadFile(){
  const file = document.getElementById('upfile').files[0];
  const path = document.getElementById('uppath').value;
  if(!file){ alert('Выбери wav'); return; }
  const fd = new FormData();
  fd.append('file', file, file.name);
  const r = await fetch('/api/upload?path='+encodeURIComponent(path), {method:'POST', body:fd});
  alert(await r.text());
  loadFiles();
}
loadStatus(); loadCfg(); loadFiles();
setInterval(loadStatus, 4000);
</script>
</body>
</html>
)HTML";

const char SWAGGER_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="utf-8">
<title>WC Sounds API</title>
<style>
body { font-family: sans-serif; max-width: 900px; margin: 16px auto; }
code, pre { background:#f6f6f6; padding:2px 6px; }
.ep { border:1px solid #ddd; padding:10px; margin:10px 0; }
</style>
</head>
<body>
<h1>WC Sounds API</h1>
<p>OpenAPI-подобная страница. Базовый URL: этот модуль, порт 80.</p>
<div class="ep"><b>GET /api/status</b><br>Состояние: время, играет ли, текущий файл, громкость, IP.</div>
<div class="ep"><b>POST /api/play</b><br>Запустить музыку по текущему расписанию.</div>
<div class="ep"><b>POST /api/stop</b><br>Остановить музыку.</div>
<div class="ep"><b>POST /api/volume?value=0..100</b><br>Громкость прямо сейчас. В config не пишет.</div>
<div class="ep"><b>GET /api/config</b><br>Текущий config.json.</div>
<div class="ep"><b>POST /api/config</b><br>Тело: JSON конфигурации. Пишет на SD в /config.json и применяет сразу.</div>
<div class="ep"><b>GET /api/files?path=/birds_sounds</b><br>Список файлов в папке.</div>
<div class="ep"><b>POST /api/upload?path=/birds_sounds/a.wav</b><br>Тело: сырой WAV. Только PCM 16-bit 16kHz mono.</div>
<div class="ep"><b>DELETE /api/delete?path=/birds_sounds/a.wav</b><br>Удалить файл с карты.</div>
<div class="ep"><b>GET /swagger/index.html</b><br>Эта страница.</div>
<pre>
{
  "openapi": "3.0.0",
  "info": {"title": "WC Sounds", "version": "1.0.0"},
  "paths": {
    "/api/status": {"get": {"summary": "status"}},
    "/api/play": {"post": {"summary": "play"}},
    "/api/stop": {"post": {"summary": "stop"}},
    "/api/volume": {"post": {"summary": "volume"}},
    "/api/config": {"get": {"summary": "get config"}, "post": {"summary": "save config"}},
    "/api/files": {"get": {"summary": "list files"}},
    "/api/upload": {"post": {"summary": "upload wav"}},
    "/api/delete": {"delete": {"summary": "delete file"}}
  }
}
</pre>
<p><a href="/">Назад к настройкам</a></p>
</body>
</html>
)HTML";

// ---------- http handlers ----------

void sendText(int code, const String &text) {
  server.send(code, "text/plain; charset=utf-8", text);
}

void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void handleSwagger() {
  server.send_P(200, "text/html; charset=utf-8", SWAGGER_HTML);
}

void handleStatus() {
  DynamicJsonDocument doc(1024);
  doc["time"] = currentTimeString();
  doc["time_ok"] = timeOk;
  doc["playing"] = playing;
  doc["file"] = currentFile;
  doc["directory"] = currentDir;
  doc["volume"] = currentVolume;
  doc["sd_ok"] = sdOk;
  doc["wifi_sta"] = wifiStaOk;
  doc["ip"] = wifiStaOk ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
  doc["motion"] = digitalRead(PIN_PIR) == HIGH;
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handlePlay() {
  playRequested = true;
  sendText(200, "ok");
}

void handleStop() {
  stopRequested = true;
  sendText(200, "ok");
}

void handleVolume() {
  if (!server.hasArg("value")) {
    sendText(400, "nuzhen parametr value");
    return;
  }
  currentVolume = clampVolume(server.arg("value").toInt());
  sendText(200, "ok");
}

void handleGetConfig() {
  server.send(200, "application/json", buildConfigJson());
}

void handlePostConfig() {
  if (server.hasArg("plain") == false) {
    sendText(400, "pustoe telo");
    return;
  }
  if (!parseConfigJson(server.arg("plain"))) {
    sendText(400, "ne smog razobrat json");
    return;
  }
  if (!saveConfigToSd()) {
    sendText(500, "ne smog zapisat na SD");
    return;
  }
  sendText(200, "sohraneno. wifi pomenyaetsya posle perezagruzki");
}

void handleFiles() {
  String path = server.hasArg("path") ? server.arg("path") : "/";
  if (!pathLooksSafe(path)) {
    sendText(400, "plohoy path");
    return;
  }
  File dir = SD.open(path);
  if (!dir) {
    sendText(404, "net takoy papki");
    return;
  }
  DynamicJsonDocument doc(2048);
  JsonArray arr = doc.createNestedArray("files");
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) {
      break;
    }
    JsonObject o = arr.createNestedObject();
    o["name"] = String(entry.name());
    o["size"] = entry.size();
    o["dir"] = entry.isDirectory();
    entry.close();
  }
  dir.close();
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handleDelete() {
  String path = server.arg("path");
  if (!pathLooksSafe(path) || path == CONFIG_PATH) {
    sendText(400, "plohoy path");
    return;
  }
  if (!SD.exists(path.c_str())) {
    sendText(404, "net fayla");
    return;
  }
  if (playing && String(currentFile) == path) {
    stopPlayback();
  }
  if (SD.remove(path.c_str())) {
    sendText(200, "udalen");
  } else {
    sendText(500, "ne udalil");
  }
}

File uploadFileHandle;

void handleUpload() {
  HTTPUpload &up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    uploadOk = true;
    uploadError = "";
    uploadBytes = 0;
    wavHeaderGot = 0;
    wavHeaderChecked = false;
    uploadTargetPath = server.arg("path");
    if (!pathLooksSafe(uploadTargetPath) || !endsWithIgnoreCase(uploadTargetPath, ".wav")) {
      uploadOk = false;
      uploadError = "path dolzhen byt /papka/file.wav";
      return;
    }
    if (playing) {
      stopPlayback();
    }
    if (SD.exists(uploadTargetPath.c_str())) {
      SD.remove(uploadTargetPath.c_str());
    }
    int slash = uploadTargetPath.lastIndexOf('/');
    if (slash > 0) {
      String dir = uploadTargetPath.substring(0, slash);
      if (!SD.exists(dir.c_str())) {
        SD.mkdir(dir.c_str());
      }
    }
    uploadFileHandle = SD.open(uploadTargetPath.c_str(), FILE_WRITE);
    if (!uploadFileHandle) {
      uploadOk = false;
      uploadError = "ne smog sozdat fayl na SD";
    }
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (!uploadOk) {
      return;
    }
    const uint8_t *data = up.buf;
    int len = up.currentSize;
    if (!wavHeaderChecked) {
      int need = 44 - wavHeaderGot;
      int take = len < need ? len : need;
      memcpy(wavHeaderBuf + wavHeaderGot, data, take);
      wavHeaderGot += take;
      if (wavHeaderGot >= 44) {
        wavHeaderChecked = true;
        String err;
        if (!checkWavHeaderBytes(wavHeaderBuf, 44, err)) {
          uploadOk = false;
          uploadError = err;
          if (uploadFileHandle) {
            uploadFileHandle.close();
          }
          SD.remove(uploadTargetPath.c_str());
          return;
        }
      }
    }
    if (uploadFileHandle) {
      uploadFileHandle.write(data, len);
      uploadBytes += len;
    }
  } else if (up.status == UPLOAD_FILE_END) {
    if (uploadFileHandle) {
      uploadFileHandle.close();
    }
    if (uploadOk && !wavHeaderChecked) {
      uploadOk = false;
      uploadError = "Slishkom korotkiy fayl, eto ne WAV 16-bit 16kHz mono";
      SD.remove(uploadTargetPath.c_str());
    }
  }
}

void handleUploadDone() {
  if (uploadOk) {
    sendText(200, "zagruzhen " + uploadTargetPath + " (" + String(uploadBytes) + " byte)");
  } else {
    sendText(400, uploadError);
  }
}

void handleNotFound() {
  sendText(404, "net takogo adresa");
}

void setupServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/swagger/index.html", HTTP_GET, handleSwagger);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/play", HTTP_POST, handlePlay);
  server.on("/api/stop", HTTP_POST, handleStop);
  server.on("/api/volume", HTTP_POST, handleVolume);
  server.on("/api/config", HTTP_GET, handleGetConfig);
  server.on("/api/config", HTTP_POST, handlePostConfig);
  server.on("/api/files", HTTP_GET, handleFiles);
  server.on("/api/delete", HTTP_DELETE, handleDelete);
  server.on("/api/upload", HTTP_POST, handleUploadDone, handleUpload);
  server.onNotFound(handleNotFound);
  server.begin(cfg.httpPort);
  logMsg("Veb-server zapushen");
}

// ---------- setup / loop ----------

void setup() {
  Serial.begin(115200);
  delay(200);
  logMsg("lolin-wc-sounds start");
  pinMode(PIN_PIR, INPUT);

  SPI.begin();
  sdOk = SD.begin(PIN_SD_CS);
  if (!sdOk) {
    logMsg("SD karta ne vidna. Prover pin CS=D0 i format FAT32.");
    setDefaultConfig();
  } else {
    logMsg("SD ok");
    configOk = loadConfigFromSd();
    if (!configOk) {
      logMsg("config.json plohoy, beru znacheniya po umolchaniyu");
      setDefaultConfig();
    }
  }

  randomSeed(ESP.getCycleCount());
  startWifi();
  if (wifiStaOk) {
    syncNtp();
  }
  setupServer();
}

void loop() {
  server.handleClient();

  unsigned long now = millis();
  if (wifiStaOk && cfg.ntpUpdateSeconds > 0 && now - lastNtpMs > cfg.ntpUpdateSeconds * 1000UL) {
    syncNtp();
  }

  bool motion = digitalRead(PIN_PIR) == HIGH;
  if (motion) {
    lastMotionMs = now;
    if (!playing && now - lastTriggerMs > (unsigned long)cfg.motionCooldownSec * 1000UL) {
      lastTriggerMs = now;
      playRequested = true;
    }
  }
  if (playing && cfg.motionTimeoutSec > 0 && now - lastMotionMs > (unsigned long)cfg.motionTimeoutSec * 1000UL) {
    stopRequested = true;
  }

  if (playRequested && !playing) {
    startPlayback();
  }
  if (stopRequested && playing) {
    stopPlayback();
  }

  if (playing) {
    feedI2s();
  }
}
