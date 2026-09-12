import type { DeviceStatus, NextAlarm } from './types';

function pad2(value: number): string {
  return String(value).padStart(2, '0');
}

export function formatEta(seconds: number): string {
  const sec = Math.max(0, Math.floor(Number.isFinite(seconds) ? seconds : 0));
  const days = Math.floor(sec / 86400);
  const hours = Math.floor((sec % 86400) / 3600);
  const minutes = Math.floor((sec % 3600) / 60);
  const parts: string[] = [];
  if (days) {
    parts.push(`${days} д`);
  }
  if (hours) {
    parts.push(`${hours} ч`);
  }
  if (minutes) {
    parts.push(`${minutes} мин`);
  }
  if (parts.length === 0) {
    return 'меньше минуты';
  }
  return parts.join(' ');
}

export function describeNextAlarm(status: DeviceStatus | null, elapsedSinceStatus = 0): string | null {
  if (!status) {
    return null;
  }
  if (!status.ntpSynced) {
    return 'Нет синхронизации времени — ближайший будильник неизвестен';
  }
  const next = status.nextAlarm;
  if (!next) {
    return 'Нет включённых будильников';
  }
  const remaining = Math.max(0, next.inSeconds - Math.max(0, elapsedSinceStatus));
  return `Ближайший: Будильник ${next.id} в ${pad2(next.hour)}:${pad2(next.minute)}, через ${formatEta(remaining)}`;
}

export function remainingNextAlarmSeconds(next: NextAlarm | null | undefined, elapsedSinceStatus = 0): number {
  if (!next) {
    return 0;
  }
  return Math.max(0, next.inSeconds - Math.max(0, elapsedSinceStatus));
}
