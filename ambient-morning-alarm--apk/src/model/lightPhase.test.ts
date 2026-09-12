import assert from 'node:assert/strict';
import { changedAppPrefs, DEFAULT_APP_PREFS, normalizeAppPrefs } from './appPrefs';
import { describeActiveRun, describeLightPhase, parseDeviceTimeMs } from './lightPhase';
import { defaultLightMode } from './types';

assert.equal(describeLightPhase(defaultLightMode('strobe'), 1), 'строб');
assert.equal(describeLightPhase(defaultLightMode('ramp'), 10), 'розжиг');
assert.equal(describeLightPhase(defaultLightMode('ramp'), 1300), 'удержание');

const pulse = defaultLightMode('pulse');
assert.equal(describeLightPhase(pulse, 2), 'розжиг');
assert.equal(describeLightPhase(pulse, 7), 'свечение');
assert.equal(describeLightPhase(pulse, 12), 'затухание');
assert.equal(describeLightPhase(pulse, 17), 'темнота');

const wait = describeActiveRun(
  { type: 'timer', alarmId: null, mode: 'pulse', startedAt: null, remainingSeconds: 1800, brightness: 0 },
  pulse,
  1800,
);
assert.equal(wait, 'ожидание таймера');

const t = parseDeviceTimeMs('2026-09-12T10:15:00');
assert.ok(t != null);

const prefs = normalizeAppPrefs({ commandTimeoutMs: 25000 });
assert.equal(prefs.commandTimeoutMs, 25000);
assert.deepEqual(changedAppPrefs(prefs), { commandTimeoutMs: 25000 });
assert.deepEqual(changedAppPrefs(DEFAULT_APP_PREFS), {});

console.log('lightPhase/appPrefs tests ok');
