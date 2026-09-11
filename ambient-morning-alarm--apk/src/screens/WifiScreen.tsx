import React from 'react';
import { Pressable, ScrollView, StyleSheet, Text, TextInput, View } from 'react-native';
import { useDevice } from '../context/DeviceContext';
import { colors, spacing } from '../theme';

export function WifiScreen() {
  const { wifiDraft, setWifiDraft, loadWifi, saveWifi, busy } = useDevice();

  return (
    <ScrollView style={styles.scroll} contentContainerStyle={styles.content}>
      <Text style={styles.hint}>SSID и пароль пишутся в NVS устройства через getWifi / setWifi.</Text>
      <View style={styles.field}>
        <Text style={styles.label}>SSID</Text>
        <TextInput
          style={styles.input}
          value={wifiDraft.ssid}
          autoCapitalize="none"
          onChangeText={(ssid) => setWifiDraft((prev) => ({ ...prev, ssid }))}
        />
      </View>
      <View style={styles.field}>
        <Text style={styles.label}>Пароль</Text>
        <TextInput
          style={styles.input}
          value={wifiDraft.password}
          autoCapitalize="none"
          secureTextEntry
          onChangeText={(password) => setWifiDraft((prev) => ({ ...prev, password }))}
        />
      </View>
      <Pressable style={[styles.btn, styles.secondary]} disabled={busy} onPress={() => void loadWifi()}>
        <Text style={styles.btnTextLight}>Прочитать</Text>
      </Pressable>
      <Pressable style={styles.btn} disabled={busy} onPress={() => void saveWifi()}>
        <Text style={styles.btnText}>Сохранить WiFi</Text>
      </Pressable>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  scroll: { flex: 1, backgroundColor: colors.bg },
  content: { padding: spacing.md, gap: spacing.md },
  hint: { color: colors.muted },
  field: { gap: 4 },
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
  btnText: { color: '#111', fontWeight: '800', fontSize: 16 },
  btnTextLight: { color: colors.text, fontWeight: '800', fontSize: 16 },
});
