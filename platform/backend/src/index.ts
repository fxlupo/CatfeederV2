import cors from '@fastify/cors';
import Fastify from 'fastify';
import mqtt, { MqttClient } from 'mqtt';
import pg from 'pg';
import { randomUUID } from 'node:crypto';
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { PushService, type WebPushSubscription } from './push.js';

const { Pool } = pg;

const platformVersion = '0.7.0';
const pushService = new PushService();
const port = Number(process.env.PORT ?? 3000);
const mqttUrl = process.env.MQTT_URL ?? 'mqtt://localhost:1883';
const mqttUsername = process.env.MQTT_USERNAME ?? 'backend';
const mqttPassword = process.env.MQTT_PASSWORD ?? '';
const defaultDeviceId = process.env.MQTT_DEVICE_ID ?? 'catfeeder';
const cameraDeviceId = process.env.CAMERA_DEVICE_ID ?? 'catfeeder-cam';
const cameraLinkedDeviceId = process.env.CAMERA_LINKED_DEVICE_ID ?? defaultDeviceId;
const publicBaseUrl = (process.env.PUBLIC_BASE_URL ?? '').replace(/\/$/, '');
const captureUploadToken = process.env.CAMERA_UPLOAD_TOKEN ?? '';
const capturesDir = process.env.CAPTURES_DIR ?? '/data/captures';
const dbHost = process.env.POSTGRES_HOST ?? 'db';
const dbPort = Number(process.env.POSTGRES_PORT ?? 5432);
const dbName = process.env.POSTGRES_DB ?? 'catfeeder';
const dbUser = process.env.POSTGRES_USER ?? 'catfeeder';
const dbPassword = process.env.POSTGRES_PASSWORD ?? 'catfeeder';
const offlineAfterMs = Number(process.env.DEVICE_OFFLINE_AFTER_MS ?? 30000);
const commandTimeoutMs = Number(process.env.COMMAND_TIMEOUT_MS ?? 120000);
const alertOfflineAfterMs = Number(process.env.ALERT_OFFLINE_AFTER_MS ?? 180000);
const alertCooldownMs = Number(process.env.ALERT_COOLDOWN_MS ?? 3600000);

type JsonMap = Record<string, unknown>;

type CommandState = {
  id: string;
  deviceId: string;
  type: 'feed' | 'config';
  state: string;
  ok?: boolean;
  message?: string;
  createdAt: string;
  updatedAt: string;
  payload: JsonMap;
};

type CaptureState = {
  id: string;
  cameraId: string;
  deviceId: string;
  createdAt: string;
  reason: string;
  correlationId?: string;
  feedEventId?: string;
  imageUrl: string;
  mimeType: string;
  bytes: number;
};

type ConfigSync = {
  state: 'unknown' | 'synced' | 'pending' | 'drift';
  desiredUpdatedAt?: string;
  reportedUpdatedAt?: string;
  desiredHash?: string;
  reportedHash?: string;
  commandId?: string;
  message?: string;
};

type DeviceState = {
  id: string;
  seenAt?: string;
  status?: JsonMap;
  telemetry?: JsonMap;
  config?: JsonMap;
  configDesired?: JsonMap;
  configSync?: ConfigSync;
  feedLog: JsonMap[];
  captures: CaptureState[];
  alerts: JsonMap[];
  commands: CommandState[];
};

const devices = new Map<string, DeviceState>();
const sseClients = new Set<NodeJS.WritableStream>();
const offlineAlerted = new Set<string>();
const activeAlertStates = new Set<string>();
const lastAlertAt = new Map<string, number>();
const terminalCommandStates = new Set(['done', 'timeout', 'aborted', 'rejected']);
const pool = new Pool({
  host: dbHost,
  port: dbPort,
  database: dbName,
  user: dbUser,
  password: dbPassword,
});
let mqttClient: MqttClient;

function nowIso() {
  return new Date().toISOString();
}

function getDevice(deviceId: string) {
  let device = devices.get(deviceId);
  if (!device) {
    device = { id: deviceId, feedLog: [], captures: [], alerts: [], commands: [] };
    devices.set(deviceId, device);
  }
  return device;
}

function markDeviceSeen(deviceId: string) {
  const device = getDevice(deviceId);
  const wasOffline = device.seenAt
    ? Date.now() - new Date(device.seenAt).getTime() > offlineAfterMs
    : false;
  device.seenAt = nowIso();
  if (wasOffline || offlineAlerted.has(deviceId)) {
    offlineAlerted.delete(deviceId);
    resetAlertState(deviceId, 'device_offline');
    auditLog(deviceId, 'device', 'connectivity', 'device.online', {
      seenAt: device.seenAt,
    }).catch(() => undefined);
  }
  return device;
}

function isOnline(device: DeviceState) {
  if (!device.seenAt) return false;
  return Date.now() - new Date(device.seenAt).getTime() <= offlineAfterMs;
}

function serializeDevice(device: DeviceState) {
  updateConfigSync(device);
  const ageMs = device.seenAt ? Date.now() - new Date(device.seenAt).getTime() : undefined;
  return {
    ...device,
    online: isOnline(device),
    ageSeconds: ageMs === undefined ? null : Math.max(0, Math.round(ageMs / 1000)),
  };
}

