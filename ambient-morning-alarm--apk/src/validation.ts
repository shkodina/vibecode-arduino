import type { AlarmConfig, LightModeConfig, TimerConfig } from './model/types';

export function validateMode(mode: LightModeConfig): string | null {
  if (mode.type !== 'ramp' && mode.type !== 'pulse' && mode.type !== 'strobe') {
    return 'Режим должен быть ramp, pulse или strobe';
  }
  if (mode.totalSeconds <= 0) {
    return 'totalSeconds > 0';
  }
  if (mode.type === 'strobe') {
    return null;
  }
  if (mode.startBrightness < 0 || mode.startBrightness > 100) {
    return 'startBrightness: 0..100';
  }
  if (mode.finishBrightness < 0 || mode.finishBrightness > 100) {
    return 'finishBrightness: 0..100';
  }
  if (mode.finishBrightness < mode.startBrightness) {
    return 'finishBrightness >= startBrightness';
  }
  if (mode.rampSeconds <= 0) {
    return 'rampSeconds > 0';
  }
  if (mode.glowSeconds < 0 || mode.fadeSeconds < 0) {
    return 'glowSeconds и fadeSeconds >= 0';
  }
  if (mode.type === 'pulse' && mode.darkSeconds <= 0) {
    return 'для pulse: darkSeconds > 0';
  }
  if (mode.type === 'ramp' && mode.darkSeconds !== 0) {
    return 'для ramp: darkSeconds = 0';
  }
  return null;
}

export function validateAlarm(alarm: AlarmConfig): string | null {
  if (alarm.hour < 0 || alarm.hour > 23) {
    return `Будильник ${alarm.id}: часы 0..23`;
  }
  if (alarm.minute < 0 || alarm.minute > 59) {
    return `Будильник ${alarm.id}: минуты 0..59`;
  }
  const modeError = validateMode(alarm.mode);
  if (modeError) {
    return `Будильник ${alarm.id}: ${modeError}`;
  }
  return null;
}

export function validateTimer(timer: TimerConfig): string | null {
  if (timer.hours < 0 || timer.hours > 23) {
    return 'Часы таймера: 0..23';
  }
  if (timer.minutes < 0 || timer.minutes > 59) {
    return 'Минуты таймера: 0..59';
  }
  return validateMode(timer.mode);
}
