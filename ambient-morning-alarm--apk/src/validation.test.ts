import assert from 'node:assert/strict';
import { defaultLightMode, modeForType, normalizeLightMode } from './model/types';
import { validateMode } from './validation';

const strobe = defaultLightMode('strobe');
assert.equal(strobe.type, 'strobe');
assert.equal(strobe.totalSeconds, 10);
assert.equal(validateMode(strobe), null);

const pulse = defaultLightMode('pulse');
assert.equal(pulse.glowSeconds, 5);
assert.equal(pulse.fadeSeconds, 5);
assert.equal(validateMode(pulse), null);

assert.equal(validateMode({ ...pulse, darkSeconds: 0 }), 'для pulse: darkSeconds > 0');
assert.equal(validateMode({ ...strobe, totalSeconds: 0 }), 'totalSeconds > 0');

const oldPulse = normalizeLightMode({
  type: 'pulse',
  startBrightness: 5,
  finishBrightness: 100,
  rampSeconds: 5,
  darkSeconds: 5,
  totalSeconds: 60,
});
assert.equal(oldPulse.glowSeconds, 0);
assert.equal(oldPulse.fadeSeconds, 0);

const switched = modeForType(defaultLightMode('ramp'), 'strobe');
assert.equal(switched.type, 'strobe');
assert.ok(switched.totalSeconds >= 1);

console.log('validation tests ok');
