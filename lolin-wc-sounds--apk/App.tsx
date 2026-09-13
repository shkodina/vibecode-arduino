import AsyncStorage from '@react-native-async-storage/async-storage';
import * as DocumentPicker from 'expo-document-picker';
import * as FileSystem from 'expo-file-system/legacy';
import { StatusBar } from 'expo-status-bar';
import { FFmpegKit, ReturnCode } from '@nikhil-cephei/ffmpeg-kit-react-native';
import React, { useCallback, useEffect, useMemo, useState } from 'react';
import {
  Alert,
  Pressable,
  SafeAreaView,
  ScrollView,
  StyleSheet,
  Text,
  TextInput,
  View,
} from 'react-native';

const COLORS = {
  bg: '#0d0d0d',
  panel: '#181818',
  card: '#222222',
  border: '#343434',
  text: '#f6f3ef',
  muted: '#aaa39b',
  accent: '#ff8a1a',
  danger: '#ff5555',
  ok: '#66cc78',
  input: '#111111',
};

const STORAGE_KEY = 'lolin-wc-sounds.modules.v1';
const SCAN_KEY = 'lolin-wc-sounds.scan.v1';
const DEFAULT_SUBNET = '192.168.88.0/24';
const DEFAULT_PORT = 80;
const WC_STATUS_KEYS = ['playing', 'sd_ok', 'volume', 'ip', 'motion', 'file', 'directory'];

type TabId = 'modules' | 'control' | 'schedule' | 'files' | 'settings';

type ModuleStatus = {
  playing?: boolean;
  sd_ok?: boolean;
  volume?: number;
  ip?: string;
  motion?: boolean;
  motion_raw?: boolean;
  file?: string;
  directory?: string;
  remaining_seconds?: number;
  time?: string;
  time_ok?: boolean;
  wifi_sta?: boolean;
};

type ModuleRow = {
  host: string;
  port: number;
  online: boolean;
  status?: ModuleStatus | null;
};

type Period = {
  start: string;
  end: string;
  directory: string;
  volume: number;
  random_on_startup: boolean;
  shuffle: boolean;
  repeat_selected: boolean;
  loop_directory: boolean;
};

type DeviceConfig = {
  wifi?: { ssid?: string; password?: string };
  ntp?: { server?: string; timezone_offset?: number; update_interval?: number };
  server?: { port?: number; watchdog_seconds?: number };
  watchdog_seconds?: number;
  motion?: {
    timeout_seconds?: number;
    cooldown_seconds?: number;
    repeat_seconds?: number;
    idle_repeat_seconds?: number;
    stable_ms?: number;
    boot_ignore_seconds?: number;
  };
  playback?: { schedule?: Period[] };
  [key: string]: unknown;
};

type FileEntry = { name: string; type?: string; dir?: boolean; size?: number };
type FolderListing = { path?: string; entries?: FileEntry[]; files?: FileEntry[] };
type PickedAudio = { uri: string; name: string; mimeType?: string; size?: number };

const TRANSLIT: Record<string, string> = {
  а: 'a',
  б: 'b',
  в: 'v',
  г: 'g',
  д: 'd',
  е: 'e',
  ё: 'yo',
  ж: 'zh',
  з: 'z',
  и: 'i',
  й: 'y',
  к: 'k',
  л: 'l',
  м: 'm',
  н: 'n',
  о: 'o',
  п: 'p',
  р: 'r',
  с: 's',
  т: 't',
  у: 'u',
  ф: 'f',
  х: 'h',
  ц: 'ts',
  ч: 'ch',
  ш: 'sh',
  щ: 'sch',
  ъ: '',
  ы: 'y',
  ь: '',
  э: 'e',
  ю: 'yu',
  я: 'ya',
};

function looksLikeModule(data: unknown): data is ModuleStatus {
  if (!data || typeof data !== 'object') return false;
  const obj = data as Record<string, unknown>;
  return WC_STATUS_KEYS.every((key) => Object.prototype.hasOwnProperty.call(obj, key));
}