function sendEvent(event: string, data: unknown) {
  const payload = `event: ${event}\ndata: ${JSON.stringify(data)}\n\n`;
  for (const client of sseClients) client.write(payload);
}

function trim<T>(items: T[], limit: number) {
  if (items.length > limit) items.splice(0, items.length - limit);
}

function normalizeForCompare(value: unknown): unknown {
  if (Array.isArray(value)) return value.map(normalizeForCompare);
  if (!value || typeof value !== 'object') return value;
  const source = value as JsonMap;
  return Object.keys(source)
    .filter((key) => key !== 'fw')
    .sort()
    .reduce<JsonMap>((result, key) => {
      result[key] = normalizeForCompare(source[key]);
      return result;
    }, {});
}

function configHash(config?: JsonMap) {
  if (!config) return undefined;
  return JSON.stringify(normalizeForCompare(config));
}

function updateConfigSync(device: DeviceState) {
  const desiredHash = configHash(device.configDesired);
  const reportedHash = configHash(device.config);
  const prior = device.configSync;

  if (!desiredHash && !reportedHash) {
    device.configSync = { state: 'unknown', message: 'no-config' };
    return;
  }

  if (!desiredHash) {
    device.configSync = {
      state: 'synced',
      reportedHash,
      reportedUpdatedAt: prior?.reportedUpdatedAt,
      message: 'reported-only',
    };
    return;
  }

  if (!reportedHash) {
    device.configSync = {
      ...prior,
      state: 'pending',
      desiredHash,
      reportedHash,
      message: 'waiting-for-reported-config',
    };
    return;
  }

  const state = desiredHash === reportedHash ? 'synced' : prior?.state === 'pending' ? 'pending' : 'drift';
  device.configSync = {
    ...prior,
    state,
    desiredHash,
    reportedHash,
    message: state === 'synced' ? 'desired-matches-reported' : 'desired-differs-from-reported',
  };
}

// ── Audit-Log ────────────────────────────────────────────────────────────────

async function auditLog(
  deviceId: string,
  actor: 'device' | 'remote-ui' | 'system',
  category: 'feed' | 'config' | 'connectivity' | 'command' | 'alert' | 'push' | 'capture',
  action: string,
  payload: JsonMap = {},
) {
  await pool.query(
    'insert into audit_log (device_id, actor, category, action, payload) values ($1, $2, $3, $4, $5)',
    [deviceId, actor, category, action, payload],
  ).catch((err: Error) => console.error('[audit]', err.message));
}

async function cleanupExpiredPushSubscriptions(endpoints: string[]) {
  if (endpoints.length === 0) return;
  await pool.query('delete from push_subscriptions where endpoint = any($1)', [endpoints]);
  await auditLog('platform', 'system', 'push', 'push.subscriptions.expired', { count: endpoints.length });
}

// ── DB ───────────────────────────────────────────────────────────────────────

async function initDb() {
  await pool.query(`
    create table if not exists devices (
      id text primary key,
      last_seen timestamptz,
      status jsonb,
      telemetry jsonb,
      config_reported jsonb,
      config_desired jsonb,
      config_desired_at timestamptz,
      config_reported_at timestamptz
    );

    create table if not exists telemetry (
      id bigserial primary key,
      device_id text not null,
      created_at timestamptz not null default now(),
      payload jsonb not null
    );

    create table if not exists feed_events (
      id bigserial primary key,
      device_id text not null,
      created_at timestamptz not null default now(),
      payload jsonb not null
    );

    create table if not exists command_log (
      id text primary key,
      device_id text not null,
      type text not null,
      state text not null,
      ok boolean,
      message text,
      created_at timestamptz not null default now(),
      updated_at timestamptz not null default now(),
      payload jsonb not null
    );

    create table if not exists alerts (
      id bigserial primary key,
      device_id text not null,
      created_at timestamptz not null default now(),
      level text not null,
      type text not null,
      message text not null,
      payload jsonb not null
    );

    create table if not exists audit_log (
      id bigserial primary key,
      device_id text not null,
      created_at timestamptz not null default now(),
      actor text not null,
      category text not null,
      action text not null,
      payload jsonb not null default '{}'
    );

    create table if not exists push_subscriptions (
      endpoint text primary key,
      created_at timestamptz not null default now(),
      p256dh text not null,
      auth text not null
    );

    create table if not exists captures (
      id text primary key,
      camera_id text not null,
      device_id text not null,
      created_at timestamptz not null default now(),
      reason text not null,
      correlation_id text,
      feed_event_id text,
      file_path text not null,
      mime_type text not null,
      bytes integer not null
    );
  `);

  await pool.query('alter table devices add column if not exists config_desired jsonb');
  await pool.query('alter table devices add column if not exists config_desired_at timestamptz');
  await pool.query('alter table devices add column if not exists config_reported_at timestamptz');
  await pool.query('create index if not exists audit_log_device_id_idx on audit_log(device_id, id desc)');
  await pool.query('create index if not exists captures_device_id_idx on captures(device_id, created_at desc)');
  await pool.query('create index if not exists captures_camera_id_idx on captures(camera_id, created_at desc)');
}

