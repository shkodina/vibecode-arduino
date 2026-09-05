const $ = (id) => document.getElementById(id);

const TRANSLIT = {
  а: "a", б: "b", в: "v", г: "g", д: "d", е: "e", ё: "yo", ж: "zh", з: "z",
  и: "i", й: "y", к: "k", л: "l", м: "m", н: "n", о: "o", п: "p", р: "r",
  с: "s", т: "t", у: "u", ф: "f", х: "h", ц: "ts", ч: "ch", ш: "sh", щ: "sch",
  ъ: "", ы: "y", ь: "", э: "e", ю: "yu", я: "ya",
};

function moduleFilename(original) {
  const stem = String(original || "").replace(/\.[^.]+$/, "");
  let out = "";
  for (const ch of stem.toLowerCase().replace(/ /g, "-")) {
    if (Object.prototype.hasOwnProperty.call(TRANSLIT, ch)) out += TRANSLIT[ch];
    else if (/[a-z0-9_-]/.test(ch)) out += ch;
    else out += "-";
  }
  const slug = out.replace(/[-_]+/g, "-").replace(/^-|-$/g, "");
  return `${slug || "sound"}.wav`;
}

let selectedHost = null;
let lastConfig = null;
let selectedFile = null;
let convertedBlob = null;
let analysis = null;
let pollTimer = null;

async function api(url, opts = {}) {
  const response = await fetch(url, opts);
  const text = await response.text();
  let data = text;
  try { data = text ? JSON.parse(text) : ""; } catch (_) {}
  if (!response.ok) {
    const detail = (data && data.detail) || text || response.statusText;
    throw new Error(detail);
  }
  return data;
}

let selectedDir = "/";
let treeCache = {};
let ignoreFileChange = false;

