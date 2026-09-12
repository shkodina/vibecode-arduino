import { changedAppPrefs, DEFAULT_APP_PREFS, normalizeAppPrefs, type AppPrefs } from '../model/appPrefs';

const STORAGE_KEY = 'piper-light-alarm.appPrefs';

type AsyncStorageLike = {
  getItem(key: string): Promise<string | null>;
  setItem(key: string, value: string): Promise<void>;
};

let memory: Partial<AppPrefs> = {};

async function store(): Promise<AsyncStorageLike | null> {
  try {
    const mod = await import('@react-native-async-storage/async-storage');
    return (mod.default || mod) as AsyncStorageLike;
  } catch {
    return null;
  }
}

export async function loadAppPrefs(): Promise<AppPrefs> {
  try {
    const asyncStorage = await store();
    const raw = asyncStorage ? await asyncStorage.getItem(STORAGE_KEY) : null;
    const parsed = raw ? (JSON.parse(raw) as Partial<AppPrefs>) : memory;
    memory = parsed || {};
    return normalizeAppPrefs(memory);
  } catch {
    return { ...DEFAULT_APP_PREFS };
  }
}

export async function saveAppPrefs(prefs: AppPrefs): Promise<void> {
  const changed = changedAppPrefs(normalizeAppPrefs(prefs));
  memory = changed;
  const asyncStorage = await store();
  if (!asyncStorage) {
    return;
  }
  if (Object.keys(changed).length === 0) {
    await asyncStorage.setItem(STORAGE_KEY, '{}');
    return;
  }
  await asyncStorage.setItem(STORAGE_KEY, JSON.stringify(changed));
}
