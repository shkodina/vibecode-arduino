import type { ActiveRun, DeviceSettings, LightModeConfig } from './types';

export function parseDeviceTimeMs(iso: string | null | undefined): number | null {
  if (!iso) {
    return null;
  }
  const match = iso.match(/^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})/);
  if (!match) {
    return null;
  }
  return new Date(
    Number(match[1]),
    Number(match[2]) - 1,
    Number(match[3]),
    Number(match[4]),
    Number(match[5]),
    Number(match[6]),
  ).getTime();
}

export function resolveActiveMode(
  run: ActiveRun,
  settings: DeviceSettings,
  testMode: LightModeConfig,
): LightModeConfig | null {
  if (run.type === 'alarm' && run.alarmId != null) {
    return settings.alarms.find((alarm) => alarm.id === run.alarmId)?.mode ?? null;
  }
  if (run.type === 'timer') {
    return settings.timer.mode;
  }
  if (run.type === 'test') {
    return testMode;
  }
  return null;
}

export function describeLightPhase(mode: LightModeConfig, elapsedSec: number): string {
  if (mode.type === 'strobe') {
    return 'строб';
  }
  if (mode.type === 'ramp') {
    return elapsedSec < mode.rampSeconds ? 'розжиг' : 'удержание';
  }
  const cycle = mode.rampSeconds + mode.glowSeconds + mode.fadeSeconds + Math.max(mode.darkSeconds, 1);
  if (cycle <= 0) {
    return 'пульс';
  }
  let t = ((elapsedSec % cycle) + cycle) % cycle;
  if (t < mode.rampSeconds) {
    return 'розжиг';
  }
  t -= mode.rampSeconds;
  if (t < mode.glowSeconds) {
    return 'свечение';
  }
  t -= mode.glowSeconds;
  if (mode.fadeSeconds > 0 && t < mode.fadeSeconds) {
    return 'затухание';
  }
  return 'темнота';
}

export function describeActiveRun(
  run: ActiveRun,
  mode: LightModeConfig | null,
  displayedRemaining: number,
): string {
  if (run.type === 'timer' && mode && displayedRemaining > mode.totalSeconds) {
    return 'ожидание таймера';
  }
  if (!mode) {
    return run.mode || 'свет';
  }
  const elapsed = Math.max(0, mode.totalSeconds - displayedRemaining);
  return describeLightPhase(mode, elapsed);
}
