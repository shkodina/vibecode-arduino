import React from 'react';
import { Pressable, StyleSheet, Switch, Text, TextInput, View } from 'react-native';
import type { AlarmConfig } from '../model/types';
import { WEEKDAY_LABELS } from '../model/types';
import { colors, spacing } from '../theme';
import { ModeForm } from './ModeForm';

type Props = {
  alarm: AlarmConfig;
  onChange: (alarm: AlarmConfig) => void;
};

export function AlarmCard({ alarm, onChange }: Props) {
  const toggleDay = (bit: number) => {
    const mask = alarm.weekdaysMask ^ (1 << bit);
    onChange({ ...alarm, weekdaysMask: mask });
  };

  return (
    <View style={styles.card}>
      <View style={styles.header}>
        <Text style={styles.title}>Будильник {alarm.id}</Text>
        <Switch
          value={alarm.enabled}
          onValueChange={(enabled) => onChange({ ...alarm, enabled })}
          trackColor={{ true: colors.accent, false: colors.border }}
        />
      </View>
      <View style={styles.row}>
        <View style={styles.field}>
          <Text style={styles.label}>Часы</Text>
          <TextInput
            style={styles.input}
            keyboardType="number-pad"
            value={String(alarm.hour)}
            onChangeText={(t) => onChange({ ...alarm, hour: Number(t.replace(/[^0-9]/g, '') || 0) })}
          />
        </View>
        <View style={styles.field}>
          <Text style={styles.label}>Минуты</Text>
          <TextInput
            style={styles.input}
            keyboardType="number-pad"
            value={String(alarm.minute)}
            onChangeText={(t) => onChange({ ...alarm, minute: Number(t.replace(/[^0-9]/g, '') || 0) })}
          />
        </View>
      </View>
      <View style={styles.days}>
        {WEEKDAY_LABELS.map((label, bit) => {
          const on = (alarm.weekdaysMask & (1 << bit)) !== 0;
          return (
            <Pressable key={label} style={[styles.day, on && styles.dayOn]} onPress={() => toggleDay(bit)}>
              <Text style={styles.dayText}>{label}</Text>
            </Pressable>
          );
        })}
      </View>
      <ModeForm mode={alarm.mode} onChange={(mode) => onChange({ ...alarm, mode })} />
    </View>
  );
}

const styles = StyleSheet.create({
  card: {
    backgroundColor: colors.card,
    borderColor: colors.border,
    borderWidth: 1,
    borderRadius: 12,
    padding: spacing.md,
    gap: spacing.sm,
    marginBottom: spacing.md,
  },
  header: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center' },
  title: { color: colors.accent, fontSize: 18, fontWeight: '700' },
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
  days: { flexDirection: 'row', flexWrap: 'wrap', gap: 6 },
  day: {
    paddingHorizontal: 8,
    paddingVertical: 6,
    borderRadius: 6,
    borderWidth: 1,
    borderColor: colors.border,
  },
  dayOn: { backgroundColor: colors.accentDim, borderColor: colors.accent },
  dayText: { color: colors.text, fontSize: 12, fontWeight: '600' },
});
