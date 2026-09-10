#ifndef AMBIENT_MORNING_ALARM_WEB_PAGE_H
#define AMBIENT_MORNING_ALARM_WEB_PAGE_H

#include <pgmspace.h>

const char WEB_PAGE_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Piper Light Alarm</title>
<style>
:root {
  --bg: #0b0b0c;
  --panel: #151518;
  --line: #2a2a2e;
  --text: #f3f1ec;
  --muted: #9a948a;
  --orange: #ff7a18;
  --orange-dim: #c45a10;
  --danger: #ff4d4d;
}
* { box-sizing: border-box; }
body {
  margin: 0;
  font-family: "Segoe UI", Tahoma, sans-serif;
  background: radial-gradient(circle at top, #1a120c 0%, var(--bg) 55%);
  color: var(--text);
  min-height: 100vh;
}
header, section, .banner {
  max-width: 920px;
  margin: 0 auto;
  padding: 16px;
}
header {
  border-bottom: 1px solid var(--line);
}
h1 {
  margin: 0 0 8px;
  color: var(--orange);
  font-size: 1.6rem;
  letter-spacing: 0.02em;
}
.meta, .muted { color: var(--muted); font-size: 0.95rem; }
.banner {
  display: none;
  background: #3a1212;
  border: 1px solid var(--danger);
  color: #ffd4d4;
  border-radius: 10px;
  margin-top: 12px;
}
.banner.show { display: block; }
.card {
  background: var(--panel);
  border: 1px solid var(--line);
  border-radius: 14px;
  padding: 14px;
  margin: 12px 0;
}
.row {
  display: flex;
  flex-wrap: wrap;
  gap: 10px;
  align-items: center;
}
label { display: block; font-size: 0.85rem; color: var(--muted); margin-bottom: 4px; }
input[type=number], select {
  width: 100%;
  min-width: 72px;
  background: #0f0f12;
  color: var(--text);
  border: 1px solid var(--line);
  border-radius: 8px;
  padding: 10px;
  font-size: 1rem;
}
.field { flex: 1 1 90px; }
.days { display: flex; flex-wrap: wrap; gap: 8px; }
.days label {
  display: flex;
  gap: 6px;
  align-items: center;
  color: var(--text);
  background: #101014;
  border: 1px solid var(--line);
  border-radius: 999px;
  padding: 8px 10px;
}
button {
  background: var(--orange);
  color: #140b04;
  border: 0;
  border-radius: 10px;
  padding: 12px 16px;
  font-weight: 700;
  font-size: 1rem;
  cursor: pointer;
}
button.secondary {
  background: transparent;
  color: var(--orange);
  border: 1px solid var(--orange-dim);
}
button:disabled { opacity: 0.5; }
h2 { margin: 18px 0 8px; font-size: 1.2rem; }
.alarm-title {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 10px;
}
.ok { color: #7dffb0; }
.bad { color: var(--danger); }
</style>
</head>
<body>
<header>
  <h1>Piper Light Alarm</h1>
  <div class="meta" id="statusMeta">Загрузка...</div>
</header>
<div id="banner" class="banner"></div>
<section>
  <div class="card" id="activeCard">
    <div class="muted">Активный запуск</div>
    <div id="activeInfo">нет</div>
    <div class="row" style="margin-top:12px">
      <button type="button" onclick="stopRun()">Стоп</button>
    </div>
  </div>

  <h2>WiFi</h2>
  <div class="card" id="wifiCard"></div>

  <h2>Будильники</h2>
  <div id="alarms"></div>
  <div class="row">
    <button type="button" onclick="saveAll()">Сохранить настройки</button>
  </div>

  <h2>Таймер</h2>
  <div class="card" id="timerCard"></div>

  <h2>Тест свечения</h2>
  <div class="card" id="testCard"></div>
</section>
<script>
var DAYS = ["Пн","Вт","Ср","Чт","Пт","Сб","Вс"];
var settings = null;

function showBanner(text) {
  var el = document.getElementById("banner");
  if (!text) {
    el.className = "banner";
    el.textContent = "";
    return;
  }
  el.className = "banner show";
  el.textContent = text;
}

function modeFields(prefix, mode) {
  mode = mode || {type:"ramp", startBrightness:5, finishBrightness:100, rampSeconds:1200, darkSeconds:0, totalSeconds:1800};
  return '' +
    '<div class="row">' +
      '<div class="field"><label>Режим</label><select id="'+prefix+'_type">' +
        '<option value="ramp"'+(mode.type==="ramp"?" selected":"")+'>ramp</option>' +
        '<option value="pulse"'+(mode.type==="pulse"?" selected":"")+'>pulse</option>' +
      '</select></div>' +
      '<div class="field"><label>Яркость старта %</label><input type="number" id="'+prefix+'_start" min="0" max="100" value="'+mode.startBrightness+'"></div>' +
      '<div class="field"><label>Яркость финиша %</label><input type="number" id="'+prefix+'_finish" min="0" max="100" value="'+mode.finishBrightness+'"></div>' +
      '<div class="field"><label>Розжиг, сек</label><input type="number" id="'+prefix+'_ramp" min="1" value="'+mode.rampSeconds+'"></div>' +
      '<div class="field"><label>Темнота, сек</label><input type="number" id="'+prefix+'_dark" min="0" value="'+mode.darkSeconds+'"></div>' +
      '<div class="field"><label>Всего, сек</label><input type="number" id="'+prefix+'_total" min="1" value="'+mode.totalSeconds+'"></div>' +
    '</div>';
}

function readMode(prefix) {
  return {
    type: document.getElementById(prefix+"_type").value,
    startBrightness: Number(document.getElementById(prefix+"_start").value),
    finishBrightness: Number(document.getElementById(prefix+"_finish").value),
    rampSeconds: Number(document.getElementById(prefix+"_ramp").value),
    darkSeconds: Number(document.getElementById(prefix+"_dark").value),
    totalSeconds: Number(document.getElementById(prefix+"_total").value)
  };
}

function validateMode(mode) {
  if (mode.startBrightness < 0 || mode.startBrightness > 100) return "яркость старта 0..100";
  if (mode.finishBrightness < 0 || mode.finishBrightness > 100) return "яркость финиша 0..100";
  if (mode.finishBrightness < mode.startBrightness) return "финиш меньше старта";
  if (mode.rampSeconds <= 0 || mode.totalSeconds <= 0) return "длительности должны быть > 0";
  if (mode.type === "pulse" && mode.darkSeconds <= 0) return "для pulse темнота > 0";
  if (mode.type === "ramp") mode.darkSeconds = 0;
  return "";
}

function daysHtml(id, mask) {
  var html = '<div class="days">';
  for (var i = 0; i < 7; i++) {
    var checked = (mask & (1 << i)) ? " checked" : "";
    html += '<label><input type="checkbox" id="a'+id+'_d'+i+'"'+checked+'>'+DAYS[i]+'</label>';
  }
  html += '</div>';
  return html;
}

function readWeekdays(id) {
  var mask = 0;
  for (var i = 0; i < 7; i++) {
    if (document.getElementById("a"+id+"_d"+i).checked) mask |= (1 << i);
  }
  return mask;
}

function renderWifi() {
  var wifi = settings.wifi || {ssid:"", password:""};
  document.getElementById("wifiCard").innerHTML =
    '<div class="row">' +
      '<div class="field" style="flex:2"><label>SSID</label><input type="text" id="wifi_ssid" maxlength="32" value="'+(wifi.ssid||"")+'"></div>' +
      '<div class="field" style="flex:2"><label>Пароль</label><input type="text" id="wifi_pass" maxlength="63" value="'+(wifi.password||"")+'"></div>' +
    '</div>' +
    '<div class="muted" style="margin-top:8px">Сохраняется в энергонезависимую память. После смены устройство переподключится к сети.</div>';
}

function renderAlarms() {
  var root = document.getElementById("alarms");
  root.innerHTML = "";
  settings.alarms.forEach(function(alarm) {
    var card = document.createElement("div");
    card.className = "card";
    card.innerHTML =
      '<div class="alarm-title"><strong>Будильник '+alarm.id+'</strong>' +
      '<label><input type="checkbox" id="a'+alarm.id+'_en"'+(alarm.enabled?" checked":"")+'> включён</label></div>' +
      '<div class="row">' +
        '<div class="field"><label>Часы</label><input type="number" id="a'+alarm.id+'_h" min="0" max="23" value="'+alarm.hour+'"></div>' +
        '<div class="field"><label>Минуты</label><input type="number" id="a'+alarm.id+'_m" min="0" max="59" value="'+alarm.minute+'"></div>' +
      '</div>' +
      '<div style="margin:10px 0 6px" class="muted">Дни недели</div>' + daysHtml(alarm.id, alarm.weekdaysMask) +
      modeFields("a"+alarm.id, alarm.mode);
    root.appendChild(card);
  });
}

function renderTimer() {
  var t = settings.timer;
  document.getElementById("timerCard").innerHTML =
    '<div class="row">' +
      '<div class="field"><label>Часы ожидания</label><input type="number" id="t_h" min="0" max="23" value="'+t.hours+'"></div>' +
      '<div class="field"><label>Минуты ожидания</label><input type="number" id="t_m" min="0" max="59" value="'+t.minutes+'"></div>' +
    '</div>' +
    modeFields("t", t.mode) +
    '<div class="row" style="margin-top:12px">' +
      '<button type="button" onclick="startTimer()">Запустить таймер</button>' +
      '<button type="button" class="secondary" onclick="stopRun()">Стоп</button>' +
    '</div>';
}

function renderTest() {
  document.getElementById("testCard").innerHTML =
    modeFields("x", {type:"pulse", startBrightness:5, finishBrightness:100, rampSeconds:5, darkSeconds:5, totalSeconds:60}) +
    '<div class="row" style="margin-top:12px">' +
      '<button type="button" onclick="startTest()">Старт</button>' +
      '<button type="button" class="secondary" onclick="stopRun()">Стоп</button>' +
    '</div>';
}

function collectSettings() {
  var ssid = document.getElementById("wifi_ssid").value;
  var password = document.getElementById("wifi_pass").value;
  if (!ssid || ssid.length > 32) throw new Error("WiFi SSID: 1..32 символов");
  if (password.length > 63) throw new Error("WiFi пароль: максимум 63 символа");

  var alarms = [];
  for (var i = 0; i < settings.alarms.length; i++) {
    var mode = readMode("a"+i);
    var err = validateMode(mode);
    if (err) throw new Error("Будильник "+i+": "+err);
    var hour = Number(document.getElementById("a"+i+"_h").value);
    var minute = Number(document.getElementById("a"+i+"_m").value);
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) throw new Error("Будильник "+i+": время");
    var mask = readWeekdays(i);
    if (mask > 127) throw new Error("Будильник "+i+": дни");
    alarms.push({
      id: i,
      enabled: document.getElementById("a"+i+"_en").checked,
      hour: hour,
      minute: minute,
      weekdaysMask: mask,
      mode: mode
    });
  }
  var tMode = readMode("t");
  var tErr = validateMode(tMode);
  if (tErr) throw new Error("Таймер: "+tErr);
  var th = Number(document.getElementById("t_h").value);
  var tm = Number(document.getElementById("t_m").value);
  if (th < 0 || th > 23 || tm < 0 || tm > 59) throw new Error("Таймер: время ожидания");
  return {
    wifi: { ssid: ssid, password: password },
    alarms: alarms,
    timer: { hours: th, minutes: tm, mode: tMode }
  };
}

async function api(path, method, body) {
  var opts = { method: method || "GET", headers: {} };
  if (body !== undefined) {
    opts.headers["Content-Type"] = "application/json";
    opts.body = JSON.stringify(body);
  }
  var res = await fetch(path, opts);
  var data = await res.json();
  if (!res.ok || data.ok === false) {
    throw new Error((data && data.error) || ("HTTP "+res.status));
  }
  return data;
}

async function loadSettings() {
  settings = await api("/api/settings");
  renderWifi();
  renderAlarms();
  renderTimer();
  renderTest();
  showBanner("");
}

async function saveAll() {
  try {
    var body = collectSettings();
    await api("/api/settings", "POST", body);
    await loadSettings();
    showBanner("");
  } catch (e) {
    showBanner(String(e.message || e));
  }
}

async function stopRun() {
  try {
    await api("/api/stop", "POST", {});
  } catch (e) {
    showBanner(String(e.message || e));
  }
}

async function startTimer() {
  try {
    await saveAll();
    await api("/api/timer/start", "POST", {});
  } catch (e) {
    showBanner(String(e.message || e));
  }
}

async function startTest() {
  try {
    var mode = readMode("x");
    var err = validateMode(mode);
    if (err) throw new Error(err);
    await api("/api/test/start", "POST", mode);
  } catch (e) {
    showBanner(String(e.message || e));
  }
}

async function refreshStatus() {
  try {
    var st = await api("/api/status");
    var wifi = st.wifiConnected ? '<span class="ok">WiFi ok</span>' : '<span class="bad">WiFi нет</span>';
    var ntp = st.ntpSynced ? '<span class="ok">NTP ok</span>' : '<span class="bad">NTP нет</span>';
    document.getElementById("statusMeta").innerHTML =
      st.currentTime + " · v" + st.firmwareVersion + " · " + wifi + " · " + ntp +
      (st.wifiSsid ? (" · " + st.wifiSsid) : "");
    if (!st.activeRun) {
      document.getElementById("activeInfo").textContent = "нет";
    } else {
      var r = st.activeRun;
      var extra = r.type === "alarm" ? (", будильник " + r.alarmId) : "";
      document.getElementById("activeInfo").textContent =
        r.type + extra + ", режим " + r.mode + ", осталось " + r.remainingSeconds +
        " с, яркость " + r.brightness + "%";
    }
    showBanner("");
  } catch (e) {
    showBanner("Нет связи с устройством: " + (e.message || e));
  }
}

loadSettings().then(refreshStatus).catch(function(e) {
  showBanner(String(e.message || e));
  renderTest();
});
setInterval(refreshStatus, 2000);
</script>
</body>
</html>
)HTML";

#endif
