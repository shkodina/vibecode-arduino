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
.banner.show.ok {
  background: #123a22;
  border-color: #2f8f5b;
  color: #d4ffe4;
}
.banner.show.info {
  background: #2a1c0c;
  border-color: var(--orange-dim);
  color: #ffe0c2;
}
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
.alarm-title strong.armed { color: var(--orange); }
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

  <h2>Будильники</h2>
  <div id="alarms"></div>

  <h2>Таймер</h2>
  <div class="card" id="timerCard"></div>

  <h2>Тест свечения</h2>
  <div class="card" id="testCard"></div>

  <h2>WiFi</h2>
  <div class="card" id="wifiCard"></div>

  <h2>Watchdog</h2>
  <div class="card" id="watchdogCard"></div>
  <div class="row">
    <button type="button" onclick="saveAll()">Сохранить настройки</button>
  </div>
</section>
<script>
var DAYS = ["Пн","Вт","Ср","Чт","Пт","Сб","Вс"];
var settings = null;
var bannerHoldUntil = 0;
var busy = false;

function showBanner(text, kind, holdMs) {
  var el = document.getElementById("banner");
  if (!text) {
    el.className = "banner";
    el.textContent = "";
    return;
  }
  kind = kind || "error";
  el.className = "banner show " + (kind === "ok" ? "ok" : (kind === "info" ? "info" : ""));
  el.textContent = text;
  if (holdMs == null) {
    holdMs = (kind === "ok" || kind === "info") ? 4000 : (kind === "error" ? 8000 : 0);
  }
  bannerHoldUntil = holdMs > 0 ? Date.now() + holdMs : 0;
}

function setBusy(on, label) {
  busy = !!on;
  var buttons = document.querySelectorAll("button");
  for (var i = 0; i < buttons.length; i++) {
    buttons[i].disabled = busy;
  }
  if (busy && label) {
    showBanner(label, "info");
  }
}

function modeFields(prefix, mode) {
  mode = mode || {type:"ramp", startBrightness:5, finishBrightness:100, rampSeconds:1200, glowSeconds:0, fadeSeconds:0, darkSeconds:0, totalSeconds:1800};
  var isStrobe = mode.type === "strobe";
  return '' +
    '<div class="row">' +
      '<div class="field"><label>Режим</label><select id="'+prefix+'_type" onchange="syncModeFields(\''+prefix+'\')">' +
        '<option value="ramp"'+(mode.type==="ramp"?" selected":"")+'>ramp</option>' +
        '<option value="pulse"'+(mode.type==="pulse"?" selected":"")+'>pulse</option>' +
        '<option value="strobe"'+(isStrobe?" selected":"")+'>strobe</option>' +
      '</select></div>' +
      '<div class="field"><label>Всего, сек</label><input type="number" id="'+prefix+'_total" min="1" value="'+mode.totalSeconds+'"></div>' +
    '</div>' +
    '<div class="muted" id="'+prefix+'_strobeHint" style="margin-top:8px;display:'+(isStrobe?"block":"none")+'">Стробоскоп: вспышка 100 мс / пауза 100 мс, яркость 100%. Меняется только длительность.</div>' +
    '<div class="row" id="'+prefix+'_extra" style="display:'+(isStrobe?"none":"flex")+';margin-top:10px">' +
      '<div class="field"><label>Яркость старта %</label><input type="number" id="'+prefix+'_start" min="0" max="100" value="'+(mode.startBrightness != null ? mode.startBrightness : 5)+'"></div>' +
      '<div class="field"><label>Яркость финиша %</label><input type="number" id="'+prefix+'_finish" min="0" max="100" value="'+(mode.finishBrightness != null ? mode.finishBrightness : 100)+'"></div>' +
      '<div class="field"><label>Розжиг, сек</label><input type="number" id="'+prefix+'_ramp" min="1" value="'+(mode.rampSeconds != null && mode.rampSeconds > 0 ? mode.rampSeconds : 1)+'"></div>' +
      '<div class="field"><label>Свечение, сек</label><input type="number" id="'+prefix+'_glow" min="0" value="'+(mode.glowSeconds != null ? mode.glowSeconds : 0)+'"></div>' +
      '<div class="field"><label>Затухание, сек</label><input type="number" id="'+prefix+'_fade" min="0" value="'+(mode.fadeSeconds != null ? mode.fadeSeconds : 0)+'"></div>' +
      '<div class="field"><label>Темнота, сек</label><input type="number" id="'+prefix+'_dark" min="0" value="'+(mode.darkSeconds != null ? mode.darkSeconds : 0)+'"></div>' +
    '</div>';
}

