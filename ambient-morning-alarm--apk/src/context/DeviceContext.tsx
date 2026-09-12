import React, { createContext, useCallback, useContext, useEffect, useMemo, useState } from 'react';
import { bleClient } from '../ble/AlarmBleClient';
import {
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

type DeviceContextValue = {
  bleState: BleClientState;
  bleMessage: string;
  deviceName: string | null;
  status: DeviceStatus | null;
  settings: DeviceSettings;
  setSettings: React.Dispatch<React.SetStateAction<DeviceSettings>>;
  wifiDraft: WifiConfig;
  setWifiDraft: React.Dispatch<React.SetStateAction<WifiConfig>>;
  testMode: LightModeConfig;
  setTestMode: React.Dispatch<React.SetStateAction<LightModeConfig>>;
  busy: boolean;
  banner: string | null;
  connect: () => Promise<void>;
  loadSettings: () => Promise<void>;
  saveSettings: () => Promise<void>;
  saveAlarm: (alarm: AlarmConfig) => Promise<void>;
  loadWifi: () => Promise<void>;
  saveWifi: () => Promise<void>;
  stop: () => Promise<void>;
  startTimer: () => Promise<void>;
  startTest: () => Promise<void>;
  stopTest: () => Promise<void>;
};

const DeviceContext = createContext<DeviceContextValue | null>(null);

export function DeviceProvider({ children }: { children: React.ReactNode }) {
  const [bleState, setBleState] = useState<BleClientState>(bleClient.state);
  const [bleMessage, setBleMessage] = useState(bleClient.stateMessage);
  const [deviceName, setDeviceName] = useState<string | null>(bleClient.deviceName);
  const [status, setStatus] = useState<DeviceStatus | null>(bleClient.lastStatus);
  const [settings, setSettings] = useState<DeviceSettings>(defaultSettings());
  const [wifiDraft, setWifiDraft] = useState<WifiConfig>({ ssid: '', password: '' });
  const [testMode, setTestMode] = useState<LightModeConfig>(defaultLightMode('strobe'));
  const [busy, setBusy] = useState(false);
  const [banner, setBanner] = useState<string | null>(null);

  useEffect(() => {
    return bleClient.subscribe(() => {
      setBleState(bleClient.state);
      setBleMessage(bleClient.stateMessage);
      setDeviceName(bleClient.deviceName);
      setStatus(bleClient.lastStatus);
      if (bleClient.state !== 'Connected' && bleClient.stateMessage) {
        setBanner(bleClient.stateMessage);
      }
    });
  }, []);

  useEffect(() => {
    void (async () => {
      await bleClient.connect();
      if (bleClient.state === 'Connected') {
        try {
          const response = await bleClient.sendBuilt(commands.getSettings());
          const payload = response.payload as DeviceSettings;
          if (payload?.alarms && payload?.timer) {
            const next = normalizeSettings(payload);
            setSettings(next);
            setWifiDraft(next.wifi);
          }
          setBanner(null);
        } catch (e) {
          setBanner(e instanceof Error ? e.message : String(e));
        }
      }
    })();
    return () => {
      void bleClient.disconnect(false);
    };
  }, []);

  const withBusy = useCallback(async (fn: () => Promise<void>) => {
    setBusy(true);
    try {
      await fn();
      setBanner(null);
    } catch (e) {
      setBanner(e instanceof Error ? e.message : String(e));
    } finally {
      setBusy(false);
    }
  }, []);

  const connect = useCallback(async () => {
    await withBusy(async () => {
      await bleClient.connect();
      if (bleClient.state === 'Connected') {
        const response = await bleClient.sendBuilt(commands.getSettings());
        const payload = response.payload as DeviceSettings;
        if (payload?.alarms && payload?.timer) {
          const next = normalizeSettings(payload);
          setSettings(next);
          setWifiDraft(next.wifi);
        }
      } else if (bleClient.stateMessage) {
        throw new Error(bleClient.stateMessage);
      }
    });
  }, [withBusy]);

  const loadSettings = useCallback(async () => {
    await withBusy(async () => {
      const response = await bleClient.sendBuilt(commands.getSettings());
      const payload = response.payload as DeviceSettings;
      const next = normalizeSettings(payload);
      setSettings(next);
      setWifiDraft(next.wifi);
    });
  }, [withBusy]);

  const saveSettings = useCallback(async () => {
    await withBusy(async () => {
      await bleClient.sendBuilt(commands.setSettings(settings));
      await bleClient.refreshStatus();
    });
  }, [settings, withBusy]);

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
      await bleClient.sendBuilt(commands.setSettings({ ...settings, wifi: settings.wifi }));
      await bleClient.sendBuilt(commands.startTimer());
      await bleClient.refreshStatus();
    });
  }, [settings, withBusy]);

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
      settings,
      setSettings,
      wifiDraft,
      setWifiDraft,
      testMode,
      setTestMode,
      busy,
      banner,
      connect,
      loadSettings,
      saveSettings,
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
      settings,
      wifiDraft,
      testMode,
      busy,
      banner,
      connect,
      loadSettings,
      saveSettings,
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
