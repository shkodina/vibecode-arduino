import React, { createContext, useCallback, useContext, useEffect, useMemo, useRef, useState } from 'react';
import { bleClient } from '../ble/AlarmBleClient';
import { DEFAULT_APP_PREFS, normalizeAppPrefs, type AppPrefs } from '../model/appPrefs';
import { parseDeviceTimeMs } from '../model/lightPhase';
import {
  asDeviceSettings,
  defaultLightMode,
  defaultSettings,
  normalizeSettings,
  type AlarmConfig,
  type BleClientState,
  type DeviceSettings,
  type DeviceStatus,
  type LightModeConfig,
  type WifiConfig,
} from '../model/types';
import { commands } from '../protocol/AlarmProtocol';
import { loadAppPrefs, saveAppPrefs } from '../storage/prefsStorage';

type DeviceContextValue = {
  bleState: BleClientState;
  bleMessage: string;
  deviceName: string | null;
  status: DeviceStatus | null;
  statusAt: number;
  settings: DeviceSettings;
  setSettings: React.Dispatch<React.SetStateAction<DeviceSettings>>;
  wifiDraft: WifiConfig;
  setWifiDraft: React.Dispatch<React.SetStateAction<WifiConfig>>;
  testMode: LightModeConfig;
  setTestMode: React.Dispatch<React.SetStateAction<LightModeConfig>>;
  appPrefs: AppPrefs;
  setAppPrefs: (next: AppPrefs | ((prev: AppPrefs) => AppPrefs)) => void;
  busy: boolean;
  banner: string | null;
  connect: () => Promise<void>;
  loadSettings: () => Promise<void>;
  saveSettings: () => Promise<void>;
  saveTimer: () => Promise<void>;
  saveAlarm: (alarm: AlarmConfig) => Promise<void>;
  loadWifi: () => Promise<void>;
  saveWifi: () => Promise<void>;
  stop: () => Promise<void>;
  startTimer: () => Promise<void>;
  startTest: () => Promise<void>;
  stopTest: () => Promise<void>;
};

const DeviceContext = createContext<DeviceContextValue | null>(null);

