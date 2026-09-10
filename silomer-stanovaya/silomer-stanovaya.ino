// Силомер становая: Lolin NodeMCU v3 + HX711 + S-образный тензодатчик.
//
// WiFi-логин и пароль по умолчанию приходят на этапе сборки из .env через
// WIFI_SSID и WIFI_PASS. В исходниках их не храним.

#include <Arduino.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <HX711.h>
#include <WebSocketsServer.h>

#include "config.h"

#ifndef WIFI_SSID
#error "WIFI_SSID ne zadan. Sobiray cherez ./sborka.sh, smotri readme.md"
#endif

#ifndef WIFI_PASS
#error "WIFI_PASS ne zadan. Sobiray cherez ./sborka.sh, smotri readme.md"
#endif

struct Konfiguraciya {
  uint32_t magic;
  float triggerKg;
  uint32_t periodSec;
  uint32_t watchdogSec;
  float calibrationScale;
  long calibrationOffset;
  float sensorMaxKg;
  char wifiSsid[MAX_SSID_LEN + 1];
  char wifiPass[MAX_PASS_LEN + 1];
  char deviceName[MAX_NAME_LEN + 1];
};

HX711 vesy;
ESP8266WebServer server(HTTP_PORT);
WebSocketsServer webSocket(WS_PORT);
Konfiguraciya config;

float tekushiyVesKg = 0.0f;
float maxTekushiyKg = 0.0f;
float maxProshliyKg = 0.0f;

bool izmerenieAktivno = false;
unsigned long startIzmereniyaMs = 0;
unsigned long posledniyVesVysheTriggeraMs = 0;
unsigned long posledneeChtenieVesaMs = 0;
unsigned long poslednyayaOtladkaMs = 0;
unsigned long poslednyayaWsOtpravkaMs = 0;
unsigned long startMs = 0;
unsigned long schetchikWs = 0;

bool zaprosPerezagruzkiWdt = false;
unsigned long startPerezagruzkiWdtMs = 0;

void skopirovatStroku(char* kuda, size_t razmer, const char* otkuda) {
  if (razmer == 0) {
    return;
  }
  if (otkuda == nullptr) {
    otkuda = "";
  }
  strncpy(kuda, otkuda, razmer - 1);
  kuda[razmer - 1] = '\0';
}

void zavodskayaKonfiguraciya() {
  memset(&config, 0, sizeof(config));
  config.magic = EEPROM_MAGIC;
  config.triggerKg = ZAVOD_TRIGGER_KG;
  config.periodSec = ZAVOD_PERIOD_SEC;
  config.watchdogSec = ZAVOD_WDT_SEC;
  config.calibrationScale = ZAVOD_HX711_SCALE;
  config.calibrationOffset = ZAVOD_HX711_OFFSET;
  config.sensorMaxKg = ZAVOD_SENSOR_MAX_KG;
  skopirovatStroku(config.wifiSsid, sizeof(config.wifiSsid), WIFI_SSID);
  skopirovatStroku(config.wifiPass, sizeof(config.wifiPass), WIFI_PASS);
  skopirovatStroku(config.deviceName, sizeof(config.deviceName), ZAVOD_DEVICE_NAME);
}

void sohranitKonfiguraciyu() {
  config.magic = EEPROM_MAGIC;
  EEPROM.put(0, config);
  EEPROM.commit();
}

void zagruzitKonfiguraciyu() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0, config);

  if (config.magic != EEPROM_MAGIC ||
      config.periodSec == 0 ||
      config.watchdogSec == 0 ||
      config.calibrationScale == 0.0f ||
      config.sensorMaxKg <= 0.0f ||
      isnan(config.triggerKg)) {
    zavodskayaKonfiguraciya();
    sohranitKonfiguraciyu();
  }

  if (strlen(config.wifiSsid) == 0) {
    skopirovatStroku(config.wifiSsid, sizeof(config.wifiSsid), WIFI_SSID);
  }
  if (strlen(config.wifiPass) == 0) {
    skopirovatStroku(config.wifiPass, sizeof(config.wifiPass), WIFI_PASS);
  }
  if (strlen(config.deviceName) == 0) {
    skopirovatStroku(config.deviceName, sizeof(config.deviceName), ZAVOD_DEVICE_NAME);
  }
}

