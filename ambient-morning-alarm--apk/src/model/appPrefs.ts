export type AppPrefs = {
  commandTimeoutMs: number;
  commandRetries: number;
  keepaliveIntervalSec: number;
  keepaliveRetries: number;
  timeSyncIntervalSec: number;
  timeSyncMaxDriftSec: number;
};

export const DEFAULT_APP_PREFS: AppPrefs = {
  commandTimeoutMs: 20000,
  commandRetries: 3,
  keepaliveIntervalSec: 8,
  keepaliveRetries: 3,
  timeSyncIntervalSec: 60,
  timeSyncMaxDriftSec: 180,
};

export function normalizeAppPrefs(raw?: Partial<AppPrefs> | null): AppPrefs {
  const n = (value: unknown, fallback: number, min: number, max: number) => {
    const num = typeof value === 'number' && Number.isFinite(value) ? value : fallback;
    return Math.min(max, Math.max(min, Math.round(num)));
  };
  return {
    commandTimeoutMs: n(raw?.commandTimeoutMs, DEFAULT_APP_PREFS.commandTimeoutMs, 3000, 60000),
    commandRetries: n(raw?.commandRetries, DEFAULT_APP_PREFS.commandRetries, 0, 8),
    keepaliveIntervalSec: n(raw?.keepaliveIntervalSec, DEFAULT_APP_PREFS.keepaliveIntervalSec, 3, 120),
    keepaliveRetries: n(raw?.keepaliveRetries, DEFAULT_APP_PREFS.keepaliveRetries, 1, 8),
    timeSyncIntervalSec: n(raw?.timeSyncIntervalSec, DEFAULT_APP_PREFS.timeSyncIntervalSec, 15, 600),
    timeSyncMaxDriftSec: n(raw?.timeSyncMaxDriftSec, DEFAULT_APP_PREFS.timeSyncMaxDriftSec, 30, 3600),
  };
}

export function changedAppPrefs(prefs: AppPrefs): Partial<AppPrefs> {
  const out: Partial<AppPrefs> = {};
  (Object.keys(DEFAULT_APP_PREFS) as (keyof AppPrefs)[]).forEach((key) => {
    if (prefs[key] !== DEFAULT_APP_PREFS[key]) {
      out[key] = prefs[key];
    }
  });
  return out;
}
