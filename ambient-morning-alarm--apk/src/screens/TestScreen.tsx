import React, { useState } from 'react';
import { Pressable, ScrollView, StyleSheet, Text } from 'react-native';
import { ModeForm } from '../components/ModeForm';
import { useDevice } from '../context/DeviceContext';
import { colors, spacing } from '../theme';
import { validateMode } from '../validation';

export function TestScreen() {
  const { testMode, setTestMode, startTest, stopTest, busy } = useDevice();
  const [localError, setLocalError] = useState<string | null>(null);

  return (
    <ScrollView style={styles.scroll} contentContainerStyle={styles.content}>
      {localError ? <Text style={styles.error}>{localError}</Text> : null}
      <Text style={styles.hint}>
        Тест не сохраняет параметры в настройки. По умолчанию стробоскоп 10 с, как в вебе.
      </Text>
      <ModeForm mode={testMode} onChange={setTestMode} />
      <Pressable
        style={styles.btn}
        disabled={busy}
        onPress={() => {
          const err = validateMode(testMode);
          if (err) {
            setLocalError(err);
            return;
          }
          setLocalError(null);
          void startTest();
        }}
      >
        <Text style={styles.btnText}>Старт</Text>
      </Pressable>
      <Pressable style={[styles.btn, styles.danger]} disabled={busy} onPress={() => void stopTest()}>
        <Text style={styles.btnTextLight}>Стоп</Text>
      </Pressable>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  scroll: { flex: 1, backgroundColor: colors.bg },
  content: { padding: spacing.md, gap: spacing.md },
  error: { color: colors.danger },
  hint: { color: colors.muted },
  btn: {
    backgroundColor: colors.accent,
    borderRadius: 10,
    padding: 14,
    alignItems: 'center',
  },
  danger: { backgroundColor: colors.danger },
  btnText: { color: '#111', fontWeight: '800', fontSize: 16 },
  btnTextLight: { color: colors.text, fontWeight: '800', fontSize: 16 },
});
