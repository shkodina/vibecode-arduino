import React from 'react';
import { Pressable, StyleSheet, Text, TextInput, View } from 'react-native';
import { MODE_TYPES, modeForType, type LightModeConfig, type LightModeType } from '../model/types';
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

const TYPE_LABELS: Record<LightModeType, string> = {
  ramp: 'ramp',
  pulse: 'pulse',
  strobe: 'строб',
};

export function ModeForm({ mode, onChange, forceType }: Props) {
  const type = forceType || mode.type;
  const pulseOrRamp = type === 'pulse' || type === 'ramp';

  return (
    <View style={styles.wrap}>
      {!forceType ? (
        <View style={styles.row}>
          {MODE_TYPES.map((t) => (
            <Pressable
              key={t}
              style={[styles.chip, type === t && styles.chipOn]}
              onPress={() => onChange(modeForType(mode, t))}
            >
              <Text style={styles.chipText}>{TYPE_LABELS[t]}</Text>
            </Pressable>
          ))}
        </View>
      ) : null}

      {type === 'strobe' ? (
        <Text style={styles.hint}>
          Стробоскоп: вспышка/пауза 100 мс, яркость 100%. Снаружи только длительность.
        </Text>
      ) : null}

      {pulseOrRamp ? (
        <>
          <View style={styles.row}>
            <NumberField
              label="старт %"
              value={mode.startBrightness}
              onChange={(startBrightness) => onChange({ ...mode, type, startBrightness })}
            />
            <NumberField
              label="финиш %"
              value={mode.finishBrightness}
              onChange={(finishBrightness) => onChange({ ...mode, type, finishBrightness })}
            />
          </View>
          <View style={styles.row}>
            <NumberField
              label="розжиг, с"
              value={mode.rampSeconds}
              onChange={(rampSeconds) => onChange({ ...mode, type, rampSeconds })}
            />
            <NumberField
              label="всего, с"
              value={mode.totalSeconds}
              onChange={(totalSeconds) => onChange({ ...mode, type, totalSeconds })}
            />
          </View>
        </>
      ) : (
        <NumberField
          label="всего, с"
          value={mode.totalSeconds}
          onChange={(totalSeconds) => onChange({ ...mode, type, totalSeconds })}
        />
      )}

      {type === 'pulse' ? (
        <View style={styles.row}>
          <NumberField
            label="свечение, с"
            value={mode.glowSeconds}
            onChange={(glowSeconds) => onChange({ ...mode, type, glowSeconds })}
          />
          <NumberField
            label="затухание, с"
            value={mode.fadeSeconds}
            onChange={(fadeSeconds) => onChange({ ...mode, type, fadeSeconds })}
          />
        </View>
      ) : null}
      {type === 'pulse' ? (
        <NumberField
          label="темнота, с"
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
  hint: { color: colors.muted, fontSize: 12, lineHeight: 16 },
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
