import React, { useState } from 'react';
import { Pressable, ScrollView, StyleSheet, Text, View } from 'react-native';
import { AlarmCard } from '../components/AlarmCard';
import { useDevice } from '../context/DeviceContext';
import { colors, spacing } from '../theme';
import { validateAlarm } from '../validation';

export function AlarmsScreen() {
  const { settings, setSettings, saveSettings, busy } = useDevice();
  const [localError, setLocalError] = useState<string | null>(null);

  return (
    <ScrollView style={styles.scroll} contentContainerStyle={styles.content}>
      {localError ? <Text style={styles.error}>{localError}</Text> : null}
      {settings.alarms.map((alarm, index) => (
        <AlarmCard
          key={alarm.id}
          alarm={alarm}
          onChange={(next) => {
            setSettings((prev) => {
              const alarms = [...prev.alarms];
              alarms[index] = next;
              return { ...prev, alarms };
            });
          }}
        />
      ))}
      <Pressable
        style={styles.btn}
        disabled={busy}
        onPress={() => {
          for (const alarm of settings.alarms) {
            const err = validateAlarm(alarm);
            if (err) {
              setLocalError(err);
              return;
            }
          }
          setLocalError(null);
          void saveSettings();
        }}
      >
        <Text style={styles.btnText}>Сохранить</Text>
      </Pressable>
      <View style={{ height: 40 }} />
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  scroll: { flex: 1, backgroundColor: colors.bg },
  content: { padding: spacing.md },
  error: { color: colors.danger, marginBottom: spacing.sm },
  btn: {
    backgroundColor: colors.accent,
    borderRadius: 10,
    padding: 14,
    alignItems: 'center',
  },
  btnText: { color: '#111', fontWeight: '800', fontSize: 16 },
});
