import React, { useState } from 'react';
import { Pressable, ScrollView, StyleSheet, Text, TextInput, View } from 'react-native';
import { ModeForm } from '../components/ModeForm';
import { useDevice } from '../context/DeviceContext';
import { colors, spacing } from '../theme';
import { validateTimer } from '../validation';

export function TimerScreen() {
  const { settings, setSettings, saveSettings, startTimer, stop, busy } = useDevice();
  const [localError, setLocalError] = useState<string | null>(null);
  const timer = settings.timer;

  return (
    <ScrollView style={styles.scroll} contentContainerStyle={styles.content}>
      {localError ? <Text style={styles.error}>{localError}</Text> : null}
      <View style={styles.row}>
        <View style={styles.field}>
          <Text style={styles.label}>Часы ожидания</Text>
          <TextInput
            style={styles.input}
            keyboardType="number-pad"
            value={String(timer.hours)}
            onChangeText={(t) =>
              setSettings((prev) => ({
                ...prev,
                timer: { ...prev.timer, hours: Number(t.replace(/[^0-9]/g, '') || 0) },
              }))
            }
          />
        </View>
        <View style={styles.field}>
          <Text style={styles.label}>Минуты ожидания</Text>
          <TextInput
            style={styles.input}
            keyboardType="number-pad"
            value={String(timer.minutes)}
            onChangeText={(t) =>
              setSettings((prev) => ({
                ...prev,
                timer: { ...prev.timer, minutes: Number(t.replace(/[^0-9]/g, '') || 0) },
              }))
            }
          />
        </View>
      </View>
      <ModeForm
        mode={timer.mode}
        forceType="pulse"
        onChange={(mode) => setSettings((prev) => ({ ...prev, timer: { ...prev.timer, mode } }))}
      />
      <Pressable
        style={styles.btn}
        disabled={busy}
        onPress={() => {
          const err = validateTimer(timer);
          if (err) {
            setLocalError(err);
            return;
          }
          setLocalError(null);
          void saveSettings();
        }}
      >
        <Text style={styles.btnText}>Сохранить</Text>
      </Pressable>
      <Pressable
        style={[styles.btn, styles.secondary]}
        disabled={busy}
        onPress={() => {
          const err = validateTimer(timer);
          if (err) {
            setLocalError(err);
            return;
          }
          setLocalError(null);
          void startTimer();
        }}
      >
        <Text style={styles.btnTextLight}>Запустить таймер</Text>
      </Pressable>
      <Pressable style={[styles.btn, styles.danger]} disabled={busy} onPress={() => void stop()}>
        <Text style={styles.btnTextLight}>Остановить</Text>
      </Pressable>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  scroll: { flex: 1, backgroundColor: colors.bg },
  content: { padding: spacing.md, gap: spacing.md },
  error: { color: colors.danger },
  row: { flexDirection: 'row', gap: spacing.sm },
  field: { flex: 1, gap: 4 },
  label: { color: colors.muted, fontSize: 12 },
  input: {
    backgroundColor: colors.input,
    borderColor: colors.border,
    borderWidth: 1,
    borderRadius: 8,
    color: colors.text,
    paddingHorizontal: 10,
    paddingVertical: 8,
    fontSize: 16,
  },
  btn: {
    backgroundColor: colors.accent,
    borderRadius: 10,
    padding: 14,
    alignItems: 'center',
  },
  secondary: { backgroundColor: colors.accentDim, borderWidth: 1, borderColor: colors.accent },
  danger: { backgroundColor: colors.danger },
  btnText: { color: '#111', fontWeight: '800', fontSize: 16 },
  btnTextLight: { color: colors.text, fontWeight: '800', fontSize: 16 },
});