String jsonKonfiguracii(bool skrytParol) {
  StaticJsonDocument<512> doc;
  doc["deviceName"] = config.deviceName;
  doc["wifiSsid"] = config.wifiSsid;
  doc["wifiPass"] = skrytParol ? "********" : config.wifiPass;
  doc["triggerKg"] = config.triggerKg;
  doc["periodSec"] = config.periodSec;
  doc["watchdogSec"] = config.watchdogSec;
  doc["calibrationScale"] = config.calibrationScale;
  doc["calibrationOffset"] = config.calibrationOffset;
  doc["sensorMaxKg"] = config.sensorMaxKg;

  String otvet;
  serializeJson(doc, otvet);
  return otvet;
}

String jsonStatusa() {
  StaticJsonDocument<768> doc;
  doc["uptimeSec"] = millis() / 1000;
  doc["deviceName"] = config.deviceName;
  doc["wifiMode"] = WiFi.getMode() == WIFI_AP ? "StandAlone" : "UseExistedWiFi";
  doc["ip"] = WiFi.getMode() == WIFI_AP ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  doc["tekushiyVesKg"] = tekushiyVesKg;
  doc["maxTekushiyKg"] = maxTekushiyKg;
  doc["maxProshliyKg"] = maxProshliyKg;
  doc["triggerKg"] = config.triggerKg;
  doc["periodSec"] = config.periodSec;
  doc["izmerenieAktivno"] = izmerenieAktivno;
  doc["watchdogSec"] = config.watchdogSec;
  doc["watchdogStatus"] = zaprosPerezagruzkiWdt ? "reboot_requested" : "ok";
  doc["calibrationScale"] = config.calibrationScale;
  doc["calibrationOffset"] = config.calibrationOffset;
  doc["sensorMaxKg"] = config.sensorMaxKg;
  doc["wsClients"] = webSocket.connectedClients();
  doc["hx711Ready"] = vesy.is_ready();

  String otvet;
  serializeJson(doc, otvet);
  return otvet;
}

void nachatIzmerenie(unsigned long seychas) {
  izmerenieAktivno = true;
  startIzmereniyaMs = seychas;
  posledniyVesVysheTriggeraMs = seychas;
  maxTekushiyKg = tekushiyVesKg;
}

void zavershitIzmerenie() {
  if (izmerenieAktivno) {
    maxProshliyKg = maxTekushiyKg;
  }
  izmerenieAktivno = false;
  maxTekushiyKg = 0.0f;
}

void obnovitIzmerenie(unsigned long seychas) {
  if (tekushiyVesKg >= config.triggerKg) {
    posledniyVesVysheTriggeraMs = seychas;
    if (!izmerenieAktivno) {
      nachatIzmerenie(seychas);
    }
    if (tekushiyVesKg > maxTekushiyKg) {
      maxTekushiyKg = tekushiyVesKg;
    }
  }

  if (!izmerenieAktivno) {
    return;
  }

  bool periodIstek = seychas - startIzmereniyaMs >= config.periodSec * 1000UL;
  bool vesUpal = tekushiyVesKg < config.triggerKg &&
                 seychas - posledniyVesVysheTriggeraMs >= PAUZA_KONCA_IZMERENIYA_MS;

  if (periodIstek || vesUpal) {
    zavershitIzmerenie();
  }
}

void obrabotatVesy() {
  unsigned long seychas = millis();
  if (seychas - posledneeChtenieVesaMs < PERIOD_VESA_MS) {
    return;
  }
  posledneeChtenieVesaMs = seychas;

  if (!vesy.is_ready()) {
    return;
  }

  tekushiyVesKg = vesy.get_units(HX711_SAMPLES);
  if (tekushiyVesKg < 0.0f && tekushiyVesKg > -0.2f) {
    tekushiyVesKg = 0.0f;
  }
  obnovitIzmerenie(seychas);
}

void otvetJson(int kod, const String& telo) {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.send(kod, "application/json", telo);
}

