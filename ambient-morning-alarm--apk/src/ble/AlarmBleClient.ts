import { decode as b64decode, encode as b64encode } from 'base-64';
import { PermissionsAndroid, Platform } from 'react-native';
import { BleError, BleManager, Device, Subscription } from 'react-native-ble-plx';
import { DEFAULT_APP_PREFS } from '../model/appPrefs';
import type { BleClientState, CommandResponse, DeviceStatus } from '../model/types';
import { commands, nextId } from '../protocol/AlarmProtocol';
import {
  COMMAND_UUID,
  DEVICE_NAME_PREFIX,
  RESPONSE_UUID,
  SCAN_TIMEOUT_MS,
  SERVICE_UUID,
  STATUS_NOTIFY_UUID,
} from './constants';

type Listener = () => void;

function decodeBase64(value: string | null | undefined): string {
  if (!value) {
    return '';
  }
  return b64decode(value);
}

function encodeBase64(value: string): string {
  return b64encode(value);
}

function delay(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function withNewRequestId(json: string): string {
  const body = JSON.parse(json) as Record<string, unknown>;
  body.requestId = nextId();
  return JSON.stringify(body);
}

export class AlarmBleClient {
  private manager = new BleManager();
  private device: Device | null = null;
  private responseSub: Subscription | null = null;
  private statusSub: Subscription | null = null;
  private chunkBuffer: { count: number; parts: (string | null)[] } | null = null;
  private pending = new Map<
    string,
    { resolve: (v: CommandResponse) => void; reject: (e: Error) => void; timer: ReturnType<typeof setTimeout> }
  >();
  private listeners = new Set<Listener>();
  private commandTimeoutMs = DEFAULT_APP_PREFS.commandTimeoutMs;
  private commandRetries = DEFAULT_APP_PREFS.commandRetries;
  private writeTail: Promise<unknown> = Promise.resolve();
  private connectInFlight: Promise<void> | null = null;

  state: BleClientState = 'Disconnected';
  stateMessage = '';
  deviceName: string | null = null;
  lastStatus: DeviceStatus | null = null;
  lastStatusAt = 0;

  configure(opts: { commandTimeoutMs?: number; commandRetries?: number }) {
    if (opts.commandTimeoutMs != null) {
      this.commandTimeoutMs = opts.commandTimeoutMs;
    }
    if (opts.commandRetries != null) {
      this.commandRetries = opts.commandRetries;
    }
  }

  subscribe(listener: Listener): () => void {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }

  private emit() {
    this.listeners.forEach((l) => l());
  }

  private setState(state: BleClientState, message = '') {
    this.state = state;
    this.stateMessage = message;
    this.emit();
  }

  async ensurePermissions(): Promise<boolean> {
    if (Platform.OS !== 'android') {
      return true;
    }
    const api = Platform.Version;
    if (typeof api === 'number' && api >= 31) {
      const result = await PermissionsAndroid.requestMultiple([
        PermissionsAndroid.PERMISSIONS.BLUETOOTH_SCAN,
        PermissionsAndroid.PERMISSIONS.BLUETOOTH_CONNECT,
      ]);
      const ok =
        result[PermissionsAndroid.PERMISSIONS.BLUETOOTH_SCAN] === PermissionsAndroid.RESULTS.GRANTED &&
        result[PermissionsAndroid.PERMISSIONS.BLUETOOTH_CONNECT] === PermissionsAndroid.RESULTS.GRANTED;
      if (!ok) {
        this.setState('NoPermission', 'Нужны разрешения Bluetooth');
      }
      return ok;
    }
    if (typeof api === 'number' && api <= 30) {
      const fine = await PermissionsAndroid.request(PermissionsAndroid.PERMISSIONS.ACCESS_FINE_LOCATION);
      if (fine !== PermissionsAndroid.RESULTS.GRANTED) {
        this.setState('NoPermission', 'Для BLE scan на Android <12 нужно Location');
        return false;
      }
    }
    return true;
  }

  async isLinkUp(): Promise<boolean> {
    if (!this.device || this.state !== 'Connected') {
      return false;
    }
    try {
      return await this.device.isConnected();
    } catch {
      return false;
    }
  }

  async connect(): Promise<void> {
    if (this.connectInFlight) {
      return this.connectInFlight;
    }
    this.connectInFlight = this.connectInner().finally(() => {
      this.connectInFlight = null;
    });
    return this.connectInFlight;
  }

  private async handshakeAfterLink(): Promise<void> {
    await this.sendBuilt(commands.setTime());
    await this.refreshStatus();
  }

  private async connectInner(): Promise<void> {
    if (await this.isLinkUp()) {
      try {
        await this.handshakeAfterLink();
        return;
      } catch {
        await this.disconnect(false);
      }
    }

    const permitted = await this.ensurePermissions();
    if (!permitted) {
      return;
    }

    this.setState('Scanning');
    let found: Device | null = null;

    try {
      found = await new Promise<Device | null>((resolve, reject) => {
        const timer = setTimeout(() => {
          this.manager.stopDeviceScan();
          resolve(null);
        }, SCAN_TIMEOUT_MS);

        this.manager.startDeviceScan(null, { allowDuplicates: false }, (error, device) => {
          if (error) {
            clearTimeout(timer);
            this.manager.stopDeviceScan();
            reject(error);
            return;
          }
          const name = device?.name || device?.localName || '';
          if (device && name.startsWith(DEVICE_NAME_PREFIX)) {
            clearTimeout(timer);
            this.manager.stopDeviceScan();
            resolve(device);
          }
        });
      });
    } catch (e) {
      const msg = e instanceof BleError ? e.message : String(e);
      this.setState('Error', msg);
      return;
    }

    if (!found) {
      this.setState('DeviceNotFound');
      return;
    }

    this.deviceName = found.name || found.localName || DEVICE_NAME_PREFIX;
    this.setState('Connecting');

    try {
      const connected = await found.connect();
      const device = await connected.discoverAllServicesAndCharacteristics();
      this.device = device;

      try {
        await device.requestMTU(512);
      } catch {
        // Redmi/части стеков не умеют MTU — не фатально, куски ответа меньше.
      }

      this.responseSub = device.monitorCharacteristicForService(
        SERVICE_UUID,
        RESPONSE_UUID,
        (error, characteristic) => {
          if (error) {
            return;
          }
          this.handleResponseBytes(decodeBase64(characteristic?.value));
        },
      );

      this.statusSub = device.monitorCharacteristicForService(
        SERVICE_UUID,
        STATUS_NOTIFY_UUID,
        (error, characteristic) => {
          if (error) {
            return;
          }
          this.handleStatusBytes(decodeBase64(characteristic?.value));
        },
      );

      device.onDisconnected(() => {
        this.cleanupConnection();
        this.setState('Disconnected');
      });

      this.setState('Connected');
      await delay(400);
      await this.handshakeAfterLink();
    } catch (e) {
      const msg = e instanceof BleError ? e.message : String(e);
      this.cleanupConnection();
      this.setState('Error', msg);
    }
  }

  async disconnect(updateState = true): Promise<void> {
    this.rejectAllPending(new Error('disconnected'));
    this.cleanupConnection();
    if (updateState) {
      this.setState('Disconnected');
    }
  }

  private cleanupConnection() {
    this.responseSub?.remove();
    this.statusSub?.remove();
    this.responseSub = null;
    this.statusSub = null;
    this.chunkBuffer = null;
    const device = this.device;
    this.device = null;
    if (device) {
      device.cancelConnection().catch(() => undefined);
    }
  }

  private rejectAllPending(error: Error) {
    for (const [, pending] of this.pending) {
      clearTimeout(pending.timer);
      pending.reject(error);
    }
    this.pending.clear();
  }

  private handleStatusBytes(text: string) {
    try {
      const parsed = JSON.parse(text) as DeviceStatus;
      this.lastStatus = parsed;
      this.lastStatusAt = Date.now();
      this.emit();
    } catch {
      // ignore malformed status notify
    }
  }

  private handleResponseBytes(text: string) {
    let parsed: unknown;
    try {
      parsed = JSON.parse(text);
    } catch {
      this.failProtocol('Ошибка протокола');
      return;
    }

    if (
      parsed &&
      typeof parsed === 'object' &&
      'chunkIndex' in parsed &&
      'chunkCount' in parsed &&
      'data' in parsed
    ) {
      const chunk = parsed as { chunkIndex: number; chunkCount: number; data: string };
      if (!this.chunkBuffer || this.chunkBuffer.count !== chunk.chunkCount) {
        this.chunkBuffer = {
          count: chunk.chunkCount,
          parts: Array.from({ length: chunk.chunkCount }, () => null),
        };
      }
      this.chunkBuffer.parts[chunk.chunkIndex] = chunk.data;
      if (this.chunkBuffer.parts.every((p) => p !== null)) {
        const full = this.chunkBuffer.parts.join('');
        this.chunkBuffer = null;
        this.handleResponseBytes(full);
      }
      return;
    }

    const response = parsed as CommandResponse;
    if (!response.requestId) {
      this.failProtocol('Ошибка протокола');
      return;
    }
    const pending = this.pending.get(response.requestId);
    if (!pending) {
      return;
    }
    clearTimeout(pending.timer);
    this.pending.delete(response.requestId);
    pending.resolve(response);
  }

  private failProtocol(message: string) {
    this.stateMessage = message;
    this.emit();
  }

  private enqueue<T>(fn: () => Promise<T>): Promise<T> {
    const run = this.writeTail.then(fn, fn);
    this.writeTail = run.then(
      () => undefined,
      () => undefined,
    );
    return run;
  }

  private async sendCommandJsonOnce(json: string): Promise<CommandResponse> {
    if (!this.device || this.state !== 'Connected') {
      throw new Error('Нет подключения к устройству');
    }
    let parsedRequest: { requestId?: string };
    try {
      parsedRequest = JSON.parse(json);
    } catch {
      throw new Error('invalid-command-json');
    }
    const requestId = parsedRequest.requestId;
    if (!requestId) {
      throw new Error('requestId-missing');
    }

    const responsePromise = new Promise<CommandResponse>((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pending.delete(requestId);
        reject(new Error('ble-timeout'));
      }, this.commandTimeoutMs);
      this.pending.set(requestId, { resolve, reject, timer });
    });

    await this.device.writeCharacteristicWithResponseForService(SERVICE_UUID, COMMAND_UUID, encodeBase64(json));
    return responsePromise;
  }

  async sendCommandJson(json: string): Promise<CommandResponse> {
    return this.enqueue(() => this.sendCommandJsonOnce(json));
  }

  async sendBuilt(json: string): Promise<CommandResponse> {
    let lastError: Error = new Error('Нет ответа от контроллера');
    const attempts = this.commandRetries + 1;
    for (let attempt = 0; attempt < attempts; attempt += 1) {
      try {
        const payload = attempt === 0 ? json : withNewRequestId(json);
        const response = await this.sendCommandJson(payload);
        if (!response.ok) {
          throw new Error(response.error || 'ok=false');
        }
        return response;
      } catch (e) {
        lastError = e instanceof Error ? e : new Error(String(e));
        if (attempt < attempts - 1) {
          await delay(250 * (attempt + 1));
        }
      }
    }
    throw lastError.message === 'ble-timeout' ? new Error('Нет ответа от контроллера') : lastError;
  }

  async refreshStatus(): Promise<DeviceStatus | null> {
    const response = await this.sendBuilt(commands.getStatus());
    this.lastStatus = (response.payload as DeviceStatus) || null;
    this.lastStatusAt = Date.now();
    this.emit();
    return this.lastStatus;
  }

  destroy() {
    this.disconnect(false).catch(() => undefined);
    this.manager.destroy();
  }
}

export const bleClient = new AlarmBleClient();