function delay(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function humanizeBleError(error: unknown): string {
  const raw = error instanceof Error ? error.message : String(error);
  if (/rejected/i.test(raw)) {
    return 'Контроллер отклонил запись. Обновите настройки и попробуйте снова.';
  }
  return raw;
}

function isUnknownCommand(error: unknown): boolean {
  const raw = error instanceof Error ? error.message : String(error);
  return /unknown-command/i.test(raw);
}

export function DeviceProvider({ children }: { children: React.ReactNode }) {
  const [bleState, setBleState] = useState<BleClientState>(bleClient.state);
  const [bleMessage, setBleMessage] = useState(bleClient.stateMessage);
  const [deviceName, setDeviceName] = useState<string | null>(bleClient.deviceName);
  const [status, setStatus] = useState<DeviceStatus | null>(bleClient.lastStatus);
  const [statusAt, setStatusAt] = useState(bleClient.lastStatusAt);
  const [settings, setSettings] = useState<DeviceSettings>(defaultSettings());
  const [wifiDraft, setWifiDraft] = useState<WifiConfig>({ ssid: '', password: '' });
  const [testMode, setTestMode] = useState<LightModeConfig>(defaultLightMode('strobe'));
  const [appPrefs, setAppPrefsState] = useState<AppPrefs>(DEFAULT_APP_PREFS);
  const [busy, setBusy] = useState(false);
  const [banner, setBanner] = useState<string | null>(null);
  const prefsRef = useRef(appPrefs);
  prefsRef.current = appPrefs;

  const applySettingsPayload = useCallback((payload: DeviceSettings) => {
    const next = normalizeSettings(payload);
    setSettings(next);
    setWifiDraft(next.wifi);
  }, []);

  const setAppPrefs = useCallback((next: AppPrefs | ((prev: AppPrefs) => AppPrefs)) => {
    setAppPrefsState((prev) => {
      const resolved = normalizeAppPrefs(typeof next === 'function' ? next(prev) : next);
      void saveAppPrefs(resolved);
      return resolved;
    });
  }, []);

  useEffect(() => {
    void loadAppPrefs().then((loaded) => {
      setAppPrefsState(loaded);
      bleClient.configure(loaded);
    });
  }, []);

  useEffect(() => {
    bleClient.configure({
      commandTimeoutMs: appPrefs.commandTimeoutMs,
      commandRetries: appPrefs.commandRetries,
    });
  }, [appPrefs.commandTimeoutMs, appPrefs.commandRetries]);

  useEffect(() => {
    return bleClient.subscribe(() => {
      setBleState(bleClient.state);
      setBleMessage(bleClient.stateMessage);
      setDeviceName(bleClient.deviceName);
      setStatus(bleClient.lastStatus);
      setStatusAt(bleClient.lastStatusAt);
    });
  }, []);

  const pullAfterConnect = useCallback(async () => {
    if (bleClient.state !== 'Connected') {
      return false;
    }
    try {
      const response = await bleClient.sendBuilt(commands.getSettings());
      const payload = asDeviceSettings(response.payload);
      if (!payload) {
        return false;
      }
      applySettingsPayload(payload);
      setBanner(null);
      return true;
    } catch {
      return false;
    }
  }, [applySettingsPayload]);

  const connectQuiet = useCallback(async (announceFailure: boolean) => {
    const retries = Math.max(1, prefsRef.current.keepaliveRetries);
    for (let attempt = 0; attempt < retries; attempt += 1) {
      if (!(await bleClient.isLinkUp())) {
        await bleClient.connect();
      }
      if (await pullAfterConnect()) {
        return true;
      }
      await delay(400 * (attempt + 1));
    }
    if (await bleClient.isLinkUp()) {
      if (announceFailure) {
        setBanner('Не удалось загрузить настройки');
      }
      return true;
    }
    if (announceFailure) {
      setBanner('Нет связи с контроллером');
    }
    return false;
  }, [pullAfterConnect]);

  const connectQuietRef = useRef(connectQuiet);
  connectQuietRef.current = connectQuiet;

  useEffect(() => {
    void connectQuietRef.current(true);
    return () => {
      void bleClient.disconnect(false);
    };
  }, []);

  useEffect(() => {
    let stopped = false;
    let inFlight = false;
    const tick = async () => {
      if (stopped || inFlight) {
        return;
      }
      inFlight = true;
      try {
        const up = await bleClient.isLinkUp();
        if (!up) {
          await connectQuietRef.current(true);
          return;
        }
        await bleClient.refreshStatus();
        setBanner(null);
      } catch {
        await connectQuietRef.current(true);
      } finally {
        inFlight = false;
      }
    };
    const id = setInterval(() => {
      void tick();
    }, appPrefs.keepaliveIntervalSec * 1000);
    return () => {
      stopped = true;
      clearInterval(id);
    };
  }, [appPrefs.keepaliveIntervalSec]);

  useEffect(() => {
    let stopped = false;
    const sync = async () => {
      if (stopped || bleClient.state !== 'Connected') {
        return;
      }
      const deviceMs = parseDeviceTimeMs(bleClient.lastStatus?.currentTime);
      if (deviceMs == null) {
        return;
      }
      const driftSec = Math.abs(Date.now() - deviceMs) / 1000;
      if (driftSec <= prefsRef.current.timeSyncMaxDriftSec) {
        return;
      }
      try {
        await bleClient.sendBuilt(commands.setTime());
        await bleClient.refreshStatus();
      } catch {
        // keep trying on the next interval
      }
    };
    const id = setInterval(() => {
      void sync();
    }, appPrefs.timeSyncIntervalSec * 1000);
    return () => {
      stopped = true;
      clearInterval(id);
    };
  }, [appPrefs.timeSyncIntervalSec]);

  const withBusy = useCallback(async (fn: () => Promise<void>) => {
    setBusy(true);
    try {
      await fn();
      setBanner(null);
    } catch (e) {
      setBanner(humanizeBleError(e));
    } finally {
      setBusy(false);
    }
  }, []);

  const connect = useCallback(async () => {
    await withBusy(async () => {
      const ok = await connectQuiet(true);
      if (!ok) {
        throw new Error('Нет связи с контроллером');
      }
    });
  }, [connectQuiet, withBusy]);

  const loadSettings = useCallback(async () => {
    await withBusy(async () => {
      const response = await bleClient.sendBuilt(commands.getSettings());
      const payload = asDeviceSettings(response.payload);
      if (!payload) {
        throw new Error('Не удалось разобрать настройки контроллера');
      }
      applySettingsPayload(payload);
    });
  }, [applySettingsPayload, withBusy]);

  const writeTimer = useCallback(async () => {
    try {
      await bleClient.sendBuilt(commands.setTimer(settings.timer));
    } catch (error) {
      if (!isUnknownCommand(error)) {
        throw error;
      }
    }
  }, [settings.timer]);

  const saveTimer = useCallback(async () => {
    await withBusy(async () => {
      await writeTimer();
      await bleClient.refreshStatus();
    });
  }, [withBusy, writeTimer]);

  const saveSettings = useCallback(async () => {
    await withBusy(async () => {
      for (const alarm of settings.alarms) {
        await bleClient.sendBuilt(commands.setAlarm(alarm));
      }
      await writeTimer();
      await bleClient.refreshStatus();
    });
  }, [settings.alarms, withBusy, writeTimer]);

  const saveAlarm = useCallback(async (alarm: AlarmConfig) => {
    await withBusy(async () => {
      await bleClient.sendBuilt(commands.setAlarm(alarm));
      await bleClient.refreshStatus();
    });
  }, [withBusy]);

  const loadWifi = useCallback(async () => {
    await withBusy(async () => {
      const response = await bleClient.sendBuilt(commands.getWifi());
      const payload = response.payload as WifiConfig;
      setWifiDraft({ ssid: payload?.ssid || '', password: payload?.password || '' });
    });
  }, [withBusy]);

  const saveWifi = useCallback(async () => {
    await withBusy(async () => {
      await bleClient.sendBuilt(commands.setWifi(wifiDraft));
      await bleClient.refreshStatus();
    });
  }, [wifiDraft, withBusy]);

  const stop = useCallback(async () => {
    await withBusy(async () => {
      await bleClient.sendBuilt(commands.stop());
      await bleClient.refreshStatus();
    });
  }, [withBusy]);

  const startTimer = useCallback(async () => {
    await withBusy(async () => {
      await writeTimer();
      await bleClient.sendBuilt(commands.startTimer());
      await bleClient.refreshStatus();
    });
  }, [withBusy, writeTimer]);

  const startTest = useCallback(async () => {
    await withBusy(async () => {
      await bleClient.sendBuilt(commands.startTest(testMode));
      await bleClient.refreshStatus();
    });
  }, [testMode, withBusy]);

  const stopTest = useCallback(async () => {
    await withBusy(async () => {
      await bleClient.sendBuilt(commands.stopTest());
      await bleClient.refreshStatus();
    });
  }, [withBusy]);

  const value = useMemo(
    () => ({
      bleState,
      bleMessage,
      deviceName,
      status,
      statusAt,
      settings,
      setSettings,
      wifiDraft,
      setWifiDraft,
      testMode,
      setTestMode,
      appPrefs,
      setAppPrefs,
      busy,
      banner,
      connect,
      loadSettings,
      saveSettings,
      saveTimer,
      saveAlarm,
      loadWifi,
      saveWifi,
      stop,
      startTimer,
      startTest,
      stopTest,
    }),
    [
      bleState,
      bleMessage,
      deviceName,
      status,
      statusAt,
      settings,
      wifiDraft,
      testMode,
      appPrefs,
      setAppPrefs,
      busy,
      banner,
      connect,
      loadSettings,
      saveSettings,
      saveTimer,
      saveAlarm,
      loadWifi,
      saveWifi,
      stop,
      startTimer,
      startTest,
      stopTest,
    ],
  );

  return <DeviceContext.Provider value={value}>{children}</DeviceContext.Provider>;
}

export function useDevice(): DeviceContextValue {
  const ctx = useContext(DeviceContext);
  if (!ctx) {
    throw new Error('useDevice outside provider');
  }
  return ctx;
}
