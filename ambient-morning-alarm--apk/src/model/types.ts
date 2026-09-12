export type LightModeType = 'ramp' | 'pulse' | 'strobe';

export type LightModeConfig = {
  type: LightModeType;
  startBrightness: number;
  finishBrightness: number;
  rampSeconds: number;
  glowSeconds: number;
  fadeSeconds: number;
  darkSeconds: number;
  totalSeconds: number;
};

export type AlarmConfig = {
  id: number;
  enabled: boolean;
  hour: number;
  minute: number;
  weekdaysMask: number;
  mode: LightModeConfig;
};

export type TimerConfig = {
  hours: number;
  minutes: number;
  mode: LightModeConfig;
};

export type WifiConfig = {
  ssid: string;
  password: string;
};

export type DeviceSettings = {
  wifi: WifiConfig;
  alarms: AlarmConfig[];
  timer: TimerConfig;
};

export type ActiveRun = {
  type: string;
  alarmId: number | null;
  mode: string;
  startedAt: string | null;
  remainingSeconds: number;
  brightness: number;
};

export type DeviceStatus = {
  currentTime: string;
  firmwareVersion: string;
  ntpSynced: boolean;
  wifiConnected: boolean;
  wifiSsid?: string;
  activeRun: ActiveRun | null;
};

export type BleClientState =
  | 'NoPermission'
  | 'Scanning'
  | 'DeviceNotFound'
  | 'Connecting'
  | 'Connected'
  | 'Disconnected'
  | 'Error';

export type CommandResponse = {
  requestId: string;
  ok: boolean;
  payload?: unknown;
  error?: string;
};

export const ALARM_COUNT = 10;

export const WEEKDAY_LABELS = ['Пн', 'Вт', 'Ср', 'Чт', 'Пт', 'Сб', 'Вс'] as const;

export const MODE_TYPES: LightModeType[] = ['ramp', 'pulse', 'strobe'];

function num(value: unknown, fallback: number): number {
  return typeof value === 'number' && Number.isFinite(value) ? value : fallback;
}

export function defaultLightMode(type: LightModeType = 'ramp'): LightModeConfig {
  if (type === 'pulse') {
    return {
      type: 'pulse',
      startBrightness: 5,
      finishBrightness: 100,
      rampSeconds: 5,
      glowSeconds: 5,
      fadeSeconds: 5,
      darkSeconds: 5,
      totalSeconds: 600,
    };
  }
  if (type === 'strobe') {
    return {
      type: 'strobe',
      startBrightness: 100,
      finishBrightness: 100,
      rampSeconds: 0,
      glowSeconds: 0,
      fadeSeconds: 0,
      darkSeconds: 0,
      totalSeconds: 10,
    };
  }
  return {
    type: 'ramp',
    startBrightness: 5,
    finishBrightness: 100,
    rampSeconds: 1200,
    glowSeconds: 0,
    fadeSeconds: 0,
    darkSeconds: 0,
    totalSeconds: 1800,
  };
}

export function normalizeLightMode(raw?: Partial<LightModeConfig> | null): LightModeConfig {
  const type: LightModeType =
    raw?.type === 'pulse' || raw?.type === 'strobe' || raw?.type === 'ramp' ? raw.type : 'ramp';
  const defaults = defaultLightMode(type);
  const missingFallback = raw ? 0 : defaults.glowSeconds;
  return {
    type,
    startBrightness: num(raw?.startBrightness, defaults.startBrightness),
    finishBrightness: num(raw?.finishBrightness, defaults.finishBrightness),
    rampSeconds: num(raw?.rampSeconds, defaults.rampSeconds),
    glowSeconds: num(raw?.glowSeconds, raw ? missingFallback : defaults.glowSeconds),
    fadeSeconds: num(raw?.fadeSeconds, raw ? 0 : defaults.fadeSeconds),
    darkSeconds: num(raw?.darkSeconds, defaults.darkSeconds),
    totalSeconds: num(raw?.totalSeconds, defaults.totalSeconds),
  };
}

export function modeForType(prev: LightModeConfig, type: LightModeType): LightModeConfig {
  if (type === 'strobe') {
    return {
      ...defaultLightMode('strobe'),
      totalSeconds: Math.max(prev.totalSeconds, 1),
    };
  }
  if (type === 'pulse') {
    return {
      ...prev,
      type: 'pulse',
      rampSeconds: Math.max(prev.rampSeconds, 1),
      darkSeconds: Math.max(prev.darkSeconds, 1),
      glowSeconds: prev.glowSeconds,
      fadeSeconds: prev.fadeSeconds,
    };
  }
  return {
    ...prev,
    type: 'ramp',
    rampSeconds: Math.max(prev.rampSeconds, 1),
    glowSeconds: 0,
    fadeSeconds: 0,
    darkSeconds: 0,
  };
}

export function defaultAlarm(id: number): AlarmConfig {
  return {
    id,
    enabled: false,
    hour: 7,
    minute: 30,
    weekdaysMask: 31,
    mode: defaultLightMode('ramp'),
  };
}

export function normalizeAlarm(raw: Partial<AlarmConfig> | undefined, fallbackId: number): AlarmConfig {
  const base = defaultAlarm(fallbackId);
  return {
    id: num(raw?.id, fallbackId),
    enabled: Boolean(raw?.enabled),
    hour: num(raw?.hour, base.hour),
    minute: num(raw?.minute, base.minute),
    weekdaysMask: num(raw?.weekdaysMask, base.weekdaysMask),
    mode: normalizeLightMode(raw?.mode),
  };
}

export function normalizeSettings(raw?: Partial<DeviceSettings> | null): DeviceSettings {
  const defaults = defaultSettings();
  const alarms = Array.from({ length: ALARM_COUNT }, (_, id) =>
    normalizeAlarm(raw?.alarms?.[id], id),
  );
  return {
    wifi: {
      ssid: raw?.wifi?.ssid || '',
      password: raw?.wifi?.password || '',
    },
    alarms,
    timer: {
      hours: num(raw?.timer?.hours, defaults.timer.hours),
      minutes: num(raw?.timer?.minutes, defaults.timer.minutes),
      mode: normalizeLightMode(raw?.timer?.mode ?? defaults.timer.mode),
    },
  };
}

export function defaultSettings(): DeviceSettings {
  return {
    wifi: { ssid: '', password: '' },
    alarms: Array.from({ length: ALARM_COUNT }, (_, id) => defaultAlarm(id)),
    timer: {
      hours: 0,
      minutes: 30,
      mode: defaultLightMode('pulse'),
    },
  };
}
