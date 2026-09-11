import React, { useState } from 'react';
import { Pressable, SafeAreaView, StyleSheet, Text, View } from 'react-native';
import { StatusBar } from 'expo-status-bar';
import { StatusHeader } from './src/components/StatusHeader';
import { DeviceProvider } from './src/context/DeviceContext';
import { AlarmsScreen } from './src/screens/AlarmsScreen';
import { TestScreen } from './src/screens/TestScreen';
import { TimerScreen } from './src/screens/TimerScreen';
import { WifiScreen } from './src/screens/WifiScreen';
import { colors } from './src/theme';

type TabId = 'alarms' | 'timer' | 'test' | 'wifi';

const TABS: { id: TabId; label: string }[] = [
  { id: 'alarms', label: 'Будильники' },
  { id: 'timer', label: 'Таймер' },
  { id: 'test', label: 'Тест' },
  { id: 'wifi', label: 'WiFi' },
];

function AppShell() {
  const [tab, setTab] = useState<TabId>('alarms');

  return (
    <SafeAreaView style={styles.root}>
      <StatusBar style="light" />
      <StatusHeader />
      <View style={styles.body}>
        {tab === 'alarms' ? <AlarmsScreen /> : null}
        {tab === 'timer' ? <TimerScreen /> : null}
        {tab === 'test' ? <TestScreen /> : null}
        {tab === 'wifi' ? <WifiScreen /> : null}
      </View>
      <View style={styles.tabs}>
        {TABS.map((item) => (
          <Pressable
            key={item.id}
            style={[styles.tab, tab === item.id && styles.tabOn]}
            onPress={() => setTab(item.id)}
          >
            <Text style={[styles.tabText, tab === item.id && styles.tabTextOn]}>{item.label}</Text>
          </Pressable>
        ))}
      </View>
    </SafeAreaView>
  );
}

export default function App() {
  return (
    <DeviceProvider>
      <AppShell />
    </DeviceProvider>
  );
}

const styles = StyleSheet.create({
  root: { flex: 1, backgroundColor: colors.bg },
  body: { flex: 1 },
  tabs: {
    flexDirection: 'row',
    borderTopColor: colors.border,
    borderTopWidth: 1,
    backgroundColor: colors.panel,
  },
  tab: { flex: 1, paddingVertical: 14, alignItems: 'center' },
  tabOn: { borderTopWidth: 2, borderTopColor: colors.accent },
  tabText: { color: colors.muted, fontWeight: '600', fontSize: 12 },
  tabTextOn: { color: colors.accent },
});