async function upsertDevice(deviceId: string, field: 'status' | 'telemetry' | 'config_reported', payload: JsonMap) {
  await pool.query(
    `insert into devices (id, last_seen, ${field})
     values ($1, now(), $2)
     on conflict (id) do update set last_seen = now(), ${field} = excluded.${field}`,
    [deviceId, payload],
  );
  if (field === 'config_reported') {
    await pool.query('update devices set config_reported_at = now() where id = $1', [deviceId]);
  }
}

async function storeDesiredConfig(deviceId: string, config: JsonMap) {
  await pool.query(
    `insert into devices (id, config_desired, config_desired_at)
     values ($1, $2, now())
     on conflict (id) do update set config_desired = excluded.config_desired, config_desired_at = now()`,
    [deviceId, config],
  );
}

async function logCommand(command: CommandState) {
  await pool.query(
    `insert into command_log (id, device_id, type, state, ok, message, payload)
     values ($1, $2, $3, $4, $5, $6, $7)
     on conflict (id) do update set
       state = excluded.state,
       ok = excluded.ok,
       message = excluded.message,
       updated_at = now(),
       payload = excluded.payload`,
    [command.id, command.deviceId, command.type, command.state, command.ok ?? null, command.message ?? '', command.payload],
  );
}

async function createAlert(deviceId: string, level: string, type: string, message: string, payload: JsonMap) {
  const alert = { id: randomUUID(), deviceId, level, type, message, createdAt: nowIso(), payload };
  const device = getDevice(deviceId);
  device.alerts.push(alert);
  trim(device.alerts, 50);
  await pool.query(
    'insert into alerts (device_id, level, type, message, payload) values ($1, $2, $3, $4, $5)',
    [deviceId, level, type, message, payload],
  );
  await auditLog(deviceId, 'system', 'alert', `alert.${type}`, { level, message });
  if (level === 'critical' || level === 'warning') {
    const expired = await pushService.broadcast({
      title: `CatFeeder – ${type.replace(/_/g, ' ')}`,
      body: `${message} [${deviceId}]`,
      tags: [level, type],
      priority: level === 'critical' ? 'high' : 'default',
    }).catch((err: Error) => console.error('[push]', err.message));
    await cleanupExpiredPushSubscriptions(expired ?? []);
  }
  sendEvent('alert', alert);
}

function alertKey(deviceId: string, type: string) {
  return `${deviceId}:${type}`;
}

function resetAlertState(deviceId: string, type: string) {
  activeAlertStates.delete(alertKey(deviceId, type));
}

function canCreateStateAlert(deviceId: string, type: string) {
  const key = alertKey(deviceId, type);
  if (activeAlertStates.has(key)) return false;

  const last = lastAlertAt.get(key) ?? 0;
  if (Date.now() - last < alertCooldownMs) return false;

  activeAlertStates.add(key);
  lastAlertAt.set(key, Date.now());
  return true;
}

async function createStateAlert(deviceId: string, level: string, type: string, message: string, payload: JsonMap) {
  if (!canCreateStateAlert(deviceId, type)) return false;
  await createAlert(deviceId, level, type, message, payload);
  return true;
}

async function checkOfflineDevices() {
  for (const device of devices.values()) {
    if (!device.seenAt) continue;
    const ageSeconds = Math.round((Date.now() - new Date(device.seenAt).getTime()) / 1000);
    if (ageSeconds * 1000 < alertOfflineAfterMs) continue;
    if (offlineAlerted.has(device.id)) continue;
    offlineAlerted.add(device.id);
    await createAlert(device.id, 'warning', 'device_offline', 'Device offline', {
      seenAt: device.seenAt,
      ageSeconds,
      thresholdSeconds: Math.round(alertOfflineAfterMs / 1000),
    });
    await auditLog(device.id, 'system', 'connectivity', 'device.offline', {
      seenAt: device.seenAt,
      ageSeconds,
    });
    sendEvent('device', serializeDevice(device));
  }
}

async function checkCommandTimeouts() {
  const activeStates = new Set(['queued', 'accepted']);
  for (const device of devices.values()) {
    for (const command of device.commands) {
      if (!activeStates.has(command.state)) continue;
      if (Date.now() - new Date(command.updatedAt).getTime() < commandTimeoutMs) continue;
      command.state = 'timeout';
      command.ok = false;
      command.message = 'command-timeout';
      command.updatedAt = nowIso();
      if (command.type === 'config' && device.configSync?.commandId === command.id) {
        device.configSync = {
          ...device.configSync,
          state: 'drift',
          message: 'config-command-timeout',
        };
      }
      await logCommand(command);
      await auditLog(device.id, 'system', 'command', `${command.type}.timeout`, {
        commandId: command.id,
      });
      sendEvent('command', command);
    }
  }
}

