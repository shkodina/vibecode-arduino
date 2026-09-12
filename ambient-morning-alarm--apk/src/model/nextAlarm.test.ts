import assert from 'node:assert/strict';
import { asDeviceSettings, defaultAlarm, normalizeSettings } from './types';
import { describeNextAlarm, formatEta } from './nextAlarm';

assert.equal(formatEta(0), 'меньше минуты');
assert.equal(formatEta(59), 'меньше минуты');
assert.equal(formatEta(60), '1 мин');
assert.equal(formatEta(75600), '21 ч');
assert.equal(formatEta(90000), '1 д 1 ч');
assert.equal(formatEta(90060), '1 д 1 ч 1 мин');

assert.equal(
  describeNextAlarm({
    currentTime: '2026-09-13T01:00:00',
    firmwareVersion: '00.00.016',
    ntpSynced: false,
    wifiConnected: false,
    activeRun: null,
    nextAlarm: null,
  }),
  'Нет синхронизации времени — ближайший будильник неизвестен',
);

assert.equal(
  describeNextAlarm({
    currentTime: '2026-09-13T01:00:00',
    firmwareVersion: '00.00.016',
    ntpSynced: true,
    wifiConnected: false,
    activeRun: null,
    nextAlarm: null,
  }),
  'Нет включённых будильников',
);

assert.equal(
  describeNextAlarm(
    {
      currentTime: '2026-09-13T01:00:00',
      firmwareVersion: '00.00.016',
      ntpSynced: true,
      wifiConnected: true,
      activeRun: null,
      nextAlarm: { id: 0, hour: 7, minute: 0, inSeconds: 8160 },
    },
    60,
  ),
  'Ближайший: Будильник 0 в 07:00, через 2 ч 15 мин',
);

const parsed = asDeviceSettings(
  JSON.stringify({
    alarms: [{ ...defaultAlarm(3), id: 3, enabled: true, hour: 6, minute: 15 }],
    timer: { hours: 1, minutes: 5, mode: { type: 'pulse' } },
  }),
);
assert.ok(parsed);
const settings = normalizeSettings(parsed);
assert.equal(settings.alarms[3].enabled, true);
assert.equal(settings.alarms[3].hour, 6);
assert.equal(settings.alarms[3].minute, 15);
assert.equal(settings.alarms[0].enabled, false);
assert.equal(settings.timer.hours, 1);

assert.equal(asDeviceSettings(null), null);
assert.equal(asDeviceSettings('{"timer":{}}'), null);

console.log('nextAlarm/settings tests ok');
