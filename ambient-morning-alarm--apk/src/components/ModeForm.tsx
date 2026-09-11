import React from 'react';
import { Pressable, StyleSheet, Text, TextInput, View } from 'react-native';
import type { LightModeConfig, LightModeType } from '../model/types';
import { colors, spacing } from '../theme';

type Props = {
  mode: LightModeConfig;
  onChange: (mode: LightModeConfig) => void;
  forceType?: LightModeType;
};

function NumberField({
  label,
  value,
  onChange,
}: {
  label: string;
  value: number;
  onChange: (n: number) => void;
}) {
  return (
    <View style={styles.field}>
      <Text style={styles.label}>{label}</Text>
      <TextInput
        style={styles.input}
        keyboardType="number-pad"
        value={String(value)}
        onChangeText={(t) => onChange(Number(t.replace(/[^0-9]/g, '') || 0))}
      />
    </View>
  );
}

export function ModeForm({ mode, onChange, forceType }: Props) {
  const type = forceType || mode.type;
  return (
    <View style={styles.wrap}>
      {!forceType ? (
        <View style={styles.row}>
          {(['ramp', 'pulse'] as LightModeType[]).map((t) => (
            <Pressable
              key={t}
              style={[styles.chip, type === t && styles.chipOn]}
              onPress={() =>
                onChange({
                  ...mode,
                  type: t,
                  darkSeconds: t === 'ramp' ? 0 : Math.max(mode.darkSeconds, 1),
                })
              }
            >
              <Text style={styles.chipText}>{t}</Text>
            </Pressable>
          ))}
        </View>
      ) : null}
      <View style={styles.row}>
        <NumberField
          label="startBrightness"
          value={mode.startBrightness}
          onChange={(startBrightness) => onChange({ ...mode, type, startBrightness })}
        />
        <NumberField
          label="finishBrightness"
          value={mode.finishBrightness}
          onChange={(finishBrightness) => onChange({ ...mode, type, finishBrightness })}
        />
      </View>
      <View style={styles.row}>
        <NumberField
          label="rampSeconds"
          value={mode.rampSeconds}
          onChange={(rampSeconds) => onChange({ ...mode, type, rampSeconds })}
        />
        <NumberField
          label="totalSeconds"
          value={mode.totalSeconds}
          onChange={(totalSeconds) => onChange({ ...mode, type, totalSeconds })}
        />
      </View>
      {type === 'pulse' ? (
        <NumberField
          label="darkSeconds"
          value={mode.darkSeconds}
          onChange={(darkSeconds) => onChange({ ...mode, type, darkSeconds })}
        />
      ) : null}
    </View>
  );
}

const styles = StyleSheet.create({
  wrap: { gap: spacing.sm },
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
  chip: {
    paddingHorizontal: 14,
    paddingVertical: 8,
    borderRadius: 8,
    borderWidth: 1,
    borderColor: colors.border,
    backgroundColor: colors.panel,
  },
  chipOn: { borderColor: colors.accent, backgroundColor: colors.accentDim },
  chipText: { color: colors.text, fontWeight: '600' },
});
