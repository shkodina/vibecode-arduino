import React from 'react';
import { Pressable, ScrollView, StyleSheet, Text, TextInput, View } from 'react-native';
import { changedAppPrefs, DEFAULT_APP_PREFS, type AppPrefs } from '../model/appPrefs';
import { useDevice } from '../context/DeviceContext';
import { colors, spacing } from '../theme';

function NumberPref({
  label,
  hint,
  value,
  field,
  onChange,
}: {
  label: string;
  hint: string;
  value: number;
  field: keyof AppPrefs;
  onChange: (field: keyof AppPrefs, n: number) => void;
}) {
  const changed = value !== DEFAULT_APP_PREFS[field];
  return (
    <View style={styles.field}>
      <Text style={styles.label}>
        {label}
        {changed ? ' · изменено' : ''}
      </Text>
      <TextInput
        style={styles.input}
        keyboardType="number-pad"
        value={String(value)}
        onChangeText={(t) => onChange(field, Number(t.replace(/[^0-9]/g, '') || 0))}
      />
      <Text style={styles.hintSmall}>{hint}</Text>
    </View>
  );
}

export function SettingsScreen() {
  const { wifiDraft, setWifiDraft, loadWifi, saveWifi, appPrefs, setAppPrefs, busy } = useDevice();
  const changedCount = Object.keys(changedAppPrefs(appPrefs)).length;

  const setPref = (field: keyof AppPrefs, n: number) => {
    setAppPrefs({ ...appPrefs, [field]: n });
  };

  return (
    <ScrollView style={styles.scroll} contentContainerStyle={styles.content}>
      <Text style={styles.section}>Связь BLE</Text>
      <Text style={styles.hint}>
        Таймауты и ретраи. Баннер только если все попытки кончились. Изменённые
        значения помнит приложение ({changedCount}).
      </Text>
      <NumberPref
        label="Таймаут ответа, мс"
        hint={`завод ${DEFAULT_APP_PREFS.commandTimeoutMs}`}
        value={appPrefs.commandTimeoutMs}
        field="commandTimeoutMs"
        onChange={setPref}
      />
      <NumberPref
        label="Повторы команды"
        hint={`завод ${DEFAULT_APP_PREFS.commandRetries}`}
        value={appPrefs.commandRetries}
        field="commandRetries"
        onChange={setPref}
      />
      <NumberPref
        label="Проверка связи, сек"
        hint={`завод ${DEFAULT_APP_PREFS.keepaliveIntervalSec}`}
        value={appPrefs.keepaliveIntervalSec}
        field="keepaliveIntervalSec"
        onChange={setPref}
      />
      <NumberPref
        label="Повторы переподключения"
        hint={`завод ${DEFAULT_APP_PREFS.keepaliveRetries}`}
        value={appPrefs.keepaliveRetries}
        field="keepaliveRetries"
        onChange={setPref}
      />

      <Text style={styles.section}>Время контроллера</Text>
      <NumberPref
        label="Проверка времени, сек"
        hint={`завод ${DEFAULT_APP_PREFS.timeSyncIntervalSec}`}
        value={appPrefs.timeSyncIntervalSec}
        field="timeSyncIntervalSec"
        onChange={setPref}
      />
      <NumberPref
        label="Рассинхрон, сек"
        hint={`завод ${DEFAULT_APP_PREFS.timeSyncMaxDriftSec} (3 мин). Если больше — шлём setTime`}
        value={appPrefs.timeSyncMaxDriftSec}
        field="timeSyncMaxDriftSec"
        onChange={setPref}
      />
      <Pressable style={[styles.btn, styles.secondary]} onPress={() => setAppPrefs(DEFAULT_APP_PREFS)}>
        <Text style={styles.btnTextLight}>Сбросить настройки приложения</Text>
      </Pressable>

      <Text style={styles.section}>WiFi устройства</Text>
      <Text style={styles.hint}>SSID и пароль пишутся в NVS через getWifi / setWifi.</Text>
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
        <Text style={styles.btnTextLight}>Прочитать WiFi</Text>
      </Pressable>
      <Pressable style={styles.btn} disabled={busy} onPress={() => void saveWifi()}>
        <Text style={styles.btnText}>Сохранить WiFi</Text>
      </Pressable>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  scroll: { flex: 1, backgroundColor: colors.bg },
  content: { padding: spacing.md, gap: spacing.md, paddingBottom: 40 },
  section: { color: colors.accent, fontWeight: '800', fontSize: 16, marginTop: 8 },
  hint: { color: colors.muted },
  hintSmall: { color: colors.muted, fontSize: 11 },
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
