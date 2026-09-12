import React, { useEffect, useState } from 'react';
import { ActivityIndicator, Pressable, StyleSheet, Text, View } from 'react-native';
import { describeActiveRun, resolveActiveMode } from '../model/lightPhase';
import { describeNextAlarm } from '../model/nextAlarm';
import { useDevice } from '../context/DeviceContext';
import { colors, spacing } from '../theme';

export function StatusHeader() {
  const { status, statusAt, settings, testMode, bleState, deviceName, banner, busy, connect, stop } = useDevice();
  const [now, setNow] = useState(Date.now());
  const active = status?.activeRun;

  useEffect(() => {
    const id = setInterval(() => setNow(Date.now()), 1000);
    return () => clearInterval(id);
  }, []);

  const elapsedSinceStatus = statusAt ? Math.max(0, Math.floor((now - statusAt) / 1000)) : 0;
  const displayedRemaining = active
    ? Math.max(0, active.remainingSeconds - elapsedSinceStatus)
    : 0;
  const mode = active ? resolveActiveMode(active, settings, testMode) : null;
  const phase = active ? describeActiveRun(active, mode, displayedRemaining) : '';
  const stale = Boolean(active && bleState !== 'Connected');
  const nextAlarmText = describeNextAlarm(status, elapsedSinceStatus);
  const connectLabel = bleState === 'Connected' ? 'Обновить' : 'Подключить';

  return (
    <View style={styles.wrap}>
      <View style={styles.row}>
        <View style={{ flex: 1 }}>
          <Text style={styles.time}>{status?.currentTime || '—:—:—'}</Text>
          <Text style={styles.meta}>
            v{status?.firmwareVersion || '—'} · {bleState}
            {deviceName ? ` · ${deviceName}` : ''}
          </Text>
          <Text style={styles.meta}>
            WiFi: {status?.wifiConnected ? 'ok' : 'нет'} · NTP: {status?.ntpSynced ? 'ok' : 'нет'}
          </Text>
        </View>
        <Pressable style={styles.btn} onPress={() => void connect()} disabled={busy}>
          {busy ? <ActivityIndicator color={colors.text} /> : <Text style={styles.btnText}>{connectLabel}</Text>}
        </Pressable>
      </View>

      {banner ? (
        <View style={styles.banner}>
          <Text style={styles.bannerText}>{banner}</Text>
        </View>
      ) : null}

      {nextAlarmText ? (
        <View style={styles.next}>
          <Text style={styles.nextText}>{nextAlarmText}</Text>
        </View>
      ) : null}

      {active ? (
        <View style={styles.active}>
          <Text style={styles.activeTitle}>
            Активно: {active.type}
            {active.alarmId != null ? ` #${active.alarmId}` : ''} · {phase}
          </Text>
          <Text style={styles.meta}>
            осталось {displayedRemaining}s · яркость {active.brightness}%
            {stale ? ' · оценка, связи нет' : ''}
          </Text>
          <Pressable style={[styles.btn, styles.stop]} onPress={() => void stop()} disabled={busy}>
            <Text style={styles.btnText}>Остановить</Text>
          </Pressable>
        </View>
      ) : null}
    </View>
  );
}

const styles = StyleSheet.create({
  wrap: {
    backgroundColor: colors.panel,
    borderBottomColor: colors.border,
    borderBottomWidth: 1,
    padding: spacing.md,
    gap: spacing.sm,
  },
  row: { flexDirection: 'row', gap: spacing.sm, alignItems: 'center' },
  time: { color: colors.text, fontSize: 22, fontWeight: '700' },
  meta: { color: colors.muted, fontSize: 12, marginTop: 2 },
  btn: {
    backgroundColor: colors.accentDim,
    borderColor: colors.accent,
    borderWidth: 1,
    borderRadius: 8,
    paddingHorizontal: 12,
    paddingVertical: 10,
    minWidth: 110,
    alignItems: 'center',
  },
  stop: { backgroundColor: colors.danger, borderColor: colors.danger, marginTop: 8 },
  btnText: { color: colors.text, fontWeight: '700' },
  banner: {
    backgroundColor: '#3a1515',
    borderColor: colors.danger,
    borderWidth: 1,
    borderRadius: 8,
    padding: spacing.sm,
  },
  bannerText: { color: colors.danger },
  next: {
    backgroundColor: '#1a1a12',
    borderColor: colors.border,
    borderWidth: 1,
    borderRadius: 8,
    padding: spacing.sm,
  },
  nextText: { color: colors.accent, fontWeight: '600' },
  active: {
    backgroundColor: '#2a1a0a',
    borderColor: colors.accent,
    borderWidth: 1,
    borderRadius: 8,
    padding: spacing.sm,
  },
  activeTitle: { color: colors.accent, fontWeight: '700' },
});