void otvetCorsOptions() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.send(204);
}

String poluchitArg(const char* imya) {
  if (server.hasArg(imya)) {
    return server.arg(imya);
  }
  return "";
}

bool obnovitKonfiguraciyuIzJson(const String& telo, String& oshibka) {
  StaticJsonDocument<768> doc;
  DeserializationError err = deserializeJson(doc, telo);
  if (err) {
    oshibka = "json parse error";
    return false;
  }

  if (doc.containsKey("triggerKg")) {
    config.triggerKg = doc["triggerKg"].as<float>();
  }
  if (doc.containsKey("periodSec")) {
    config.periodSec = max(1UL, doc["periodSec"].as<unsigned long>());
  }
  if (doc.containsKey("watchdogSec")) {
    config.watchdogSec = max(1UL, doc["watchdogSec"].as<unsigned long>());
  }
  if (doc.containsKey("calibrationScale")) {
    float scale = doc["calibrationScale"].as<float>();
    if (scale == 0.0f || isnan(scale)) {
      oshibka = "calibrationScale must be non-zero";
      return false;
    }
    config.calibrationScale = scale;
    vesy.set_scale(config.calibrationScale);
  }
  if (doc.containsKey("calibrationOffset")) {
    config.calibrationOffset = doc["calibrationOffset"].as<long>();
    vesy.set_offset(config.calibrationOffset);
  }
  if (doc.containsKey("sensorMaxKg")) {
    float maxKg = doc["sensorMaxKg"].as<float>();
    if (maxKg <= 0.0f || isnan(maxKg)) {
      oshibka = "sensorMaxKg must be positive";
      return false;
    }
    config.sensorMaxKg = maxKg;
  }
  if (doc.containsKey("wifiSsid")) {
    skopirovatStroku(config.wifiSsid, sizeof(config.wifiSsid), doc["wifiSsid"].as<const char*>());
  }
  if (doc.containsKey("wifiPass")) {
    skopirovatStroku(config.wifiPass, sizeof(config.wifiPass), doc["wifiPass"].as<const char*>());
  }
  if (doc.containsKey("deviceName")) {
    skopirovatStroku(config.deviceName, sizeof(config.deviceName), doc["deviceName"].as<const char*>());
  }

  sohranitKonfiguraciyu();
  return true;
}

String imyaWifiRezhima() {
  return WiFi.getMode() == WIFI_AP ? "StandAlone" : "UseExistedWiFi";
}

bool zhdatGotovnostHX711() {
  unsigned long start = millis();
  while (!vesy.is_ready() && millis() - start < HX711_READY_TIMEOUT_MS) {
    server.handleClient();
    webSocket.loop();
    yield();
  }
  return vesy.is_ready();
}

bool otkalibrovatPoVesu(float etalonKg, String& oshibka) {
  if (etalonKg <= 0.0f || isnan(etalonKg)) {
    oshibka = "knownKg must be positive";
    return false;
  }
  if (!zhdatGotovnostHX711()) {
    oshibka = "hx711 is not ready";
    return false;
  }

  long syroe = vesy.read_average(max(5, HX711_SAMPLES * 4));
  float novyyScale = (syroe - config.calibrationOffset) / etalonKg;
  if (novyyScale == 0.0f || isnan(novyyScale)) {
    oshibka = "calculated calibrationScale is invalid";
    return false;
  }

  config.calibrationScale = novyyScale;
  vesy.set_scale(config.calibrationScale);
  sohranitKonfiguraciyu();
  return true;
}