function syncModeFields(prefix) {
  var typeEl = document.getElementById(prefix+"_type");
  var extra = document.getElementById(prefix+"_extra");
  var hint = document.getElementById(prefix+"_strobeHint");
  if (!typeEl) return;
  var strobe = typeEl.value === "strobe";
  if (extra) extra.style.display = strobe ? "none" : "flex";
  if (hint) hint.style.display = strobe ? "block" : "none";
}

function readMode(prefix) {
  return {
    type: document.getElementById(prefix+"_type").value,
    startBrightness: Number(document.getElementById(prefix+"_start").value),
    finishBrightness: Number(document.getElementById(prefix+"_finish").value),
    rampSeconds: Number(document.getElementById(prefix+"_ramp").value),
    glowSeconds: Number(document.getElementById(prefix+"_glow").value),
    fadeSeconds: Number(document.getElementById(prefix+"_fade").value),
    darkSeconds: Number(document.getElementById(prefix+"_dark").value),
    totalSeconds: Number(document.getElementById(prefix+"_total").value)
  };
}

function validateMode(mode) {
  if (mode.totalSeconds <= 0) return "длительности должны быть > 0";
  if (mode.type === "strobe") {
    mode.startBrightness = 100;
    mode.finishBrightness = 100;
    mode.rampSeconds = 0;
    mode.glowSeconds = 0;
    mode.fadeSeconds = 0;
    mode.darkSeconds = 0;
    return "";
  }
  if (mode.startBrightness < 0 || mode.startBrightness > 100) return "яркость старта 0..100";
  if (mode.finishBrightness < 0 || mode.finishBrightness > 100) return "яркость финиша 0..100";
  if (mode.finishBrightness < mode.startBrightness) return "финиш меньше старта";
  if (mode.rampSeconds <= 0) return "длительности должны быть > 0";
  if (mode.glowSeconds < 0 || mode.fadeSeconds < 0 || mode.darkSeconds < 0) return "длительности не могут быть < 0";
  if (mode.type === "pulse" && mode.darkSeconds <= 0) return "для pulse темнота > 0";
  if (mode.type === "ramp") {
    mode.glowSeconds = 0;
    mode.fadeSeconds = 0;
    mode.darkSeconds = 0;
  }
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

function renderWatchdog() {
  var sec = settings && settings.watchdogSeconds != null ? settings.watchdogSeconds : 30;
  document.getElementById("watchdogCard").innerHTML =
    '<div class="row">' +
      '<div class="field"><label>Таймаут, сек</label><input type="number" id="wdt_sec" min="5" max="120" value="'+sec+'"></div>' +
    '</div>' +
    '<div class="muted" style="margin-top:8px">Если прошивка зависнет дольше этого времени, контроллер перезагрузится. Заводское значение 30 с. Пишется в NVS вместе с настройками.</div>';
}

function renderAlarms() {
  var root = document.getElementById("alarms");
  root.innerHTML = "";
  settings.alarms.forEach(function(alarm) {
    var card = document.createElement("div");
    card.className = "card";
    card.innerHTML =
      '<div class="alarm-title"><strong id="a'+alarm.id+'_title" class="'+(alarm.enabled?"armed":"")+'">Будильник '+alarm.id+'</strong>' +
      '<label><input type="checkbox" id="a'+alarm.id+'_en"'+(alarm.enabled?" checked":"")+' onchange="syncAlarmArmed('+alarm.id+')"> включён</label></div>' +
      '<div class="row">' +
        '<div class="field"><label>Часы</label><input type="number" id="a'+alarm.id+'_h" min="0" max="23" value="'+alarm.hour+'"></div>' +
        '<div class="field"><label>Минуты</label><input type="number" id="a'+alarm.id+'_m" min="0" max="59" value="'+alarm.minute+'"></div>' +
      '</div>' +
      '<div style="margin:10px 0 6px" class="muted">Дни недели</div>' + daysHtml(alarm.id, alarm.weekdaysMask) +
      modeFields("a"+alarm.id, alarm.mode) +
      '<div class="row" style="margin-top:12px">' +
        '<button type="button" onclick="saveAlarm('+alarm.id+')">Сохранить будильник '+alarm.id+'</button>' +
      '</div>';
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
    modeFields("x", {type:"strobe", startBrightness:100, finishBrightness:100, rampSeconds:0, glowSeconds:0, fadeSeconds:0, darkSeconds:0, totalSeconds:10}) +
    '<div class="muted" style="margin-top:8px">PWM на GPIO'+(settings && settings.pwmPin ? settings.pwmPin : 4)+' (шёлк «4»). При тесте должен мигать и синий LED на плате.</div>' +
    '<div class="row" style="margin-top:12px">' +
      '<button type="button" onclick="startTest()">Старт</button>' +
      '<button type="button" class="secondary" onclick="stopRun()">Стоп</button>' +
    '</div>';
}

function collectAlarm(id) {
  var mode = readMode("a"+id);
  var err = validateMode(mode);
  if (err) throw new Error("Будильник "+id+": "+err);
  var hour = Number(document.getElementById("a"+id+"_h").value);
  var minute = Number(document.getElementById("a"+id+"_m").value);
  if (hour < 0 || hour > 23 || minute < 0 || minute > 59) throw new Error("Будильник "+id+": время");
  var mask = readWeekdays(id);
  if (mask > 127) throw new Error("Будильник "+id+": дни");
  return {
    id: id,
    enabled: document.getElementById("a"+id+"_en").checked,
    hour: hour,
    minute: minute,
    weekdaysMask: mask,
    mode: mode
  };
}

function collectSettings() {
  var ssid = document.getElementById("wifi_ssid").value;
  var password = document.getElementById("wifi_pass").value;
  if (!ssid || ssid.length > 32) throw new Error("WiFi SSID: 1..32 символов");
  if (password.length > 63) throw new Error("WiFi пароль: максимум 63 символа");

  var alarms = [];
  for (var i = 0; i < settings.alarms.length; i++) {
    alarms.push(collectAlarm(i));
  }
  var tMode = readMode("t");
  var tErr = validateMode(tMode);
  if (tErr) throw new Error("Таймер: "+tErr);
  var th = Number(document.getElementById("t_h").value);
  var tm = Number(document.getElementById("t_m").value);
  if (th < 0 || th > 23 || tm < 0 || tm > 59) throw new Error("Таймер: время ожидания");
  var wdt = Number(document.getElementById("wdt_sec").value);
  if (wdt < 5 || wdt > 120) throw new Error("Watchdog: 5..120 секунд");
  return {
    wifi: { ssid: ssid, password: password },
    alarms: alarms,
    timer: { hours: th, minutes: tm, mode: tMode },
    watchdogSeconds: wdt
  };
}

async function api(path, method, body) {
  var ctrl = new AbortController();
  var timer = setTimeout(function() { ctrl.abort(); }, 8000);
  var opts = { method: method || "GET", headers: {}, signal: ctrl.signal, cache: "no-store" };
  if (body !== undefined) {
    opts.headers["Content-Type"] = "application/json";
    opts.body = JSON.stringify(body);
  }
  try {
    var res = await fetch(path, opts);
    var text = await res.text();
    var data = {};
    try { data = text ? JSON.parse(text) : {}; } catch (e) {
      throw new Error("Ответ не JSON: HTTP " + res.status);
    }
    if (!res.ok || data.ok === false) {
      throw new Error((data && data.error) || ("HTTP "+res.status));
    }
    return data;
  } catch (e) {
    if (e && e.name === "AbortError") throw new Error("Таймаут " + path);
    throw e;
  } finally {
    clearTimeout(timer);
  }
}

async function loadSettings() {
  settings = await api("/api/settings");
  renderWifi();
  renderAlarms();
  renderTimer();
  renderTest();
  renderWatchdog();
}

async function saveAll() {
  try {
    setBusy(true, "Сохраняю настройки...");
    var body = collectSettings();
    await api("/api/settings", "POST", body);
    await loadSettings();
    showBanner("Настройки сохранены", "ok");
  } catch (e) {
    showBanner("Ошибка сохранения: " + (e.message || e), "error");
  } finally {
    setBusy(false);
  }
}

function syncAlarmArmed(id) {
  var en = document.getElementById("a"+id+"_en");
  var title = document.getElementById("a"+id+"_title");
  if (!title || !en) return;
  if (en.checked) title.classList.add("armed");
  else title.classList.remove("armed");
}

async function saveAlarm(id) {
  try {
    setBusy(true, "Сохраняю будильник "+id+"...");
    var body = collectAlarm(id);
    await api("/api/alarm", "POST", body);
    if (settings && settings.alarms && settings.alarms[id]) {
      settings.alarms[id] = body;
    }
    showBanner("Будильник "+id+" сохранён", "ok");
  } catch (e) {
    showBanner("Ошибка сохранения будильника: " + (e.message || e), "error");
  } finally {
    setBusy(false);
  }
}

function pad2(n) {
  return (n < 10 ? "0" : "") + n;
}

function formatEta(sec) {
  sec = Math.max(0, Number(sec) || 0);
  var d = Math.floor(sec / 86400);
  var h = Math.floor((sec % 86400) / 3600);
  var m = Math.floor((sec % 3600) / 60);
  var parts = [];
  if (d) parts.push(d + " д");
  if (h) parts.push(h + " ч");
  if (m) parts.push(m + " мин");
  if (parts.length === 0) return "меньше минуты";
  return parts.join(" ");
}

function paintNextAlarmBanner(st) {
  if (Date.now() < bannerHoldUntil) return;
  if (!st.ntpSynced) {
    showBanner("Нет синхронизации времени — ближайший будильник неизвестен", "info", 0);
    return;
  }
  if (!st.nextAlarm) {
    showBanner("Нет включённых будильников", "info", 0);
    return;
  }
  var n = st.nextAlarm;
  showBanner(
    "Ближайший: Будильник " + n.id + " в " + pad2(n.hour) + ":" + pad2(n.minute) +
    ", через " + formatEta(n.inSeconds),
    "info",
    0
  );
}

async function stopRun() {
  try {
    setBusy(true, "Останавливаю...");
    await api("/api/stop", "POST", {});
    showBanner("Остановлено", "ok");
    await refreshStatus(true);
  } catch (e) {
    showBanner("Ошибка стопа: " + (e.message || e), "error");
  } finally {
    setBusy(false);
  }
}

async function startTimer() {
  try {
    setBusy(true, "Сохраняю и запускаю таймер...");
    var body = collectSettings();
    await api("/api/settings", "POST", body);
    await api("/api/timer/start", "POST", {});
    showBanner("Таймер запущен", "ok");
    await refreshStatus(true);
  } catch (e) {
    showBanner("Ошибка таймера: " + (e.message || e), "error");
  } finally {
    setBusy(false);
  }
}

async function startTest() {
  try {
    setBusy(true, "Запускаю тест...");
    var mode = readMode("x");
    var err = validateMode(mode);
    if (err) throw new Error(err);
    await api("/api/test/start", "POST", mode);
    showBanner("Тест принят: " + mode.type + ", " + mode.totalSeconds + " с. Смотри синий LED и ленту.", "ok");
    await refreshStatus(true);
  } catch (e) {
    showBanner("Ошибка теста: " + (e.message || e), "error");
  } finally {
    setBusy(false);
  }
}

async function refreshStatus(forceKeepBanner) {
  try {
    var st = await api("/api/status");
    var wifi = st.wifiConnected ? '<span class="ok">WiFi ok</span>' : '<span class="bad">WiFi нет</span>';
    var ntp = st.ntpSynced ? '<span class="ok">NTP ok</span>' : '<span class="bad">NTP нет</span>';
    document.getElementById("statusMeta").innerHTML =
      st.currentTime + " · v" + st.firmwareVersion + " · " +
      (st.radioMode ? (st.radioMode + " · ") : "") +
      wifi + " · " + ntp +
      (st.wifiSsid ? (" · " + st.wifiSsid) : "") +
      " · PWM GPIO" + (st.pwmPin != null ? st.pwmPin : "?") +
      " · яркость " + (st.pwmBrightness != null ? st.pwmBrightness : 0) + "%";
    if (!st.activeRun) {
      document.getElementById("activeInfo").textContent = "нет";
    } else {
      var r = st.activeRun;
      var extra = r.type === "alarm" ? (", будильник " + r.alarmId) : "";
      document.getElementById("activeInfo").textContent =
        r.type + extra + ", режим " + r.mode + ", осталось " + r.remainingSeconds +
        " с, яркость " + r.brightness + "%";
    }
    paintNextAlarmBanner(st);
  } catch (e) {
    showBanner("Нет связи с устройством: " + (e.message || e), "error");
  }
}

loadSettings().then(function() { return refreshStatus(true); }).catch(function(e) {
  document.getElementById("statusMeta").textContent = "Ошибка загрузки: " + (e.message || e);
  showBanner(String(e.message || e), "error");
  renderTest();
});
setInterval(function() { if (!busy) refreshStatus(false); }, 2000);
</script>
</body>
</html>
)HTML";

#endif