// Löscht terminal commands nur aus dem In-Memory-State.
// Die DB-Records bleiben als permanente Audit-History erhalten.
async function clearTerminalCommands(deviceId: string) {
  const device = getDevice(deviceId);
  const cleared = device.commands.filter((c) => terminalCommandStates.has(c.state)).length;
  device.commands = device.commands.filter((c) => !terminalCommandStates.has(c.state));
  if (cleared > 0) {
    await auditLog(deviceId, 'remote-ui', 'command', 'commands.cleared', { count: cleared });
  }
  sendEvent('command', { deviceId, cleared: true });
}

async function clearAlerts(deviceId: string) {
  const device = getDevice(deviceId);
  device.alerts = [];
  offlineAlerted.delete(deviceId);
  await pool.query('delete from alerts where device_id = $1', [deviceId]);
  await auditLog(deviceId, 'remote-ui', 'alert', 'alerts.cleared');
  sendEvent('alert', { deviceId, cleared: true });
}

function rememberCommand(command: CommandState) {
  const device = getDevice(command.deviceId);
  const existing = device.commands.findIndex((item) => item.id === command.id);
  if (existing >= 0) device.commands[existing] = command;
  else device.commands.push(command);
  trim(device.commands, 50);
}

function publishCommand(deviceId: string, suffix: string, payload: JsonMap) {
  const topic = `catfeeder/${deviceId}/${suffix}`;
  mqttClient.publish(topic, JSON.stringify(payload), { qos: 1 });
}

function captureImageUrl(captureId: string) {
  return `/api/captures/${encodeURIComponent(captureId)}/image`;
}

function captureUploadUrl(cameraId: string) {
  const pathPart = `/api/devices/${encodeURIComponent(cameraId)}/captures`;
  return publicBaseUrl ? `${publicBaseUrl}${pathPart}` : pathPart;
}

function safePathSegment(value: string) {
  return value.replace(/[^a-zA-Z0-9_.-]/g, '_').slice(0, 80) || 'unknown';
}

async function rememberCapture(capture: CaptureState) {
  const device = getDevice(capture.deviceId);
  const existing = device.captures.findIndex((item) => item.id === capture.id);
  if (existing >= 0) device.captures[existing] = capture;
  else device.captures.unshift(capture);
  if (device.captures.length > 100) device.captures.length = 100;
}

async function publishCaptureCommand(
  deviceId: string,
  reason: string,
  correlationId?: string,
  feedEventId?: string,
) {
  if (!cameraDeviceId) return;
  const id = randomUUID();
  const payload: JsonMap = {
    id,
    type: 'capture',
    reason,
    deviceId,
    correlationId: correlationId ?? id,
    uploadUrl: captureUploadUrl(cameraDeviceId),
    issuedAt: nowIso(),
  };
  if (feedEventId) payload.feedEventId = feedEventId;
  publishCommand(cameraDeviceId, 'cmd/capture', payload);
  await auditLog(deviceId, 'system', 'capture', 'capture.requested', {
    captureCommandId: id,
    cameraId: cameraDeviceId,
    reason,
    correlationId: payload.correlationId,
    feedEventId,
  });
  sendEvent('capture', { deviceId, cameraId: cameraDeviceId, requested: true, payload });
}

async function rejectCommand(deviceId: string, type: 'feed' | 'config', message: string) {
  const command: CommandState = {
    id: randomUUID(),
    deviceId,
    type,
    state: 'rejected',
    ok: false,
    message,
    createdAt: nowIso(),
    updatedAt: nowIso(),
    payload: {},
  };
  rememberCommand(command);
  await logCommand(command);
  await auditLog(deviceId, 'system', 'command', `${type}.rejected`, { reason: message });
  sendEvent('command', command);
  return command;
}