const char* STRANICA_HTML = R"HTML(
<!doctype html>
<html lang="ru">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Силомер становая</title>
  <style>
    body { margin: 0; font-family: Arial, sans-serif; background: #101418; color: #eef3f8; }
    main { max-width: 900px; margin: 0 auto; padding: 24px; }
    h1 { margin-top: 0; }
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(210px, 1fr)); gap: 12px; }
    .card, form { background: #1c242d; border: 1px solid #2e3a46; border-radius: 12px; padding: 16px; }
    .value { font-size: 42px; font-weight: bold; }
    .unit { opacity: 0.75; }
    label { display: block; margin: 10px 0 4px; }
    input { width: 100%; box-sizing: border-box; padding: 9px; border-radius: 8px; border: 1px solid #526170; }
    button { margin-top: 12px; padding: 10px 14px; border: 0; border-radius: 8px; background: #33a1ff; color: #00101f; font-weight: bold; cursor: pointer; }
    button.pressed { transform: translateY(2px); filter: brightness(0.85); }
    button.danger { background: #ff6b6b; color: #260000; }
    .action-status { position: sticky; bottom: 0; margin-top: 16px; padding: 12px 14px; border-radius: 10px; background: #233142; color: #dff1ff; min-height: 20px; }
    .action-status.error { background: #4a2020; color: #ffdada; }
    a { color: #8dccff; }
    small { color: #b7c4d0; }
  </style>
</head>
<body>
<main>
  <h1 id="device">Силомер</h1>
  <div class="grid">
    <div class="card"><div>Текущий вес</div><div class="value"><span id="w">--</span> <span class="unit">кг</span></div></div>
    <div class="card"><div>Максимум текущего измерения</div><div class="value"><span id="mc">--</span> <span class="unit">кг</span></div></div>
    <div class="card"><div>Максимум прошлого измерения</div><div class="value"><span id="mp">--</span> <span class="unit">кг</span></div></div>
    <div class="card"><div>Статус</div><p id="status">загрузка...</p><small><a href="/swagger">Swagger UI</a></small></div>
  </div>

  <h2>Настройки</h2>
  <form id="cfg">
    <label>Триггерный вес, кг</label>
    <input id="triggerKg" type="number" step="0.1" min="0">
    <label>Период измерения, сек</label>
    <input id="periodSec" type="number" step="1" min="1">
    <label>Таймаут watchdog, сек</label>
    <input id="watchdogSec" type="number" step="1" min="1">
    <label>Калибровочный коэффициент HX711, raw/кг</label>
    <input id="calibrationScale" type="number" step="0.1">
    <label>Калибровочный offset HX711, raw</label>
    <input id="calibrationOffset" type="number" step="1">
    <label>Максимальный номинальный вес датчика, кг</label>
    <input id="sensorMaxKg" type="number" step="1" min="1">
    <label>Имя устройства для веб-админки</label>
    <input id="deviceName" maxlength="32">
    <label>Логин новой WiFi-сети</label>
    <input id="wifiSsid" maxlength="32">
    <label>Скрытый пароль новой WiFi-сети</label>
    <input id="wifiPass" type="password" maxlength="64" autocomplete="new-password">
    <button type="submit">Сохранить настройки</button>
  </form>
  <form id="cal">
    <label>Эталонный вес для калибровки, кг</label>
    <input id="knownKg" type="number" step="0.1" min="0.1" value="3.0">
    <button type="submit">Откалибровать по текущему весу</button>
  </form>
  <button onclick="tare(event)">Tare: принять пустой датчик за 0 кг</button>
  <button class="danger" onclick="resetFactory(event)">Сбросить все настройки в заводские</button>
  <button class="danger" onclick="rebootWdt(event)">Перезагрузить контроллер через watchdog</button>
  <div id="actionStatus" class="action-status">Готов к работе.</div>
</main>
<script>
function kg(x) { return Number(x || 0).toFixed(1); }
function qs(id) { return document.getElementById(id); }
function showActionStatus(message, isError) {
  var box = qs('actionStatus');
  box.textContent = message;
  box.classList.toggle('error', !!isError);
}
function pressButton(button) {
  if (!button) return;
  button.classList.add('pressed');
  setTimeout(function() { button.classList.remove('pressed'); }, 180);
}
async function checkedFetch(url, options) {
  var r = await fetch(url, options || {});
  if (!r.ok) {
    var text = await r.text();
    throw new Error(text || ('HTTP ' + r.status));
  }
  return r;
}
async function loadConfig() {
  const c = await fetch('/api/config').then(r => r.json());
  qs('triggerKg').value = c.triggerKg;
  qs('periodSec').value = c.periodSec;
  qs('watchdogSec').value = c.watchdogSec;
  qs('calibrationScale').value = c.calibrationScale;
  qs('calibrationOffset').value = c.calibrationOffset;
  qs('sensorMaxKg').value = c.sensorMaxKg;
  qs('deviceName').value = c.deviceName;
  qs('wifiSsid').value = c.wifiSsid;
  qs('device').textContent = c.deviceName || 'Силомер';
}
async function loadStatus() {
  const s = await fetch('/api/status').then(r => r.json());
  qs('w').textContent = kg(s.tekushiyVesKg);
  qs('mc').textContent = kg(s.maxTekushiyKg);
  qs('mp').textContent = kg(s.maxProshliyKg);
  qs('status').textContent = 'режим ' + s.wifiMode + ', uptime ' + s.uptimeSec + ' сек, watchdog ' + s.watchdogStatus;
}
qs('cfg').addEventListener('submit', async function(e) {
  e.preventDefault();
  pressButton(e.submitter);
  const payload = {
    triggerKg: Number(qs('triggerKg').value),
    periodSec: Number(qs('periodSec').value),
    watchdogSec: Number(qs('watchdogSec').value),
    calibrationScale: Number(qs('calibrationScale').value),
    calibrationOffset: Number(qs('calibrationOffset').value),
    sensorMaxKg: Number(qs('sensorMaxKg').value),
    deviceName: qs('deviceName').value,
    wifiSsid: qs('wifiSsid').value,
    wifiPass: qs('wifiPass').value
  };
  if (!payload.wifiPass) delete payload.wifiPass;
  try {
    await checkedFetch('/api/config', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(payload) });
    await loadConfig();
    await loadStatus();
    showActionStatus('Настройки сохранены.', false);
  } catch (err) {
    showActionStatus('Ошибка сохранения настроек: ' + err.message, true);
  }
});
qs('cal').addEventListener('submit', async function(e) {
  e.preventDefault();
  pressButton(e.submitter);
  try {
    await checkedFetch('/api/calibrate', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({knownKg: Number(qs('knownKg').value)}) });
    await loadConfig();
    await loadStatus();
    showActionStatus('Калибровка выполнена.', false);
  } catch (err) {
    showActionStatus('Ошибка калибровки: ' + err.message, true);
  }
});
async function tare(event) {
  pressButton(event && event.target);
  try {
    await checkedFetch('/api/tare', { method: 'POST' });
    await loadConfig();
    await loadStatus();
    showActionStatus('Tare выполнен: текущий пустой датчик принят за 0 кг.', false);
  } catch (err) {
    showActionStatus('Ошибка tare: ' + err.message, true);
  }
}
async function resetFactory(event) {
  pressButton(event && event.target);
  if (confirm('Сбросить настройки?')) {
    try {
      await checkedFetch('/api/reset', { method: 'POST' });
      await loadConfig();
      await loadStatus();
      showActionStatus('Сброс выполнен: заводские настройки восстановлены.', false);
    } catch (err) {
      showActionStatus('Ошибка сброса: ' + err.message, true);
    }
  }
}
async function rebootWdt(event) {
  pressButton(event && event.target);
  try {
    await checkedFetch('/api/reboot', { method: 'POST' });
    showActionStatus('Watchdog-перезагрузка запрошена.', false);
  } catch (err) {
    showActionStatus('Ошибка запроса перезагрузки: ' + err.message, true);
  }
}
loadConfig();
loadStatus();
setInterval(loadStatus, 500);
</script>
</body>
</html>
)HTML";

const char* SWAGGER_HTML = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <title>Silomer API Swagger</title>
  <link rel="stylesheet" href="https://unpkg.com/swagger-ui-dist@5/swagger-ui.css">
</head>
<body>
  <div id="swagger-ui"></div>
  <script src="https://unpkg.com/swagger-ui-dist@5/swagger-ui-bundle.js"></script>
  <script>SwaggerUIBundle({url:'/api/openapi.json',dom_id:'#swagger-ui'});</script>
  <noscript>Swagger UI needs JavaScript. Open /api/openapi.json for the API description.</noscript>
</body>
</html>
)HTML";

const char* OPENAPI_JSON = R"JSON(
{
  "openapi": "3.0.0",
  "info": {"title": "Silomer Stanovaya API", "version": "1.0.0"},
  "paths": {
    "/api/config": {
      "get": {"summary": "Read configuration", "responses": {"200": {"description": "Config JSON"}}},
      "post": {"summary": "Update configuration", "responses": {"200": {"description": "Updated config JSON"}}}
    },
    "/api/status": {
      "get": {"summary": "Read current controller status", "responses": {"200": {"description": "Status JSON"}}}
    },
    "/api/reset": {
      "post": {"summary": "Factory reset configuration", "responses": {"200": {"description": "Factory config JSON"}}}
    },
    "/api/reboot": {
      "post": {"summary": "Request watchdog reboot", "responses": {"200": {"description": "Reboot status JSON"}}}
    },
    "/api/tare": {
      "post": {"summary": "Set current empty sensor as zero", "responses": {"200": {"description": "Updated config JSON"}}}
    },
    "/api/calibrate": {
      "post": {"summary": "Calibrate scale by known weight", "responses": {"200": {"description": "Updated config JSON"}}}
    },
    "/api/ws": {
      "get": {"summary": "Weight updates over WebSocket on port 81", "responses": {"101": {"description": "WebSocket upgrade"}}}
    }
  }
}
)JSON";

void otdatGlavnuyuStranicu() {
  server.send(200, "text/html; charset=utf-8", STRANICA_HTML);
}

void otdatConfig() {
  otvetJson(200, jsonKonfiguracii(true));
}

void prinyatConfig() {
  String oshibka;
  if (!obnovitKonfiguraciyuIzJson(server.arg("plain"), oshibka)) {
    otvetJson(400, "{\"error\":\"" + oshibka + "\"}");
    return;
  }
  otvetJson(200, jsonKonfiguracii(true));
}

void otdatStatus() {
  otvetJson(200, jsonStatusa());
}

void sbrositConfig() {
  zavodskayaKonfiguraciya();
  sohranitKonfiguraciyu();
  otvetJson(200, jsonKonfiguracii(true));
}

void zaprositPerezagruzkuWdt() {
  zaprosPerezagruzkiWdt = true;
  startPerezagruzkiWdtMs = millis();
  otvetJson(200, "{\"watchdogStatus\":\"reboot_requested\"}");
}

void sdelatTare() {
  if (!zhdatGotovnostHX711()) {
    otvetJson(503, "{\"error\":\"hx711 is not ready\"}");
    return;
  }

  vesy.tare(max(5, HX711_SAMPLES * 4));
  config.calibrationOffset = vesy.get_offset();
  sohranitKonfiguraciyu();
  otvetJson(200, jsonKonfiguracii(true));
}

void otkalibrovat() {
  StaticJsonDocument<192> doc;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    otvetJson(400, "{\"error\":\"json parse error\"}");
    return;
  }

  String oshibka;
  if (!otkalibrovatPoVesu(doc["knownKg"].as<float>(), oshibka)) {
    otvetJson(400, "{\"error\":\"" + oshibka + "\"}");
    return;
  }
  otvetJson(200, jsonKonfiguracii(true));
}

void otdatSwagger() {
  server.send(200, "text/html; charset=utf-8", SWAGGER_HTML);
}

void otdatOpenApi() {
  server.send(200, "application/json", OPENAPI_JSON);
}

void otdatNeNajdeno() {
  if (server.method() == HTTP_OPTIONS) {
    otvetCorsOptions();
    return;
  }
  server.send(404, "text/plain; charset=utf-8", "Takoy stranicy net");
}

void nastroitMarshruty() {
  server.on("/", HTTP_GET, otdatGlavnuyuStranicu);
  server.on("/api/config", HTTP_OPTIONS, otvetCorsOptions);
  server.on("/api/config", HTTP_GET, otdatConfig);
  server.on("/api/config", HTTP_POST, prinyatConfig);
  server.on("/api/status", HTTP_OPTIONS, otvetCorsOptions);
  server.on("/api/status", HTTP_GET, otdatStatus);
  server.on("/api/reset", HTTP_OPTIONS, otvetCorsOptions);
  server.on("/api/reset", HTTP_POST, sbrositConfig);
  server.on("/api/reboot", HTTP_OPTIONS, otvetCorsOptions);
  server.on("/api/reboot", HTTP_POST, zaprositPerezagruzkuWdt);
  server.on("/api/tare", HTTP_OPTIONS, otvetCorsOptions);
  server.on("/api/tare", HTTP_POST, sdelatTare);
  server.on("/api/calibrate", HTTP_OPTIONS, otvetCorsOptions);
  server.on("/api/calibrate", HTTP_POST, otkalibrovat);
  server.on("/swagger", HTTP_GET, otdatSwagger);
  server.on("/api/openapi.json", HTTP_GET, otdatOpenApi);
  server.onNotFound(otdatNeNajdeno);
}

void obrabotatWs(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  (void)payload;
  (void)length;
  if (type == WStype_CONNECTED) {
    String status = jsonStatusa();
    webSocket.sendTXT(num, status);
  }
}

void otpravitWsStatus() {
  unsigned long seychas = millis();
  if (seychas - poslednyayaWsOtpravkaMs < WS_PUSH_MS) {
    return;
  }
  poslednyayaWsOtpravkaMs = seychas;
  schetchikWs++;
  String status = jsonStatusa();
  webSocket.broadcastTXT(status);
}

void zapustitWifi() {
  pinMode(PIN_REZHIM, INPUT_PULLUP);
  int rezhim = digitalRead(PIN_REZHIM);

  Serial.print("Rezhim pin D5=");
  Serial.print(rezhim);
  Serial.println(rezhim == UROVEN_STA ? " (UseExistedWiFi)" : " (StandAlone)");

  if (rezhim == UROVEN_STA) {
    WiFi.mode(WIFI_STA);
    WiFi.hostname(config.deviceName);
    WiFi.begin(config.wifiSsid, config.wifiPass);

    Serial.print("STA wifi: ");
    Serial.println(config.wifiSsid);
    unsigned long startConnect = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startConnect < 20000UL) {
      delay(250);
      Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      return;
    }

    Serial.println("STA ne podklyuchilsya, zapuskayu AP.");
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("AP: ");
  Serial.println(AP_SSID);
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());
}

void nastroitVesy() {
  vesy.begin(PIN_HX711_DT, PIN_HX711_SCK);
  vesy.set_scale(config.calibrationScale);
  vesy.set_offset(config.calibrationOffset);
  if (HX711_TARE_PRI_STARTE && vesy.is_ready()) {
    vesy.tare();
    config.calibrationOffset = vesy.get_offset();
    sohranitKonfiguraciyu();
  }
}

void obrabotatOtladku() {
  unsigned long seychas = millis();
  if (seychas - poslednyayaOtladkaMs < PERIOD_OTLADKI_MS) {
    return;
  }
  poslednyayaOtladkaMs = seychas;
  Serial.print("ves=");
  Serial.print(tekushiyVesKg, 1);
  Serial.print(" kg, max=");
  Serial.print(maxTekushiyKg, 1);
  Serial.print(" kg, prev=");
  Serial.print(maxProshliyKg, 1);
  Serial.println(" kg");
}

void obrabotatPerezagruzkuWdt() {
  if (!zaprosPerezagruzkiWdt) {
    return;
  }
  unsigned long proshloSec = (millis() - startPerezagruzkiWdtMs) / 1000UL;
  if (proshloSec >= config.watchdogSec) {
    // delay()/yield() кормят watchdog на ESP8266, поэтому намеренно зависаем.
    while (true) {
    }
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  startMs = millis();

  zagruzitKonfiguraciyu();
  nastroitVesy();
  zapustitWifi();
  nastroitMarshruty();

  server.begin();
  webSocket.begin();
  webSocket.onEvent(obrabotatWs);

  Serial.println("HTTP server gotov.");
  Serial.println("WebSocket gotov.");
}

void loop() {
  server.handleClient();
  webSocket.loop();
  obrabotatVesy();
  otpravitWsStatus();
  obrabotatOtladku();
  obrabotatPerezagruzkuWdt();
  (void)startMs;
  (void)schetchikWs;
}