function cleanHost(input: string) {
  return input.trim().replace(/^https?:\/\//, '').replace(/\/.*$/, '').replace(/:\d+$/, '');
}

function hostSort(a: ModuleRow, b: ModuleRow) {
  const pa = a.host.split('.').map((x) => Number(x));
  const pb = b.host.split('.').map((x) => Number(x));
  for (let i = 0; i < Math.max(pa.length, pb.length); i += 1) {
    const diff = (pa[i] || 0) - (pb[i] || 0);
    if (diff) return diff;
  }
  return a.host.localeCompare(b.host);
}

function hostsFromCidr(cidr: string): string[] {
  const [ip, maskText] = cidr.trim().split('/');
  const mask = Number(maskText || 24);
  const parts = ip.split('.').map((x) => Number(x));
  if (parts.length !== 4 || parts.some((x) => !Number.isInteger(x) || x < 0 || x > 255)) return [];
  if (!Number.isInteger(mask) || mask < 16 || mask > 30) return [];
  const base = parts.reduce((acc, x) => (acc << 8) + x, 0) >>> 0;
  const hostCount = 2 ** (32 - mask);
  const network = (base & (0xffffffff << (32 - mask))) >>> 0;
  const rows: string[] = [];
  for (let offset = 1; offset < hostCount - 1; offset += 1) {
    const n = (network + offset) >>> 0;
    rows.push(`${(n >>> 24) & 255}.${(n >>> 16) & 255}.${(n >>> 8) & 255}.${n & 255}`);
  }
  return rows;
}

function moduleFilename(original: string) {
  const stem = (original || '').replace(/\.[^.]+$/, '').toLowerCase().replace(/ /g, '-');
  let out = '';
  for (const ch of stem) out += TRANSLIT[ch] ?? (/[a-z0-9_-]/.test(ch) ? ch : '-');
  const slug = out.replace(/[-_]+/g, '-').replace(/^-|-$/g, '');
  return `${slug || 'sound'}.wav`;
}

function joinPath(dir: string, name: string) {
  const clean = name.replace(/^.*\//, '').replace(/\/$/, '').trim();
  if (!clean || clean === '.' || clean === '..') return null;
  return dir === '/' ? `/${clean}` : `${dir.replace(/\/$/, '')}/${clean}`;
}

function parentDir(path: string) {
  if (path === '/') return '/';
  return path.replace(/\/[^/]+$/, '') || '/';
}

function filePath(uri: string) {
  return decodeURIComponent(uri.replace(/^file:\/\//, ''));
}

async function fetchJson<T>(url: string, timeoutMs = 5000, init?: RequestInit): Promise<T> {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeoutMs);
  try {
    const response = await fetch(url, { ...init, signal: controller.signal });
    const text = await response.text();
    let data: unknown = text;
    try {
      data = text ? JSON.parse(text) : {};
    } catch {
      data = text;
    }
    if (!response.ok) {
      const msg =
        typeof data === 'object' && data && 'detail' in data
          ? String((data as { detail?: unknown }).detail)
          : String(text || response.statusText);
      throw new Error(msg);
    }
    return data as T;
  } finally {
    clearTimeout(timer);
  }
}

async function postText(url: string, init?: RequestInit, timeoutMs = 15000) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeoutMs);
  try {
    const response = await fetch(url, { ...init, signal: controller.signal });
    const text = await response.text();
    if (!response.ok) throw new Error(text || response.statusText);
    return text || 'ok';
  } finally {
    clearTimeout(timer);
  }
}

async function probe(host: string, port: number, timeoutMs = 600): Promise<ModuleRow | null> {
  try {
    const status = await fetchJson<ModuleStatus>(`http://${host}:${port}/api/status`, timeoutMs);
    if (!looksLikeModule(status)) return null;
    return { host, port, online: true, status };
  } catch {
    return null;
  }
}

async function mapLimit<T, R>(items: T[], limit: number, fn: (item: T) => Promise<R | null>) {
  const out: R[] = [];
  let index = 0;
  const workers = Array.from({ length: Math.min(limit, items.length) }, async () => {
    while (index < items.length) {
      const item = items[index];
      index += 1;
      const result = await fn(item);
      if (result) out.push(result);
    }
  });
  await Promise.all(workers);
  return out;
}

function Field({
  label,
  value,
  onChangeText,
  keyboardType,
  secureTextEntry,
}: {
  label: string;
  value: string;
  onChangeText: (value: string) => void;
  keyboardType?: 'default' | 'numeric';
  secureTextEntry?: boolean;
}) {
  return (
    <View style={styles.field}>
      <Text style={styles.label}>{label}</Text>
      <TextInput
        value={value}
        onChangeText={onChangeText}
        keyboardType={keyboardType}
        secureTextEntry={secureTextEntry}
        placeholderTextColor={COLORS.muted}
        style={styles.input}
      />
    </View>
  );
}

function Toggle({ label, value, onChange }: { label: string; value: boolean; onChange: (value: boolean) => void }) {
  return (
    <Pressable style={[styles.toggle, value && styles.toggleOn]} onPress={() => onChange(!value)}>
      <Text style={[styles.toggleText, value && styles.toggleTextOn]}>{value ? '✓ ' : ''}{label}</Text>
    </Pressable>
  );
}

function Button({
  title,
  onPress,
  kind = 'normal',
  disabled,
}: {
  title: string;
  onPress: () => void;
  kind?: 'normal' | 'ghost' | 'danger';
  disabled?: boolean;
}) {
  return (
    <Pressable
      disabled={disabled}
      style={[styles.button, kind === 'ghost' && styles.ghost, kind === 'danger' && styles.danger, disabled && styles.disabled]}
      onPress={onPress}
    >
      <Text style={styles.buttonText}>{title}</Text>
    </Pressable>
  );
}

function Dot({ on, color = COLORS.ok }: { on?: boolean; color?: string }) {
  return <View style={[styles.dot, on ? { backgroundColor: color } : null]} />;
}

function newPeriod(): Period {
  return {
    start: '00:00',
    end: '24:00',
    directory: '/data',
    volume: 60,
    random_on_startup: true,
    shuffle: false,
    repeat_selected: true,
    loop_directory: false,
  };
}

export default function App() {
  const [tab, setTab] = useState<TabId>('modules');
  const [subnet, setSubnet] = useState(DEFAULT_SUBNET);
  const [manualHost, setManualHost] = useState('');
  const [modules, setModules] = useState<ModuleRow[]>([]);
  const [selectedHost, setSelectedHost] = useState<string | null>(null);
  const [status, setStatus] = useState<ModuleStatus | null>(null);
  const [config, setConfig] = useState<DeviceConfig | null>(null);
  const [folder, setFolder] = useState('/');
  const [listing, setListing] = useState<FolderListing | null>(null);
  const [picked, setPicked] = useState<PickedAudio | null>(null);
  const [uploadPath, setUploadPath] = useState('/sound.wav');
  const [newFolder, setNewFolder] = useState('');
  const [busy, setBusy] = useState(false);
  const [message, setMessage] = useState('Готово');
  const selected = useMemo(
    () => modules.find((item) => item.host === selectedHost) || null,
    [modules, selectedHost],
  );
  const port = selected?.port || DEFAULT_PORT;

  const baseUrl = useCallback(
    (path: string) => {
      if (!selectedHost) throw new Error('модуль не выбран');
      return `http://${selectedHost}:${port}${path}`;
    },
    [port, selectedHost],
  );

  const saveModules = useCallback(async (rows: ModuleRow[]) => {
    const stable = rows
      .map((item) => ({ host: item.host, port: item.port, online: item.online, status: item.status || null }))
      .sort(hostSort);
    setModules(stable);
    await AsyncStorage.setItem(STORAGE_KEY, JSON.stringify(stable.map(({ host, port }) => ({ host, port }))));
  }, []);

  const refreshKnown = useCallback(
    async (knownRows = modules) => {
      if (!knownRows.length) return;
      setBusy(true);
      setMessage('Проверяю сохранённые модули...');
      const checked = await mapLimit(knownRows, 12, async (item) => {
        const live = await probe(item.host, item.port, 900);
        return live || { ...item, online: false, status: null };
      });
      await saveModules(checked);
      setMessage(`Проверено модулей: ${checked.length}`);
      setBusy(false);
    },
    [modules, saveModules],
  );

  const refreshStatus = useCallback(async () => {
    if (!selectedHost) return;
    try {
      const next = await fetchJson<ModuleStatus>(baseUrl('/api/status'), 3000);
      setStatus(next);
      setModules((rows) => rows.map((row) => (row.host === selectedHost ? { ...row, online: true, status: next } : row)));
    } catch (error) {
      setMessage(error instanceof Error ? error.message : String(error));
    }
  }, [baseUrl, selectedHost]);

  const loadConfig = useCallback(async () => {
    if (!selectedHost) return;
    const next = await fetchJson<DeviceConfig>(baseUrl('/api/config'), 7000);
    setConfig(next);
  }, [baseUrl, selectedHost]);

  const loadFiles = useCallback(
    async (path = folder) => {
      if (!selectedHost) return;
      const data = await fetchJson<FolderListing>(`${baseUrl('/api/files')}?path=${encodeURIComponent(path)}`, 10000);
      setFolder(path);
      setListing(data);
    },
    [baseUrl, folder, selectedHost],
  );

  useEffect(() => {
    (async () => {
      const [storedModules, storedScan] = await Promise.all([AsyncStorage.getItem(STORAGE_KEY), AsyncStorage.getItem(SCAN_KEY)]);
      if (storedScan) setSubnet(storedScan);
      if (storedModules) {
        const parsed = JSON.parse(storedModules) as { host: string; port: number }[];
        const rows = parsed.map((item) => ({ host: item.host, port: item.port || DEFAULT_PORT, online: false, status: null }));
        setModules(rows);
        refreshKnown(rows).catch((error) => setMessage(error instanceof Error ? error.message : String(error)));
      }
    })().catch((error) => setMessage(error instanceof Error ? error.message : String(error)));
  }, []);

  useEffect(() => {
    if (!selectedHost) return undefined;
    refreshStatus();
    loadConfig().catch((error) => setMessage(error instanceof Error ? error.message : String(error)));
    loadFiles('/').catch(() => setListing(null));
    const timer = setInterval(refreshStatus, 4000);
    return () => clearInterval(timer);
  }, [loadConfig, loadFiles, refreshStatus, selectedHost]);

  async function scanNetwork() {
    const hosts = hostsFromCidr(subnet);
    if (!hosts.length) {
      setMessage('Поддерживается CIDR /16..../30, например 192.168.88.0/24');
      return;
    }
    setBusy(true);
    setMessage(`Сканирую ${hosts.length} адресов по HTTP :80...`);
    await AsyncStorage.setItem(SCAN_KEY, subnet);
    try {
      const found = await mapLimit(hosts, 64, (host) => probe(host, DEFAULT_PORT, 650));
      await saveModules(found);
      setMessage(found.length ? `Найдено модулей: ${found.length}` : 'Модули не найдены');
    } finally {
      setBusy(false);
    }
  }

  async function addManualHost() {
    const host = cleanHost(manualHost);
    if (!host) return;
    setBusy(true);
    const found = await probe(host, DEFAULT_PORT, 2000);
    const next = found || { host, port: DEFAULT_PORT, online: false, status: null };
    await saveModules([...modules.filter((item) => item.host !== host), next]);
    setManualHost('');
    setMessage(found ? `Добавлен ${host}` : `${host} сохранён, но сейчас не ответил как wc-sounds`);
    setBusy(false);
  }

  async function openModule(row: ModuleRow) {
    if (!row.online) {
      setMessage('Модуль офлайн, сначала обновите или просканируйте');
      return;
    }
    setSelectedHost(row.host);
    setStatus(row.status || null);
    setTab('control');
  }

  async function command(path: string, ok: string, init?: RequestInit, timeoutMs = 15000) {
    if (!selectedHost) return;
    setBusy(true);
    try {
      const text = await postText(baseUrl(path), init, timeoutMs);
      setMessage(text || ok);
      await refreshStatus();
    } catch (error) {
      setMessage(error instanceof Error ? error.message : String(error));
    } finally {
      setBusy(false);
    }
  }

  function updateConfig(patch: (draft: DeviceConfig) => DeviceConfig) {
    setConfig((current) => patch(current ? { ...current } : {}));
  }

  function updatePeriod(index: number, patch: Partial<Period>) {
    updateConfig((draft) => {
      const schedule = [...(draft.playback?.schedule || [])];
      schedule[index] = { ...schedule[index], ...patch };
      return { ...draft, playback: { ...(draft.playback || {}), schedule } };
    });
  }

  async function saveConfig() {
    if (!selectedHost || !config) return;
    setBusy(true);
    try {
      const text = await postText(
        baseUrl('/api/config'),
        { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(config) },
        20000,
      );
      setMessage(text || 'config.json сохранён');
      await loadConfig();
    } catch (error) {
      setMessage(error instanceof Error ? error.message : String(error));
    } finally {
      setBusy(false);
    }
  }

  async function restoreConfig() {
    Alert.alert('Восстановить config.bak.json?', 'Модуль вернёт прошлый конфиг с SD-карты.', [
      { text: 'Отмена', style: 'cancel' },
      { text: 'Восстановить', style: 'destructive', onPress: () => command('/api/config/restore', 'восстановлено', { method: 'POST' }) },
    ]);
  }

  async function pickFile() {
    const result = await DocumentPicker.getDocumentAsync({ copyToCacheDirectory: true, type: '*/*' });
    if (result.canceled) return;
    const asset = result.assets[0];
    const file = { uri: asset.uri, name: asset.name || 'sound', mimeType: asset.mimeType, size: asset.size };
    setPicked(file);
    setUploadPath(joinPath(folder, moduleFilename(file.name)) || '/sound.wav');
    setMessage('Файл выбран. При загрузке будет конвертация в WAV 16 kHz mono.');
  }

  async function convertAudio(input: PickedAudio) {
    const outName = moduleFilename(input.name);
    const outputUri = `${FileSystem.cacheDirectory || ''}${Date.now()}-${outName}`;
    const inputPath = filePath(input.uri);
    const outputPath = filePath(outputUri);
    const commandLine = `-y -i "${inputPath}" -ar 16000 -ac 1 -c:a pcm_s16le "${outputPath}"`;
    const session = await FFmpegKit.execute(commandLine);
    const code = await session.getReturnCode();
    if (!ReturnCode.isSuccess(code)) {
      const logs = await session.getAllLogsAsString();
      throw new Error(logs.slice(-900) || 'ffmpeg не смог конвертировать файл');
    }
    const info = await FileSystem.getInfoAsync(outputUri);
    if (!info.exists) throw new Error('ffmpeg не создал выходной WAV');
    return { uri: outputUri, name: outName, mimeType: 'audio/wav', size: info.size };
  }

  async function uploadAudio() {
    if (!selectedHost || !picked) return;
    setBusy(true);
    try {
      setMessage('Конвертирую аудио на телефоне...');
      const wav = await convertAudio(picked);
      const target = uploadPath.endsWith('.wav') ? uploadPath : joinPath(parentDir(uploadPath), wav.name) || `/${wav.name}`;
      const form = new FormData();
      form.append('file', {
        uri: wav.uri,
        name: target.split('/').pop() || wav.name,
        type: 'audio/wav',
      } as unknown as Blob);
      setMessage(`Загружаю ${Math.max(1, Math.round((wav.size || 0) / 1024))} КБ на модуль...`);
      const response = await fetch(`${baseUrl('/api/upload')}?path=${encodeURIComponent(target)}`, { method: 'POST', body: form });
      const text = await response.text();
      if (!response.ok) throw new Error(text || response.statusText);
      setMessage(text || `Загружено ${target}`);
      await loadFiles(folder);
      await FileSystem.deleteAsync(wav.uri, { idempotent: true });
    } catch (error) {
      setMessage(error instanceof Error ? error.message : String(error));
    } finally {
      setBusy(false);
    }
  }

  async function mkdir() {
    if (!selectedHost) return;
    const path = joinPath(folder, newFolder.toLowerCase());
    if (!path) {
      setMessage('Введите имя папки: латиница, цифры, - или _');
      return;
    }
    await command(`/api/mkdir?path=${encodeURIComponent(path)}`, 'папка создана', { method: 'POST' });
    setNewFolder('');
    await loadFiles(folder);
  }

  async function deletePath(path: string) {
    Alert.alert('Удалить файл?', path, [
      { text: 'Отмена', style: 'cancel' },
      {
        text: 'Удалить',
        style: 'destructive',
        onPress: async () => {
          await command(`/api/delete?path=${encodeURIComponent(path)}`, 'удалено', { method: 'DELETE' });
          await loadFiles(folder);
        },
      },
    ]);
  }

  const schedule = config?.playback?.schedule || [];
  const entries = listing?.entries || listing?.files || [];
  const dirs = entries.filter((entry) => (entry.type === 'dir' || entry.dir) && !entry.name.includes(' '));
  const files = entries.filter((entry) => !(entry.type === 'dir' || entry.dir));

  return (
    <SafeAreaView style={styles.root}>
      <StatusBar style="light" />
      <View style={styles.header}>
        <View>
          <Text style={styles.title}>WC Sounds</Text>
          <Text style={styles.subtitle}>{selectedHost ? `${selectedHost}:${port}` : 'lolin-wc-sounds APK'}</Text>
        </View>
        <View style={styles.headerPills}>
          <Text style={styles.pill}>{busy ? 'работаю' : 'готово'}</Text>
          {status ? <Text style={styles.pill}>{status.playing ? 'играет' : 'тишина'}</Text> : null}
        </View>
      </View>
      <Text style={styles.message}>{message}</Text>
      <View style={styles.tabs}>
        {[
          ['modules', 'Модули'],
          ['control', 'Пульт'],
          ['schedule', 'Расписание'],
          ['files', 'Файлы'],
          ['settings', 'Настройки'],
        ].map(([id, label]) => (
          <Pressable key={id} style={[styles.tab, tab === id && styles.tabOn]} onPress={() => setTab(id as TabId)}>
            <Text style={[styles.tabText, tab === id && styles.tabTextOn]}>{label}</Text>
          </Pressable>
        ))}
      </View>
      <ScrollView style={styles.body} contentContainerStyle={styles.bodyContent}>
        {tab === 'modules' ? (
          <View>
            <Text style={styles.sectionTitle}>Поиск в LAN</Text>
            <Field label="Подсеть CIDR" value={subnet} onChangeText={setSubnet} />
            <View style={styles.row}>
              <Button title="Сканировать сеть" onPress={scanNetwork} disabled={busy} />
              <Button title="Обновить сохранённые" onPress={() => refreshKnown()} kind="ghost" disabled={busy || !modules.length} />
            </View>
            <Text style={styles.sectionTitle}>Добавить вручную</Text>
            <Field label="IP или http://IP" value={manualHost} onChangeText={setManualHost} />
            <Button title="Добавить IP" onPress={addManualHost} disabled={busy} />
            <Text style={styles.sectionTitle}>Найденные модули</Text>
            {modules.map((item) => (
              <Pressable key={item.host} style={[styles.card, selectedHost === item.host && styles.cardActive]} onPress={() => openModule(item)}>
                <View style={styles.cardTop}>
                  <Text style={styles.cardTitle}>{item.host}</Text>
                  <Dot on={item.online} color={item.status?.playing ? COLORS.accent : COLORS.ok} />
                </View>
                <Text style={styles.meta}>
                  {item.online
                    ? `${item.status?.playing ? 'играет' : 'тишина'} · vol ${item.status?.volume ?? '-'} · ${item.status?.file || 'нет файла'}`
                    : 'офлайн'}
                </Text>
              </Pressable>
            ))}
          </View>
        ) : null}

        {tab === 'control' ? (
          <View>
            <Text style={styles.sectionTitle}>Что сейчас</Text>
            <View style={styles.card}>
              <Text style={styles.cardTitle}>{selectedHost || 'Модуль не выбран'}</Text>
              <Text style={styles.meta}>время: {status?.time || '-'} · осталось: {status?.remaining_seconds ?? 0} с</Text>
              <Text style={styles.meta}>файл: {status?.file || '-'} · папка: {status?.directory || '-'}</Text>
              <View style={styles.leds}>
                <Text style={styles.led}><Dot on={status?.playing} color={COLORS.accent} /> играет</Text>
                <Text style={styles.led}><Dot on={status?.motion} /> движение</Text>
                <Text style={styles.led}><Dot on={status?.motion_raw} /> PIR raw</Text>
                <Text style={styles.led}><Dot on={status?.sd_ok} /> SD</Text>
                <Text style={styles.led}><Dot on={status?.wifi_sta} /> WiFi</Text>
                <Text style={styles.led}><Dot on={status?.time_ok} /> NTP</Text>
              </View>
            </View>
            <View style={styles.row}>
              <Button title="Play" onPress={() => command('/api/play', 'play', { method: 'POST' })} disabled={!selectedHost || busy} />
              <Button title="Stop" onPress={() => command('/api/stop', 'stop', { method: 'POST' })} kind="danger" disabled={!selectedHost || busy} />
              <Button title="Статус" onPress={refreshStatus} kind="ghost" disabled={!selectedHost || busy} />
            </View>
            <Text style={styles.sectionTitle}>Громкость сейчас</Text>
            <Field
              label="0..100"
              value={String(status?.volume ?? 60)}
              keyboardType="numeric"
              onChangeText={(value) => setStatus((current) => ({ ...(current || {}), volume: Number(value) }))}
            />
            <Button
              title="Применить громкость"
              onPress={() => command(`/api/volume?value=${encodeURIComponent(String(status?.volume ?? 60))}`, 'громкость применена', { method: 'POST' })}
              disabled={!selectedHost || busy}
            />
            <View style={styles.row}>
              <Button title="Reload config" onPress={() => command('/api/reload', 'reload', { method: 'POST' })} kind="ghost" disabled={!selectedHost || busy} />
              <Button title="Reboot" onPress={() => command('/api/reboot', 'reboot', { method: 'POST' }, 4000)} kind="danger" disabled={!selectedHost || busy} />
            </View>
          </View>
        ) : null}

        {tab === 'schedule' ? (
          <View>
            <View style={styles.rowBetween}>
              <Text style={styles.sectionTitle}>Расписание</Text>
              <Button
                title="+ период"
                onPress={() =>
                  updateConfig((draft) => ({
                    ...draft,
                    playback: { ...(draft.playback || {}), schedule: [...(draft.playback?.schedule || []), newPeriod()] },
                  }))
                }
                kind="ghost"
                disabled={!config}
              />
            </View>
            {schedule.map((period, index) => (
              <View key={index} style={styles.card}>
                <View style={styles.rowBetween}>
                  <Text style={styles.cardTitle}>Период {index + 1}</Text>
                  <Button
                    title="Удалить"
                    kind="danger"
                    onPress={() =>
                      updateConfig((draft) => ({
                        ...draft,
                        playback: { ...(draft.playback || {}), schedule: schedule.filter((_, i) => i !== index) },
                      }))
                    }
                  />
                </View>
                <Field label="С" value={period.start} onChangeText={(value) => updatePeriod(index, { start: value })} />
                <Field label="До" value={period.end} onChangeText={(value) => updatePeriod(index, { end: value })} />
                <Field label="Папка" value={period.directory} onChangeText={(value) => updatePeriod(index, { directory: value })} />
                <Field label="Громкость" value={String(period.volume)} keyboardType="numeric" onChangeText={(value) => updatePeriod(index, { volume: Number(value) })} />
                <View style={styles.checks}>
                  <Toggle label="случайный при PIR" value={!!period.random_on_startup} onChange={(value) => updatePeriod(index, { random_on_startup: value })} />
                  <Toggle label="shuffle" value={!!period.shuffle} onChange={(value) => updatePeriod(index, { shuffle: value })} />
                  <Toggle label="крутить один файл" value={!!period.repeat_selected} onChange={(value) => updatePeriod(index, { repeat_selected: value })} />
                  <Toggle label="цикл папки" value={!!period.loop_directory} onChange={(value) => updatePeriod(index, { loop_directory: value })} />
                </View>
              </View>
            ))}
            <Button title="Сохранить config.json" onPress={saveConfig} disabled={!config || busy} />
          </View>
        ) : null}

        {tab === 'files' ? (
          <View>
            <Text style={styles.sectionTitle}>SD-карта</Text>
            <Text style={styles.meta}>Текущая папка: {folder}</Text>
            <View style={styles.row}>
              <Button title="Обновить" onPress={() => loadFiles(folder)} kind="ghost" disabled={!selectedHost || busy} />
              <Button title="Корень" onPress={() => loadFiles('/')} kind="ghost" disabled={!selectedHost || busy} />
            </View>
            <Field label="Новая папка в текущей" value={newFolder} onChangeText={setNewFolder} />
            <Button title="Создать папку" onPress={mkdir} kind="ghost" disabled={!selectedHost || busy || !newFolder.trim()} />
            {folder !== '/' ? <Button title="Вверх" onPress={() => loadFiles(parentDir(folder))} kind="ghost" /> : null}
            {dirs.map((entry) => {
              const path = entry.name.startsWith('/') ? entry.name : joinPath(folder, entry.name);
              return path ? (
                <Pressable key={path} style={styles.fileRow} onPress={() => loadFiles(path)}>
                  <Text style={styles.fileText}>[dir] {entry.name}</Text>
                </Pressable>
              ) : null;
            })}
            {files.map((entry) => {
              const path = entry.name.startsWith('/') ? entry.name : joinPath(folder, entry.name);
              return path ? (
                <View key={path} style={styles.fileRow}>
                  <Text style={styles.fileText}>{entry.name} {entry.size ? `(${entry.size} b)` : ''}</Text>
                  <Button title="Удалить" onPress={() => deletePath(path)} kind="danger" />
                </View>
              ) : null;
            })}
            <Text style={styles.sectionTitle}>Загрузка аудио</Text>
            <Button title="Выбрать файл" onPress={pickFile} kind="ghost" />
            <Text style={styles.meta}>{picked ? `${picked.name} → ffmpeg WAV 16 kHz mono` : 'Файл не выбран'}</Text>
            <Field label="Путь на SD" value={uploadPath} onChangeText={setUploadPath} />
            <Button title="Конвертировать и загрузить" onPress={uploadAudio} disabled={!selectedHost || !picked || busy} />
          </View>
        ) : null}

        {tab === 'settings' ? (
          <View>
            <Text style={styles.sectionTitle}>WiFi / NTP</Text>
            <Field label="SSID" value={config?.wifi?.ssid || ''} onChangeText={(value) => updateConfig((draft) => ({ ...draft, wifi: { ...(draft.wifi || {}), ssid: value } }))} />
            <Field
              label="Password"
              value={config?.wifi?.password || ''}
              secureTextEntry
              onChangeText={(value) => updateConfig((draft) => ({ ...draft, wifi: { ...(draft.wifi || {}), password: value } }))}
            />
            <Field label="NTP server" value={config?.ntp?.server || 'pool.ntp.org'} onChangeText={(value) => updateConfig((draft) => ({ ...draft, ntp: { ...(draft.ntp || {}), server: value } }))} />
            <Field
              label="Timezone hours"
              value={String(config?.ntp?.timezone_offset ?? 3)}
              keyboardType="numeric"
              onChangeText={(value) => updateConfig((draft) => ({ ...draft, ntp: { ...(draft.ntp || {}), timezone_offset: Number(value) } }))}
            />
            <Field
              label="NTP interval"
              value={String(config?.ntp?.update_interval ?? 3600)}
              keyboardType="numeric"
              onChangeText={(value) => updateConfig((draft) => ({ ...draft, ntp: { ...(draft.ntp || {}), update_interval: Number(value) } }))}
            />
            <Text style={styles.sectionTitle}>Motion</Text>
            {[
              ['timeout_seconds', 'Timeout sec', 30],
              ['cooldown_seconds', 'Cooldown sec', 5],
              ['repeat_seconds', 'Repeat sec', 15],
              ['idle_repeat_seconds', 'Idle repeat sec', 15],
              ['stable_ms', 'Stable ms', 400],
              ['boot_ignore_seconds', 'Boot ignore sec', 10],
            ].map(([key, label, fallback]) => (
              <Field
                key={String(key)}
                label={String(label)}
                value={String(config?.motion?.[key as keyof NonNullable<DeviceConfig['motion']>] ?? fallback)}
                keyboardType="numeric"
                onChangeText={(value) =>
                  updateConfig((draft) => ({
                    ...draft,
                    motion: { ...(draft.motion || {}), [String(key)]: Number(value) },
                  }))
                }
              />
            ))}
            <Field
              label="Watchdog sec"
              value={String(config?.watchdog_seconds ?? config?.server?.watchdog_seconds ?? 30)}
              keyboardType="numeric"
              onChangeText={(value) =>
                updateConfig((draft) => ({
                  ...draft,
                  watchdog_seconds: Number(value),
                  server: { ...(draft.server || {}), port: draft.server?.port || 80, watchdog_seconds: Number(value) },
                }))
              }
            />
            <View style={styles.row}>
              <Button title="Сохранить config.json" onPress={saveConfig} disabled={!config || busy} />
              <Button title="Восстановить bak" onPress={restoreConfig} kind="danger" disabled={!selectedHost || busy} />
            </View>
            <Text style={styles.hint}>Смена WiFi сохраняется на SD, но подключение к новой сети будет после reboot модуля.</Text>
          </View>
        ) : null}
      </ScrollView>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  root: { flex: 1, backgroundColor: COLORS.bg },
  header: {
    paddingHorizontal: 16,
    paddingTop: 12,
    paddingBottom: 8,
    borderBottomWidth: 1,
    borderBottomColor: COLORS.border,
    flexDirection: 'row',
    justifyContent: 'space-between',
    gap: 12,
  },
  title: { color: COLORS.text, fontSize: 24, fontWeight: '800' },
  subtitle: { color: COLORS.muted, marginTop: 2 },
  headerPills: { flexDirection: 'row', gap: 6, alignItems: 'center', flexWrap: 'wrap', justifyContent: 'flex-end', flex: 1 },
  pill: { color: COLORS.accent, borderColor: COLORS.border, borderWidth: 1, borderRadius: 999, paddingHorizontal: 8, paddingVertical: 4, fontSize: 12 },
  message: { color: COLORS.muted, paddingHorizontal: 16, paddingVertical: 8 },
  tabs: { flexDirection: 'row', backgroundColor: COLORS.panel, borderTopWidth: 1, borderBottomWidth: 1, borderColor: COLORS.border },
  tab: { flex: 1, alignItems: 'center', paddingVertical: 12 },
  tabOn: { borderBottomWidth: 2, borderBottomColor: COLORS.accent },
  tabText: { color: COLORS.muted, fontSize: 11, fontWeight: '700' },
  tabTextOn: { color: COLORS.accent },
  body: { flex: 1 },
  bodyContent: { padding: 16, paddingBottom: 32 },
  sectionTitle: { color: COLORS.accent, fontSize: 18, fontWeight: '800', marginTop: 12, marginBottom: 10 },
  card: { backgroundColor: COLORS.card, borderColor: COLORS.border, borderWidth: 1, borderRadius: 16, padding: 14, marginBottom: 10 },
  cardActive: { borderColor: COLORS.accent },
  cardTop: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center' },
  cardTitle: { color: COLORS.text, fontSize: 16, fontWeight: '800', marginBottom: 6 },
  meta: { color: COLORS.muted, lineHeight: 20 },
  hint: { color: COLORS.muted, lineHeight: 20, marginTop: 8 },
  field: { marginBottom: 10 },
  label: { color: COLORS.muted, marginBottom: 5, fontSize: 12, fontWeight: '700' },
  input: { backgroundColor: COLORS.input, borderColor: COLORS.border, borderWidth: 1, color: COLORS.text, borderRadius: 10, paddingHorizontal: 12, paddingVertical: 10 },
  row: { flexDirection: 'row', flexWrap: 'wrap', gap: 8, alignItems: 'center', marginVertical: 6 },
  rowBetween: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center', gap: 8 },
  button: { backgroundColor: COLORS.accent, borderRadius: 10, paddingHorizontal: 12, paddingVertical: 10, marginVertical: 3 },
  ghost: { backgroundColor: COLORS.panel, borderWidth: 1, borderColor: COLORS.border },
  danger: { backgroundColor: COLORS.danger },
  disabled: { opacity: 0.45 },
  buttonText: { color: COLORS.text, fontWeight: '800' },
  dot: { width: 10, height: 10, borderRadius: 5, backgroundColor: COLORS.border, marginRight: 5 },
  leds: { flexDirection: 'row', flexWrap: 'wrap', gap: 8, marginTop: 10 },
  led: { color: COLORS.muted, flexDirection: 'row', alignItems: 'center' },
  checks: { flexDirection: 'row', flexWrap: 'wrap', gap: 8, marginTop: 4 },
  toggle: { borderColor: COLORS.border, borderWidth: 1, borderRadius: 999, paddingHorizontal: 10, paddingVertical: 8 },
  toggleOn: { borderColor: COLORS.accent, backgroundColor: '#2b1b0d' },
  toggleText: { color: COLORS.muted, fontWeight: '700' },
  toggleTextOn: { color: COLORS.accent },
  fileRow: {
    backgroundColor: COLORS.card,
    borderColor: COLORS.border,
    borderWidth: 1,
    borderRadius: 12,
    padding: 10,
    marginBottom: 7,
    flexDirection: 'row',
    justifyContent: 'space-between',
    gap: 8,
    alignItems: 'center',
  },
  fileText: { color: COLORS.text, flex: 1 },
});