function joinPath(dir, name) {
  const short = String(name || "").replace(/^.*\//, "").replace(/\/$/, "");
  if (!short || short === "." || short === "..") return null;
  if (dir === "/") return `/${short}`;
  return `${dir.replace(/\/$/, "")}/${short}`;
}

function flash(msg) {
  const el = $("flash");
  el.hidden = !msg;
  el.textContent = msg || "";
}

function setAnalyze(kind, text) {
  const box = $("analyze");
  box.hidden = false;
  box.className = "analyze " + kind;
  box.textContent = text;
}

function errorText(raw) {
  if (raw == null) return "неизвестная ошибка";
  if (raw instanceof Error) return errorText(raw.message);
  const text = String(raw);
  try {
    const parsed = JSON.parse(text);
    if (parsed && parsed.detail) {
      return typeof parsed.detail === "string" ? parsed.detail : JSON.stringify(parsed.detail);
    }
  } catch (_) {}
  return text;
}

function putFileInPicker(file) {
  ignoreFileChange = true;
  try {
    const dt = new DataTransfer();
    dt.items.add(file);
    $("upfile").files = dt.files;
  } catch (_) {}
  selectedFile = file;
  setTimeout(() => { ignoreFileChange = false; }, 50);
}

function periodCard(p, idx) {
  const wrap = document.createElement("div");
  wrap.className = "period";
  wrap.dataset.idx = String(idx);
  wrap.innerHTML = `
    <label>С <input class="p-start" value="${p.start || "00:00"}"></label>
    <label>До <input class="p-end" value="${p.end || "24:00"}"></label>
    <label>Папка <input class="p-dir" value="${p.directory || "/birds_sounds"}"></label>
    <label>Громкость <input class="p-vol" type="number" min="0" max="100" value="${p.volume ?? 100}"></label>
    <div class="checks">
      <label><input class="p-rand" type="checkbox" ${p.random_on_startup ? "checked" : ""}> случайный при PIR</label>
      <label><input class="p-shuf" type="checkbox" ${p.shuffle ? "checked" : ""}> shuffle</label>
      <label><input class="p-rep" type="checkbox" ${p.repeat_selected ? "checked" : ""}> крутить один файл</label>
      <label><input class="p-loop" type="checkbox" ${p.loop_directory ? "checked" : ""}> цикл папки</label>
    </div>`;
  return wrap;
}

function readSchedule() {
  return [...document.querySelectorAll(".period")].map((el) => ({
    start: el.querySelector(".p-start").value,
    end: el.querySelector(".p-end").value,
    directory: el.querySelector(".p-dir").value,
    volume: Number(el.querySelector(".p-vol").value),
    random_on_startup: el.querySelector(".p-rand").checked,
    shuffle: el.querySelector(".p-shuf").checked,
    repeat_selected: el.querySelector(".p-rep").checked,
    loop_directory: el.querySelector(".p-loop").checked,
  }));
}

function renderLeds(s) {
  $("d-leds").innerHTML = `
    <span class="led ${s.playing ? "hot" : ""}"><span class="dot ${s.playing ? "play" : ""}"></span>играет</span>
    <span class="led ${s.motion ? "hot" : ""}"><span class="dot ${s.motion ? "motion" : ""}"></span>движение</span>
    <span class="led ${s.sd_ok ? "hot" : ""}"><span class="dot ${s.sd_ok ? "on" : ""}"></span>SD</span>
    <span class="led ${s.wifi_sta ? "hot" : ""}"><span class="dot ${s.wifi_sta ? "on" : ""}"></span>STA</span>
    <span class="led"><span class="dot ${s.time_ok ? "on" : ""}"></span>NTP</span>`;
}

function renderStatus(s) {
  $("d-ip").textContent = selectedHost;
  $("d-live").textContent =
    `время ${s.time || "—"} · файл ${s.file || "—"} · папка ${s.directory || "—"} · громкость ${s.volume ?? "—"}`;
  renderLeds(s);
  if (typeof s.volume === "number") {
    $("live-vol").value = s.volume;
    $("live-vol-n").textContent = s.volume;
  }
}

function renderConfig(c) {
  lastConfig = c;
  $("ssid").value = c.wifi?.ssid || "";
  $("pass").value = c.wifi?.password || "";
  $("ntp").value = c.ntp?.server || "";
  $("tz").value = c.ntp?.timezone_offset ?? 3;
  $("ntpint").value = c.ntp?.update_interval ?? 3600;
  $("mtime").value = c.motion?.timeout_seconds ?? 30;
  $("mcool").value = c.motion?.cooldown_seconds ?? 5;
  const box = $("schedule");
  box.innerHTML = "";
  (c.playback?.schedule || []).forEach((p, i) => box.appendChild(periodCard(p, i)));
}

function isDir(entry) {
  return entry.type === "dir" || entry.dir === true;
}

function parseListing(path, data) {
  const entries = data.entries || data.files || [];
  const dirs = [];
  const files = [];
  for (const entry of entries) {
    const full = String(entry.name || "").startsWith("/")
      ? String(entry.name)
      : joinPath(path, entry.name);
    if (!full) continue;
    const short = full.replace(/^.*\//, "");
    if (isDir(entry)) {
      if (short.includes(" ")) continue;
      dirs.push({ name: short, path: full });
    }
    else files.push({ name: short, path: full, size: entry.size || 0 });
  }
  dirs.sort((a, b) => a.name.localeCompare(b.name));
  files.sort((a, b) => a.name.localeCompare(b.name));
  return { dirs, files };
}

async function fetchListing(path) {
  try {
    return parseListing(path, await api(`/api/modules/${selectedHost}/files?path=${encodeURIComponent(path)}`));
  } catch (err) {
    if (path !== "/") throw err;
    const guesses = new Set(["/data", "/water", "/birds_sounds"]);
    (lastConfig?.playback?.schedule || []).forEach((p) => {
      if (p.directory) guesses.add(p.directory.replace(/\/$/, "") || "/");
    });
    const dirs = [];
    for (const guess of guesses) {
      if (guess === "/") continue;
      try {
        await api(`/api/modules/${selectedHost}/files?path=${encodeURIComponent(guess)}`);
        dirs.push({ name: guess.replace(/^\//, ""), path: guess });
      } catch (_) {}
    }
    dirs.sort((a, b) => a.name.localeCompare(b.name));
    return { dirs, files: [] };
  }
}

async function walkTree(path, depth) {
  const node = await fetchListing(path);
  treeCache[path] = node;
  if (depth >= 6) return;
  for (const dir of node.dirs) {
    try {
      await walkTree(dir.path, depth + 1);
    } catch (_) {
      treeCache[dir.path] = { dirs: [], files: [] };
    }
  }
}

function renderTree() {
  const el = $("file-tree");
  if (!el) return;
  el.innerHTML = "";
  const add = (path, name, depth) => {
    const btn = document.createElement("button");
    btn.type = "button";
    btn.className = "tree-item" + (path === selectedDir ? " active" : "");
    btn.style.paddingLeft = `${8 + depth * 12}px`;
    btn.textContent = path === "/" ? "/" : name;
    btn.onclick = () => selectFolder(path);
    el.appendChild(btn);
    (treeCache[path]?.dirs || []).forEach((d) => add(d.path, d.name, depth + 1));
  };
  add("/", "/", 0);
}

function renderFolder() {
  $("dir-path").value = selectedDir;
  const box = $("files");
  box.innerHTML = "";
  const node = treeCache[selectedDir] || { dirs: [], files: [] };
  if (selectedDir !== "/") {
    const up = document.createElement("div");
    up.className = "file-row";
    const parent = selectedDir.replace(/\/[^/]+$/, "") || "/";
    up.innerHTML = "<span>📁 ..</span>";
    const btn = document.createElement("button");
    btn.className = "ghost";
    btn.textContent = "вверх";
    btn.onclick = () => selectFolder(parent);
    up.appendChild(btn);
    box.appendChild(up);
  }
  node.dirs.forEach((d) => {
    const row = document.createElement("div");
    row.className = "file-row";
    row.innerHTML = `<span>📁 ${d.name}/</span>`;
    const open = document.createElement("button");
    open.className = "ghost";
    open.textContent = "открыть";
    open.onclick = () => selectFolder(d.path);
    row.appendChild(open);
    box.appendChild(row);
  });
  node.files.forEach((f) => {
    const row = document.createElement("div");
    row.className = "file-row";
    row.innerHTML = `<span>♪ ${f.name} (${f.size} б)</span>`;
    const del = document.createElement("button");
    del.className = "danger";
    del.textContent = "удалить";
    del.onclick = async () => {
      if (!confirm(`Удалить ${f.path}?`)) return;
      try {
        flash(await api(`/api/modules/${selectedHost}/delete?path=${encodeURIComponent(f.path)}`, { method: "DELETE" }));
        await loadFiles();
      } catch (e) { flash(e.message); }
    };
    row.appendChild(del);
    box.appendChild(row);
  });
  if (!node.dirs.length && !node.files.length) {
    const empty = document.createElement("div");
    empty.className = "file-row";
    empty.textContent = "пусто";
    box.appendChild(empty);
  }
}

function selectFolder(path) {
  selectedDir = path || "/";
  $("dir-path").value = selectedDir;
  renderTree();
  renderFolder();
}

async function loadFiles(fromRoot) {
  if (!selectedHost) return;
  if (fromRoot) selectedDir = "/";
  treeCache = {};
  $("files").textContent = "загрузка…";
  try {
    await walkTree("/", 0);
  } catch (e) {
    flash(e.message);
    treeCache["/"] = { dirs: [], files: [] };
  }
  if (!treeCache[selectedDir]) selectedDir = "/";
  renderTree();
  renderFolder();
}

async function refreshStatus() {
  if (!selectedHost) return;
  try {
    renderStatus(await api(`/api/modules/${selectedHost}/status`));
  } catch (_) {}
}

async function openModule(host, status) {
  selectedHost = host;
  $("empty").classList.add("hidden");
  $("detail").classList.remove("hidden");
  document.querySelectorAll(".card").forEach((c) => c.classList.toggle("active", c.dataset.host === host));
  renderStatus(status);
  selectedDir = "/";
  $("dir-path").value = "/";
  flash("");
  convertedBlob = null;
  try {
    renderConfig(await api(`/api/modules/${host}/config`));
    await loadFiles(true);
  } catch (e) {
    flash(e.message);
  }
  clearInterval(pollTimer);
  pollTimer = setInterval(refreshStatus, 4000);
}

function renderModules(modules) {
  const list = $("module-list");
  list.innerHTML = "";
  $("count").textContent = String(modules.length);
  if (!modules.length) {
    $("scan-note").textContent = "Модули lolin-wc-sounds в пуле не найдены. Проверьте подсеть в config.yaml.";
    return;
  }
  $("scan-note").textContent = "Клик по модулю открывает его настройки. Серая лампочка — сейчас недоступен.";
  modules.forEach((m) => {
    const online = m.online !== false;
    const s = m.status || {};
    const btn = document.createElement("button");
    btn.className = "card" + (online ? "" : " offline");
    btn.dataset.host = m.host;
    const meta = online
      ? `<span class="dot ${s.playing ? "play" : ""}"></span>${s.playing ? "играет" : "тишина"} · vol ${s.volume ?? "—"} · ${s.file || "нет файла"}`
      : "недоступен";
    btn.innerHTML = `
      <span class="lamp ${online ? "" : "off"}" aria-hidden="true"></span>
      <div class="body">
        <p class="ip">${m.host}</p>
        <p class="meta">${meta}</p>
      </div>`;
    btn.onclick = () => {
      if (!online) {
        flash("модуль недоступен");
        return;
      }
      openModule(m.host, s);
    };
    list.appendChild(btn);
  });
}

$("scan-btn").onclick = async () => {
  const btn = $("scan-btn");
  btn.disabled = true;
  btn.textContent = "Сканирую…";
  try {
    const data = await api("/api/scan", { method: "POST" });
    renderModules(data.modules || []);
    $("scan-note").textContent =
      `Сохранено в config.yaml: ${data.saved ?? (data.modules || []).length}. Опрошено адресов: ${data.scanned}.`;
  } catch (e) {
    $("scan-note").textContent = e.message;
  } finally {
    btn.disabled = false;
    btn.textContent = "Сканировать сеть";
  }
};

$("live-vol").oninput = () => { $("live-vol-n").textContent = $("live-vol").value; };

document.querySelector(".toolbar").addEventListener("click", async (ev) => {
  const act = ev.target.dataset?.act;
  if (!act || !selectedHost) return;
  try {
    if (act === "play") flash(await api(`/api/modules/${selectedHost}/play`, { method: "POST" }));
    if (act === "stop") flash(await api(`/api/modules/${selectedHost}/stop`, { method: "POST" }));
    if (act === "volume") {
      const v = $("live-vol").value;
      flash(await api(`/api/modules/${selectedHost}/volume?value=${encodeURIComponent(v)}`, { method: "POST" }));
    }
    if (act === "reload") {
      flash(await api(`/api/modules/${selectedHost}/reload`, { method: "POST" }));
      renderConfig(await api(`/api/modules/${selectedHost}/config`));
    }
    if (act === "reboot") {
      if (!confirm("Перезагрузить модуль через watchdog?")) return;
      flash("модуль уходит в reboot…");
      try { await api(`/api/modules/${selectedHost}/reboot`, { method: "POST" }); } catch (_) {}
      setTimeout(refreshStatus, 8000);
    }
    refreshStatus();
  } catch (e) { flash(e.message); }
});

$("save-cfg").onclick = async () => {
  if (!selectedHost || !lastConfig) return;
  const body = {
    ...lastConfig,
    wifi: { ssid: $("ssid").value, password: $("pass").value },
    ntp: {
      server: $("ntp").value,
      timezone_offset: Number($("tz").value),
      update_interval: Number($("ntpint").value),
    },
    motion: {
      timeout_seconds: Number($("mtime").value),
      cooldown_seconds: Number($("mcool").value),
    },
    playback: { ...(lastConfig.playback || {}), schedule: readSchedule() },
  };
  try {
    const fd = await fetch(`/api/modules/${selectedHost}/config`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body),
    });
    flash(await fd.text());
  } catch (e) { flash(e.message); }
};

$("list-files").onclick = () => loadFiles(true).catch((e) => flash(e.message));
$("mkdir-btn").onclick = async () => {
  const name = (prompt("Имя новой папки в " + selectedDir + " (латиница, без пробелов)") || "").trim().toLowerCase();
  if (!name) return;
  const path = joinPath(selectedDir, name);
  if (!path) { flash("плохое имя папки"); return; }
  try {
    flash(await api(`/api/modules/${selectedHost}/mkdir?path=${encodeURIComponent(path)}`, { method: "POST" }));
    await loadFiles();
    selectFolder(selectedDir);
  } catch (e) { flash(e.message); }
};

$("upfile").onchange = async (ev) => {
  if (ignoreFileChange) return;
  selectedFile = ev.target.files[0] || null;
  convertedBlob = null;
  analysis = null;
  $("convert-btn").hidden = true;
  const box = $("analyze");
  if (!selectedFile) { box.hidden = true; return; }
  const fd = new FormData();
  fd.append("file", selectedFile, selectedFile.name);
  try {
    analysis = await api("/api/analyze", { method: "POST", body: fd });
    box.hidden = false;
    if (analysis.needs_conversion) {
      box.className = "analyze need";
      box.textContent = `${analysis.reason}. Можно конвертировать через ffmpeg в WAV PCM 16-bit 16 kHz mono.`;
      $("convert-btn").hidden = false;
    } else {
      box.className = "analyze ok";
      box.textContent = analysis.reason;
    }
    const filename = analysis.module_filename || moduleFilename(selectedFile.name);
    const dir = selectedDir || "/";
    $("uppath").value = `${dir === "/" ? "" : dir}/${filename}`;
  } catch (e) {
    box.hidden = false;
    box.className = "analyze need";
    box.textContent = e.message;
  }
};

$("convert-btn").onclick = async () => {
  if (!selectedFile) return;
  const srcName = selectedFile.name;
  const wavName = moduleFilename(srcName);
  const btn = $("convert-btn");
  btn.disabled = true;
  $("upload-btn").disabled = true;
  btn.textContent = "Конвертирую…";
  setAnalyze("busy", `Конвертирую «${srcName}» через ffmpeg в WAV PCM 16-bit 16 kHz mono…`);
  flash("идёт конвертация…");
  try {
    const fd = new FormData();
    fd.append("file", selectedFile, srcName);
    const r = await fetch("/api/convert", { method: "POST", body: fd });
    const raw = await r.arrayBuffer();
    if (!r.ok) throw new Error(errorText(new TextDecoder().decode(raw)));
    convertedBlob = new Blob([raw], { type: "audio/wav" });
    const wavFile = new File([convertedBlob], wavName, { type: "audio/wav" });
    putFileInPicker(wavFile);
    analysis = { needs_conversion: false, ok: true, module_filename: wavName };
    try {
      const check = new FormData();
      check.append("file", wavFile, wavName);
      analysis = await api("/api/analyze", { method: "POST", body: check });
    } catch (_) {}
    const kb = Math.max(1, Math.round(convertedBlob.size / 1024));
    setAnalyze(
      "ok",
      `Успех: ${srcName} → ${wavName} (${kb} КБ). Это WAV PCM 16-bit 16 kHz mono, файл выбран для загрузки на модуль.`
    );
    flash(`конвертация успешна: ${wavName}`);
    btn.hidden = true;
    const path = $("uppath").value;
    const origStem = srcName.replace(/\.[^.]+$/, "");
    const leaf = (path.split("/").pop() || "");
    if (!path || leaf.includes(origStem) || /[а-яё]/i.test(leaf)) {
      const dir = selectedDir || "/";
      $("uppath").value = `${dir === "/" ? "" : dir}/${wavName}`;
    }
  } catch (e) {
    const msg = errorText(e);
    setAnalyze("fail", `Провал: конвертация не удалась. ${msg}`);
    flash(`конвертация не удалась: ${msg}`);
    btn.hidden = false;
  } finally {
    btn.disabled = false;
    btn.textContent = "Конвертировать через ffmpeg";
    $("upload-btn").disabled = false;
  }
};

$("upload-btn").onclick = async () => {
  if (!selectedHost) return;
  const path = $("uppath").value;
  let blob = convertedBlob;
  if (!blob && selectedFile) {
    if (analysis?.needs_conversion) {
      if (!confirm("Файл не в формате модуля. Конвертировать через ffmpeg и загрузить?")) return;
      const fdConv = new FormData();
      fdConv.append("file", selectedFile, selectedFile.name);
      const rConv = await fetch("/api/convert", { method: "POST", body: fdConv });
      if (!rConv.ok) { flash(errorText(await rConv.text())); return; }
      blob = await rConv.blob();
    } else {
      blob = selectedFile;
    }
  }
  if (!blob) { flash("сначала выберите файл"); return; }
  const kb = Math.max(1, Math.round(blob.size / 1024));
  const fd = new FormData();
  fd.append("file", blob, path.split("/").pop() || "sound.wav");
  $("upload-btn").disabled = true;
  flash(`загружаю ${kb} КБ на модуль…`);
  try {
    const r = await fetch(
      `/api/modules/${selectedHost}/upload?path=${encodeURIComponent(path)}`,
      { method: "POST", body: fd }
    );
    const text = await r.text();
    if (!r.ok) {
      flash(errorText(text));
      return;
    }
    flash(text);
    try {
      await loadFiles();
    } catch (_) {
      flash(text + " — модуль ещё не ответил, нажмите Обновить через несколько секунд");
    }
  } catch (e) {
    flash(errorText(e));
  } finally {
    $("upload-btn").disabled = false;
  }
};

(async () => {
  try {
    const meta = await api("/api/meta");
    $("meta-line").textContent = `${meta.subnet} · порт ${meta.scan_port} · ${meta.host_count} адресов`;
  } catch (_) {
    $("meta-line").textContent = "не удалось прочитать конфиг";
  }
  try {
    $("scan-note").textContent = "Проверяю сохранённые модули из config.yaml…";
    const known = await api("/api/known");
    if ((known.modules || []).length) {
      renderModules(known.modules);
      const down = known.modules.filter((m) => m.online === false).length;
      $("scan-note").textContent =
        down
          ? `Проверены модули из config.yaml. Недоступны: ${down}. Полный скан — кнопкой.`
          : "Проверены модули из config.yaml. Полный скан — кнопкой.";
    } else {
      $("scan-note").textContent = "В config.yaml пока нет модулей. Нажмите «Сканировать сеть».";
    }
  } catch (e) {
    $("scan-note").textContent = e.message;
  }
})();