async function hydrateDevice(deviceId: string) {
  const device = getDevice(deviceId);
  const stored = await pool.query(
    `select
       last_seen as "lastSeen",
       status,
       telemetry,
       config_reported as "configReported",
       config_desired as "configDesired",
       config_desired_at as "configDesiredAt",
       config_reported_at as "configReportedAt"
     from devices where id = $1`,
    [deviceId],
  );
  if (stored.rows[0]) {
    device.seenAt = stored.rows[0].lastSeen?.toISOString?.() ?? device.seenAt;
    device.status ??= stored.rows[0].status ?? undefined;
    device.telemetry ??= stored.rows[0].telemetry ?? undefined;
    device.config ??= stored.rows[0].configReported ?? undefined;
    device.configDesired ??= stored.rows[0].configDesired ?? undefined;
    device.configSync = {
      ...device.configSync,
      state: device.configSync?.state ?? 'unknown',
      desiredUpdatedAt: stored.rows[0].configDesiredAt?.toISOString?.(),
      reportedUpdatedAt: stored.rows[0].configReportedAt?.toISOString?.(),
    };
  }

  if (device.feedLog.length === 0) {
    const rows = await pool.query(
      'select created_at as "createdAt", payload from feed_events where device_id = $1 order by id desc limit 100',
      [deviceId],
    );
    device.feedLog = rows.rows.map((row) => ({ ...row.payload, receivedAt: row.createdAt }));
  }

  if (device.captures.length === 0) {
    const rows = await pool.query(
      `select id, camera_id as "cameraId", device_id as "deviceId",
              created_at as "createdAt", reason, correlation_id as "correlationId",
              feed_event_id as "feedEventId", mime_type as "mimeType", bytes
       from captures
       where device_id = $1 or camera_id = $1
       order by created_at desc limit 100`,
      [deviceId],
    );
    device.captures = rows.rows.map((row) => ({
      ...row,
      imageUrl: captureImageUrl(row.id),
    }));
  }

  if (device.alerts.length === 0) {
    const rows = await pool.query(
      'select created_at as "createdAt", level, type, message, payload from alerts where device_id = $1 order by id desc limit 50',
      [deviceId],
    );
    device.alerts = rows.rows.map((row) => ({ ...row.payload, level: row.level, type: row.type, message: row.message, createdAt: row.createdAt }));
  }

  if (device.commands.length === 0) {
    // Lädt nur non-terminal aktive Commands + die letzten 20 terminal (für kurzfristige Anzeige)
    const rows = await pool.query(
      `select id, device_id as "deviceId", type, state, ok, message,
              created_at as "createdAt", updated_at as "updatedAt", payload
       from command_log where device_id = $1
       order by updated_at desc limit 50`,
      [deviceId],
    );
    device.commands = rows.rows.reverse();
  }

  updateConfigSync(device);
  return device;
}

async function handleMqttMessage(topic: string, raw: Buffer) {
  const parts = topic.split('/');
  if (parts.length < 3 || parts[0] !== 'catfeeder') return;
  const deviceId = parts[1];
  const channel = parts.slice(2).join('/');
  let payload: JsonMap;

  try {
    payload = JSON.parse(raw.toString('utf8')) as JsonMap;
  } catch {
    return;
  }

  const device = markDeviceSeen(deviceId);

  if (channel === 'status') {
    device.status = payload;
    await upsertDevice(deviceId, 'status', payload);
    sendEvent('device', serializeDevice(device));
    return;
  }

  if (channel === 'telemetry') {
    device.telemetry = payload;
    await upsertDevice(deviceId, 'telemetry', payload);
    await pool.query('insert into telemetry (device_id, payload) values ($1, $2)', [deviceId, payload]);
    sendEvent('telemetry', serializeDevice(device));
    if (payload.fillLow === true) {
      await createStateAlert(deviceId, 'warning', 'fill_low', 'Fuellstand niedrig', payload);
    } else {
      resetAlertState(deviceId, 'fill_low');
    }

    if (payload.overcurrent === true) {
      await createStateAlert(deviceId, 'critical', 'overcurrent', 'Ueberstrom erkannt', payload);
    } else {
      resetAlertState(deviceId, 'overcurrent');
    }
    return;
  }

  if (channel === 'config/reported') {
    device.config = payload;
    await upsertDevice(deviceId, 'config_reported', payload);
    const prevSync = device.configSync;
    device.configSync = {
      ...device.configSync,
      state: device.configSync?.state ?? 'unknown',
      reportedUpdatedAt: nowIso(),
    };
    updateConfigSync(device);
    const syncState = device.configSync?.state;
    await auditLog(deviceId, 'device', 'config', 'config.reported', {
      syncState,
      commandId: prevSync?.commandId,
    });
    if (syncState === 'synced' && prevSync?.state === 'pending') {
      await auditLog(deviceId, 'device', 'config', 'config.confirmed', {
        commandId: prevSync.commandId,
      });
    }
    sendEvent('config', serializeDevice(device));
    return;
  }

  if (channel === 'feed/log' || channel === 'event') {
    if (channel === 'feed/log') {
      device.feedLog.push({ ...payload, receivedAt: nowIso() });
      trim(device.feedLog, 100);
      const feedInsert = await pool.query(
        'insert into feed_events (device_id, payload) values ($1, $2) returning id',
        [deviceId, payload],
      );
      const action = payload.aborted === true ? 'feed.aborted' : 'feed.completed';
      await auditLog(deviceId, 'device', 'feed', action, {
        grams: payload.grams,
        servo: payload.servo,
        blockRetries: payload.blockRetryCount,
      });
      await publishCaptureCommand(
        deviceId,
        payload.aborted === true ? 'feed_aborted' : 'feed_done',
        String(payload.commandId ?? payload.id ?? feedInsert.rows[0]?.id ?? randomUUID()),
        String(feedInsert.rows[0]?.id ?? ''),
      );
    }
    sendEvent('feed', serializeDevice(device));
    if (payload.aborted === true || payload.type === 'feed_aborted') {
      await createAlert(deviceId, 'critical', 'feed_aborted', 'Fuetterung abgebrochen', payload);
    }
    return;
  }

  if (channel === 'cmd/ack' || channel === 'cmd/result') {
    const id = String(payload.id ?? '');
    if (!id) return;
    const prior = device.commands.find((item) => item.id === id);
    const command: CommandState = {
      id,
      deviceId,
      type: prior?.type ?? 'feed',
      state: String(payload.state ?? (channel === 'cmd/ack' ? 'accepted' : 'done')),
      ok: Boolean(payload.ok),
      message: String(payload.message ?? ''),
      createdAt: prior?.createdAt ?? nowIso(),
      updatedAt: nowIso(),
      payload: prior?.payload ?? payload,
    };
    rememberCommand(command);
    if (command.type === 'config' && command.ok === false && device.configSync?.commandId === command.id) {
      device.configSync = {
        ...device.configSync,
        state: 'drift',
        message: command.message || 'config-command-failed',
      };
      await auditLog(deviceId, 'device', 'config', 'config.failed', {
        commandId: command.id,
        message: command.message,
      });
    }
    if (channel === 'cmd/result') {
      const action = command.ok ? `${command.type}.done` : `${command.type}.failed`;
      await auditLog(deviceId, 'device', 'command', action, {
        commandId: command.id,
        ok: command.ok,
        message: command.message,
      });
    }
    await logCommand(command);
    sendEvent('command', command);
  }
}

