import type { AlarmConfig, DeviceSettings, LightModeConfig, WifiConfig } from '../model/types';

let nextRequestId = 1;

export function nextId(): string {
  const id = String(nextRequestId);
  nextRequestId += 1;
  return id;
}

export function buildCommand(
  command: string,
  payload?: unknown,
  requestId: string = nextId(),
): string {
  const body: Record<string, unknown> = { requestId, command };
  if (payload !== undefined) {
    body.payload = payload;
  }
  return JSON.stringify(body);
}

export const commands = {
  getSettings: () => buildCommand('getSettings'),
  setSettings: (settings: DeviceSettings) => buildCommand('setSettings', settings),
  setAlarm: (alarm: AlarmConfig) => buildCommand('setAlarm', alarm),
  getWifi: () => buildCommand('getWifi'),
  setWifi: (wifi: WifiConfig) => buildCommand('setWifi', wifi),
  getStatus: () => buildCommand('getStatus'),
  setTime: () => buildCommand('setTime', { epoch: Math.floor(Date.now() / 1000) }),
  stop: () => buildCommand('stop'),
  startTimer: () => buildCommand('startTimer'),
  startTest: (mode: LightModeConfig) => buildCommand('startTest', mode),
  stopTest: () => buildCommand('stopTest'),
};
