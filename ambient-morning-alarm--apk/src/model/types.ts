export type LightModeType = 'ramp' | 'pulse';

export type LightModeConfig = {
  type: LightModeType;
  startBrightness: number;
  finishBrightness: number;
  rampSeconds: number;
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

export function defaultLightMode(type: LightModeType = 'ramp'): LightModeConfig {
  if (type === 'pulse') {
    return {
      type: 'pulse',
      startBrightness: 5,
      finishBrightness: 100,
      rampSeconds: 5,
      darkSeconds: 5,
      totalSeconds: 600,
    };
  }
  return {
    type: 'ramp',
    startBrightness: 5,
    finishBrightness: 100,
    rampSeconds: 1200,
    darkSeconds: 0,
    totalSeconds: 1800,
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