async function startMqtt() {
  mqttClient = mqtt.connect(mqttUrl, {
    username: mqttUsername,
    password: mqttPassword,
    reconnectPeriod: 3000,
    keepalive: 30,
  });

  mqttClient.on('connect', () => {
    console.log(`[mqtt] connected ${mqttUrl}`);
    mqttClient.subscribe('catfeeder/+/status');
    mqttClient.subscribe('catfeeder/+/telemetry');
    mqttClient.subscribe('catfeeder/+/config/reported');
    mqttClient.subscribe('catfeeder/+/feed/log');
    mqttClient.subscribe('catfeeder/+/event');
    mqttClient.subscribe('catfeeder/+/cmd/ack');
    mqttClient.subscribe('catfeeder/+/cmd/result');
  });

  mqttClient.on('message', (topic, payload) => {
    handleMqttMessage(topic, payload).catch((err) => console.error('[mqtt] message failed', err));
  });

  mqttClient.on('error', (err) => console.error('[mqtt]', err.message));
}

async function main() {
  await initDb();
  // Web-Push-Subscriptions aus DB laden
  const subRows = await pool.query(
    'select endpoint, p256dh, auth from push_subscriptions',
  );
  pushService.loadSubscriptions(subRows.rows);
  await startMqtt();
  setInterval(() => {
    checkOfflineDevices().catch((err) => console.error('[offline-check]', err));
    checkCommandTimeouts().catch((err) => console.error('[command-timeout]', err));
  }, 5000);

  const app = Fastify({ logger: true });
  await app.register(cors, { origin: true });
  app.addContentTypeParser('image/jpeg', { parseAs: 'buffer' }, (_request, body, done) => {
    done(null, body);
  });
  app.addContentTypeParser('image/jpg', { parseAs: 'buffer' }, (_request, body, done) => {
    done(null, body);
  });

  app.get('/api/health', async () => ({
    ok: true,
    platformVersion,
    mqttConnected: mqttClient.connected,
    devices: devices.size,
    offlineAfterMs,
    commandTimeoutMs,
    alertOfflineAfterMs,
    alertCooldownMs,
    cameraDeviceId,
    captureUploadEnabled: true,
  }));

  app.get('/api/events', async (_request, reply) => {
    reply.hijack();
    reply.raw.writeHead(200, {
      'Content-Type': 'text/event-stream',
      'Cache-Control': 'no-cache',
      Connection: 'keep-alive',
    });
    reply.raw.write('\n');
    sseClients.add(reply.raw);
    reply.raw.on('close', () => sseClients.delete(reply.raw));
  });

  app.get('/api/devices', async () => Array.from(devices.values()).map(serializeDevice));

  app.get('/api/devices/:id', async (request) => {
    const { id } = request.params as { id: string };
    const device = await hydrateDevice(id);
    return serializeDevice(device);
  });

  app.get('/api/devices/:id/telemetry', async (request) => {
    const { id } = request.params as { id: string };
    const rows = await pool.query(
      'select created_at as "createdAt", payload from telemetry where device_id = $1 order by id desc limit 200',
      [id],
    );
    return rows.rows;
  });

  app.get('/api/devices/:id/feed-log', async (request) => {
    const { id } = request.params as { id: string };
    const rows = await pool.query(
      'select created_at as "createdAt", payload from feed_events where device_id = $1 order by id desc limit 100',
      [id],
    );
    return rows.rows;
  });

  app.get('/api/devices/:id/captures', async (request) => {
    const { id } = request.params as { id: string };
    const query = request.query as { limit?: string };
    const limit = Math.min(100, Number(query.limit ?? 50));
    const rows = await pool.query(
      `select id, camera_id as "cameraId", device_id as "deviceId",
              created_at as "createdAt", reason, correlation_id as "correlationId",
              feed_event_id as "feedEventId", mime_type as "mimeType", bytes
       from captures
       where device_id = $1 or camera_id = $1
       order by created_at desc limit $2`,
      [id, limit],
    );
    return rows.rows.map((row) => ({ ...row, imageUrl: captureImageUrl(row.id) }));
  });

  app.get('/api/captures/:id/image', async (request, reply) => {
    const { id } = request.params as { id: string };
    const row = await pool.query(
      'select file_path as "filePath", mime_type as "mimeType" from captures where id = $1',
      [id],
    );
    if (!row.rows[0]) {
      reply.code(404);
      return { error: 'capture-not-found' };
    }
    const image = await readFile(row.rows[0].filePath);
    reply.header('Cache-Control', 'private, max-age=3600');
    reply.type(row.rows[0].mimeType);
    return reply.send(image);
  });

  app.post('/api/devices/:cameraId/captures', async (request, reply) => {
    const { cameraId } = request.params as { cameraId: string };
    const query = request.query as {
      deviceId?: string;
      reason?: string;
      correlationId?: string;
      feedEventId?: string;
      token?: string;
    };
    const headerToken = request.headers['x-capture-token'];
    const providedToken = Array.isArray(headerToken) ? headerToken[0] : headerToken ?? query.token ?? '';
    if (captureUploadToken && providedToken !== captureUploadToken) {
      await auditLog(cameraLinkedDeviceId, 'device', 'capture', 'capture.rejected', {
        cameraId,
        reason: 'invalid-token',
      });
      reply.code(401);
      return { ok: false, error: 'invalid-token' };
    }
    if (!Buffer.isBuffer(request.body)) {
      reply.code(400);
      return { ok: false, error: 'jpeg-body-required' };
    }

    const body = request.body;
    const id = randomUUID();
    const deviceId = String(query.deviceId ?? cameraLinkedDeviceId);
    const reason = String(query.reason ?? 'manual');
    const day = new Date().toISOString().slice(0, 10);
    const dir = path.join(capturesDir, safePathSegment(cameraId), day);
    const filePath = path.join(dir, `${safePathSegment(id)}.jpg`);
    await mkdir(dir, { recursive: true });
    await writeFile(filePath, body);

    const row = await pool.query(
      `insert into captures
         (id, camera_id, device_id, reason, correlation_id, feed_event_id, file_path, mime_type, bytes)
       values ($1, $2, $3, $4, $5, $6, $7, $8, $9)
       returning created_at as "createdAt"`,
      [
        id,
        cameraId,
        deviceId,
        reason,
        query.correlationId ?? null,
        query.feedEventId ?? null,
        filePath,
        'image/jpeg',
        body.length,
      ],
    );

    const capture: CaptureState = {
      id,
      cameraId,
      deviceId,
      createdAt: row.rows[0].createdAt.toISOString(),
      reason,
      correlationId: query.correlationId,
      feedEventId: query.feedEventId,
      imageUrl: captureImageUrl(id),
      mimeType: 'image/jpeg',
      bytes: body.length,
    };
    await rememberCapture(capture);
    await auditLog(deviceId, 'device', 'capture', 'capture.uploaded', {
      captureId: id,
      cameraId,
      reason,
      bytes: body.length,
      correlationId: query.correlationId,
      feedEventId: query.feedEventId,
    });
    sendEvent('capture', { deviceId, capture });
    return { ok: true, capture };
  });

  // Paginierter Command-Verlauf aus der Datenbank (inkl. älterer terminal states)
  app.get('/api/devices/:id/commands', async (request) => {
    const { id } = request.params as { id: string };
    const query = request.query as { limit?: string; offset?: string; state?: string };
    const limit = Math.min(100, Number(query.limit ?? 50));
    const offset = Number(query.offset ?? 0);
    let sql = `select id, device_id as "deviceId", type, state, ok, message,
                      created_at as "createdAt", updated_at as "updatedAt", payload
               from command_log where device_id = $1`;
    const params: (string | number)[] = [id];
    if (query.state === 'active') {
      sql += " and state in ('queued','accepted')";
    } else if (query.state === 'terminal') {
      sql += " and state in ('done','timeout','aborted','rejected')";
    }
    sql += ` order by updated_at desc limit $${params.length + 1} offset $${params.length + 2}`;
    params.push(limit, offset);
    const rows = await pool.query(sql, params);
    return rows.rows;
  });

  // Paginierter Audit-Log
  app.get('/api/devices/:id/audit', async (request) => {
    const { id } = request.params as { id: string };
    const query = request.query as { limit?: string; offset?: string; category?: string };
    const limit = Math.min(200, Number(query.limit ?? 100));
    const offset = Number(query.offset ?? 0);
    let sql = `select id, created_at as "createdAt", actor, category, action, payload
               from audit_log where device_id = $1`;
    const params: (string | number)[] = [id];
    if (query.category) {
      params.push(query.category);
      sql += ` and category = $${params.length}`;
    }
    sql += ` order by id desc limit $${params.length + 1} offset $${params.length + 2}`;
    params.push(limit, offset);
    const rows = await pool.query(sql, params);
    return rows.rows;
  });

  app.post('/api/devices/:id/feed', async (request) => {
    const { id } = request.params as { id: string };
    const device = await hydrateDevice(id);
    if (!isOnline(device)) {
      return rejectCommand(id, 'feed', 'device-offline');
    }
    const body = (request.body ?? {}) as { grams?: number; servo?: number };
    const grams = Math.max(1, Math.min(500, Number(body.grams ?? 20)));
    const servo = Math.max(0, Math.min(2, Number(body.servo ?? 0)));
    const command: CommandState = {
      id: randomUUID(),
      deviceId: id,
      type: 'feed',
      state: 'queued',
      createdAt: nowIso(),
      updatedAt: nowIso(),
      payload: {
        id: randomUUID(),
        type: 'feed',
        grams,
        servo,
        source: 'remote-ui',
        issuedAt: nowIso(),
      },
    };
    command.payload.id = command.id;
    rememberCommand(command);
    await logCommand(command);
    await auditLog(id, 'remote-ui', 'feed', 'feed.issued', {
      commandId: command.id,
      grams,
      servo,
    });
    publishCommand(id, 'cmd/feed', command.payload);
    await publishCaptureCommand(id, 'feed_command', command.id);
    sendEvent('command', command);
    return command;
  });

  app.get('/api/devices/:id/config', async (request) => {
    const { id } = request.params as { id: string };
    publishCommand(id, 'cmd/config/get', { id: randomUUID(), type: 'config_get', issuedAt: nowIso() });
    return getDevice(id).config ?? {};
  });

  app.put('/api/devices/:id/config', async (request) => {
    const { id } = request.params as { id: string };
    const device = await hydrateDevice(id);
    if (!isOnline(device)) {
      return rejectCommand(id, 'config', 'device-offline');
    }
    const config = (request.body ?? {}) as JsonMap;
    await storeDesiredConfig(id, config);
    const command: CommandState = {
      id: randomUUID(),
      deviceId: id,
      type: 'config',
      state: 'queued',
      createdAt: nowIso(),
      updatedAt: nowIso(),
      payload: { id: '', type: 'config_set', config, source: 'remote-ui', issuedAt: nowIso() },
    };
    command.payload.id = command.id;
    device.configDesired = config;
    device.configSync = {
      ...device.configSync,
      state: 'pending',
      desiredUpdatedAt: nowIso(),
      commandId: command.id,
      message: 'waiting-for-device-report',
    };
    updateConfigSync(device);
    rememberCommand(command);
    await logCommand(command);
    await auditLog(id, 'remote-ui', 'config', 'config.sent', { commandId: command.id });
    publishCommand(id, 'cmd/config/set', command.payload);
    sendEvent('command', command);
    return command;
  });

  app.delete('/api/devices/:id/commands/terminal', async (request) => {
    const { id } = request.params as { id: string };
    await clearTerminalCommands(id);
    return serializeDevice(await hydrateDevice(id));
  });

  app.delete('/api/devices/:id/alerts', async (request) => {
    const { id } = request.params as { id: string };
    await clearAlerts(id);
    return serializeDevice(await hydrateDevice(id));
  });

  // ── Push-Endpunkte ───────────────────────────────────────────────────────

  app.get('/api/push/config', async () => ({
    vapidPublicKey: pushService.vapidPublicKey || null,
    webPushEnabled: pushService.hasVapid(),
    ntfyEnabled: pushService.hasNtfy(),
    ntfyUrl: process.env.NTFY_URL ?? null,
    subscriptions: pushService.subscriptionCount(),
  }));

  app.post('/api/push/subscribe', async (request) => {
    const sub = request.body as WebPushSubscription;
    if (!sub?.endpoint || !sub?.keys?.p256dh || !sub?.keys?.auth) {
      throw new Error('Ungültige Subscription');
    }
    pushService.addSubscription(sub);
    await pool.query(
      `insert into push_subscriptions (endpoint, p256dh, auth) values ($1, $2, $3)
       on conflict (endpoint) do update set p256dh = excluded.p256dh, auth = excluded.auth`,
      [sub.endpoint, sub.keys.p256dh, sub.keys.auth],
    );
    await auditLog('platform', 'remote-ui', 'push', 'push.subscribed', {
      endpointSuffix: sub.endpoint.slice(-24),
    });
    return { ok: true };
  });

  app.delete('/api/push/subscribe', async (request) => {
    const body = request.body as { endpoint?: string };
    if (!body?.endpoint) throw new Error('endpoint fehlt');
    pushService.removeSubscription(body.endpoint);
    await pool.query('delete from push_subscriptions where endpoint = $1', [body.endpoint]);
    await auditLog('platform', 'remote-ui', 'push', 'push.unsubscribed', {
      endpointSuffix: body.endpoint.slice(-24),
    });
    return { ok: true };
  });

  app.post('/api/push/test', async () => {
    const expired = await pushService.broadcast({
      title: 'CatFeeder Test',
      body: 'Testbenachrichtigung – Push funktioniert.',
      tags: ['test'],
      priority: 'default',
    });
    await cleanupExpiredPushSubscriptions(expired);
    await auditLog('platform', 'remote-ui', 'push', 'push.test', {
      expiredSubscriptions: expired.length,
    });
    return { ok: true };
  });

  await app.listen({ host: '0.0.0.0', port });
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
