import React, { useEffect, useMemo, useRef, useState } from 'react';
import { createRoot } from 'react-dom/client';
import {
  Activity,
  AlertTriangle,
  Bell,
  CalendarClock,
  Gauge,
  History,
  Play,
  Save,
  Settings,
  Wifi,
} from 'lucide-react';
import './styles.css';

type Slot = { on: boolean; h: number; m: number; g: number; sv: number };
type Config = {
  fw?: string;
  deviceId?: string;
  hostname?: string;
  defaultGrams?: number;
  stepsPerGram?: number;
  stepperSpeed?: number;
  stepperPulseUS?: number;
  stepperDirSetupUS?: number;
  stepperHoldMS?: number;
  stepperBlockMA?: number;
  blockRetries?: number;
  blockReverseSteps?: number;
  blockMinRotPct?: number;
  servoSpeedDPS?: number;
  s1Open?: number;
  s1Close?: number;
  s2Open?: number;
  s2Close?: number;
  slots?: Slot[];
};
type Device = {
  id: string;
  online?: boolean;
  ageSeconds?: number | null;
  seenAt?: string;
  status?: Record<string, any>;
  telemetry?: Record<string, any>;
  config?: Config;
  configDesired?: Config;
  configSync?: ConfigSync;
  feedLog?: Record<string, any>[];
  alerts?: Record<string, any>[];
  commands?: Record<string, any>[];
};
type ConfigSync = {
  state?: 'unknown' | 'synced' | 'pending' | 'drift';
  desiredUpdatedAt?: string;
  reportedUpdatedAt?: string;
  commandId?: string;
  message?: string;
};
type AuditEntry = {
  id: number;
  createdAt: string;
  actor: 'device' | 'remote-ui' | 'system';
  category: string;
  action: string;
  payload: Record<string, unknown>;
};
type PushConfig = {
  vapidPublicKey?: string | null;
  webPushEnabled?: boolean;
  ntfyEnabled?: boolean;
  ntfyUrl?: string | null;
  subscriptions?: number;
};
type Health = {
  platformVersion?: string;
};

const api = {
  async get<T>(url: string): Promise<T> {
    const res = await fetch(url);
    if (!res.ok) throw new Error(await res.text());
    return res.json();
  },
  async send<T>(url: string, method: string, body?: unknown): Promise<T> {
    const res = await fetch(url, {
      method,
      headers: { 'Content-Type': 'application/json' },
      body: body ? JSON.stringify(body) : undefined,
    });
    if (!res.ok) throw new Error(await res.text());
    return res.json();
  },
};

function App() {
  const [deviceId, setDeviceId] = useState('catfeeder');
  const [device, setDevice] = useState<Device>({ id: 'catfeeder' });
  const [configDraft, setConfigDraft] = useState<Config>({});
  const [tab, setTab] = useState('dashboard');
  const [feedGrams, setFeedGrams] = useState(5);
  const [feedServo, setFeedServo] = useState(0);
  const [notice, setNotice] = useState('');
  const [platformVersion, setPlatformVersion] = useState('0.6.0');
  const [configDirty, setConfigDirty] = useState(false);
  const configDirtyRef = useRef(false);
  const [audit, setAudit] = useState<AuditEntry[]>([]);
  const [pushConfig, setPushConfig] = useState<PushConfig>({});
  const [pushState, setPushState] = useState<'unknown' | 'subscribed' | 'denied' | 'unsupported'>('unknown');

  async function loadDevice(id = deviceId) {
    const data = await api.get<Device>(`/api/devices/${id}`);
    setDevice(data);
    if (!configDirtyRef.current) {
      setConfigDraft(normalizeConfig(data.configDesired ?? data.config));
    }
  }

  function updateConfigDraft(next: Config) {
    configDirtyRef.current = true;
    setConfigDirty(true);
    setConfigDraft(next);
  }

  useEffect(() => {
    configDirtyRef.current = false;
    setConfigDirty(false);
    loadDevice().catch(() => undefined);
    api.get<Health>('/api/health')
      .then((health) => setPlatformVersion(health.platformVersion ?? '0.6.0'))
      .catch(() => undefined);
    if ('serviceWorker' in navigator) {
      navigator.serviceWorker.register('/sw.js').catch(() => undefined);
    }
    const source = new EventSource('/api/events');
    const refresh = () => loadDevice().catch(() => undefined);
    source.addEventListener('device', refresh);
    source.addEventListener('telemetry', refresh);
    source.addEventListener('config', refresh);
    source.addEventListener('feed', refresh);
    source.addEventListener('alert', refresh);
    source.addEventListener('command', refresh);
    return () => source.close();
  }, [deviceId]);

  useEffect(() => {
    if (tab === 'history') loadAudit().catch(() => undefined);
    if (tab === 'calibration') loadPushConfig().catch(() => undefined);
  }, [tab, deviceId]);

  const online = useMemo(() => {
    if (typeof device.online === 'boolean') return device.online;
    if (!device.seenAt) return false;
    return Date.now() - new Date(device.seenAt).getTime() < 30000;
  }, [device.online, device.seenAt]);
  const activeCommands = useMemo(
    () => (device.commands ?? []).filter((command) => ['queued', 'accepted'].includes(command.state)),
    [device.commands],
  );
  const recentCommands = useMemo(
    () => (device.commands ?? []).filter((command) => !['queued', 'accepted'].includes(command.state)).slice(-5).reverse(),
    [device.commands],
  );

  async function feedNow() {
    if (!online) {
      setNotice('Device offline');
      return;
    }
    await api.send(`/api/devices/${deviceId}/feed`, 'POST', { grams: feedGrams, servo: feedServo });
    setNotice('Fütterung gesendet');
  }

  async function saveConfig() {
    if (!online) {
      setNotice('Device offline');
      return;
    }
    const sentConfig = normalizeConfig(configDraft);
    await api.send(`/api/devices/${deviceId}/config`, 'PUT', sentConfig);
    configDirtyRef.current = false;
    setConfigDirty(false);
    setConfigDraft(sentConfig);
    setDevice((current) => ({
      ...current,
      configDesired: sentConfig,
      configSync: {
        ...current.configSync,
        state: 'pending',
        desiredUpdatedAt: new Date().toISOString(),
        message: 'waiting-for-device-report',
      },
    }));
    setNotice('Konfiguration gesendet');
  }

  async function clearTerminalCommands() {
    const data = await api.send<Device>(`/api/devices/${deviceId}/commands/terminal`, 'DELETE');
    setDevice(data);
    setNotice('Command-Historie bereinigt');
  }

  async function loadPushConfig() {
    try {
      const data = await api.get<PushConfig>('/api/push/config');
      setPushConfig(data);
    } catch { /* ignore */ }
    // iOS Safari ohne PWA: kein Web Push möglich
    const isIOS = /iPad|iPhone|iPod/.test(navigator.userAgent);
    const isStandalone = Boolean((navigator as any).standalone);
    if (isIOS && !isStandalone) {
      setPushState('unsupported');
      return;
    }
    if (!('serviceWorker' in navigator) || !('PushManager' in window)) {
      setPushState('unsupported');
      return;
    }
    if (Notification.permission === 'denied') { setPushState('denied'); return; }
    try {
      const reg = await navigator.serviceWorker.ready;
      const sub = await reg.pushManager.getSubscription();
      setPushState(sub ? 'subscribed' : 'unknown');
    } catch { /* ignore */ }
  }

  async function subscribePush() {
    if (!pushConfig.vapidPublicKey) { setNotice('VAPID nicht konfiguriert'); return; }
    const perm = await Notification.requestPermission();
    if (perm !== 'granted') { setPushState('denied'); setNotice('Berechtigung verweigert'); return; }
    const reg = await navigator.serviceWorker.ready;
    const sub = await reg.pushManager.subscribe({
      userVisibleOnly: true,
      applicationServerKey: urlBase64ToUint8Array(pushConfig.vapidPublicKey),
    });
    await api.send('/api/push/subscribe', 'POST', sub.toJSON());
    setPushState('subscribed');
    setNotice('Browser-Push aktiviert ✓');
  }

  async function unsubscribePush() {
    const reg = await navigator.serviceWorker.ready;
    const sub = await reg.pushManager.getSubscription();
    if (!sub) return;
    await api.send('/api/push/subscribe', 'DELETE', { endpoint: sub.endpoint });
    await sub.unsubscribe();
    setPushState('unknown');
    setNotice('Browser-Push deaktiviert');
  }

  async function sendPushTest() {
    await api.send('/api/push/test', 'POST');
    setNotice('Testbenachrichtigung gesendet');
  }

  async function loadAudit(id = deviceId) {
    try {
      const data = await api.get<AuditEntry[]>(`/api/devices/${id}/audit?limit=100`);
      setAudit(data);
    } catch { /* ignore */ }
  }

  async function clearAlerts() {
    const data = await api.send<Device>(`/api/devices/${deviceId}/alerts`, 'DELETE');
    setDevice(data);
    setNotice('Alerts bereinigt');
  }

  return (
    <main>
      <header className="topbar">
        <div className="title-block">
          <h1>CatFeeder</h1>
          <p>
            <span className={`status-dot ${online ? 'online' : 'offline'}`} />
            {online ? 'Online' : 'Offline'} · Firmware {device.status?.fw ?? device.config?.fw ?? 'unbekannt'} · Plattform {platformVersion} · zuletzt {formatAge(device.ageSeconds)}
          </p>
        </div>
        <label className="device-picker">
          <span>Device</span>
          <input value={deviceId} onChange={(event) => setDeviceId(event.target.value)} />
        </label>
      </header>

      <nav className="tabs">
        <button className={tab === 'dashboard' ? 'active' : ''} onClick={() => setTab('dashboard')}><Activity size={18} />Dashboard</button>
        <button className={tab === 'schedule' ? 'active' : ''} onClick={() => setTab('schedule')}><CalendarClock size={18} />Zeitplan</button>
        <button className={tab === 'calibration' ? 'active' : ''} onClick={() => setTab('calibration')}><Settings size={18} />Kalibrierung</button>
        <button className={tab === 'history' ? 'active' : ''} onClick={() => setTab('history')}><History size={18} />Historie</button>
      </nav>

      {notice && <div className="notice">{notice}</div>}

      {tab === 'dashboard' && (
        <section className="grid">
          <Panel title="Live-Status" icon={<Wifi size={18} />}>
            <Metric label="RSSI" value={`${device.status?.wifiRssi ?? '-'} dBm`} />
            <Metric label="Uptime" value={`${device.status?.uptimeS ?? '-'} s`} />
            <Metric label="Heap" value={`${device.status?.heap ?? '-'} B`} />
            <Metric label="IP" value={device.status?.ip ?? '-'} />
            <Metric label="Status" value={<span className={`badge ${online ? 'ok' : 'warn'}`}>{online ? 'online' : 'offline'}</span>} />
            <Metric label="Config" value={<ConfigSyncBadge sync={device.configSync} />} />
          </Panel>

          <Panel title="Telemetrie" icon={<Gauge size={18} />}>
            <Metric label="Füllstand" value={`${device.telemetry?.fillPct ?? '-'} %`} />
            <Metric label="Abstand" value={`${device.telemetry?.fillMM ?? '-'} mm`} />
            <Metric label="Strom" value={`${device.telemetry?.currentMA ?? '-'} mA`} />
            <Metric label="Winkel" value={`${device.telemetry?.angleDeg ?? '-'}°`} />
          </Panel>

          <Panel title="Sofort-Fütterung" icon={<Play size={18} />}>
            <div className="form-row">
              <label>Gramm<input type="number" value={feedGrams} min={1} max={500} onChange={(e) => setFeedGrams(Number(e.target.value))} /></label>
              <label>Servo<select value={feedServo} onChange={(e) => setFeedServo(Number(e.target.value))}><option value={0}>Beide</option><option value={1}>Servo 1</option><option value={2}>Servo 2</option></select></label>
            </div>
            <button className="primary" onClick={feedNow} disabled={!online}><Play size={18} />Start</button>
            {!online && <p className="muted action-note">Device offline</p>}
          </Panel>

          <Panel title="Alerts" icon={<AlertTriangle size={18} />}>
            {(device.alerts ?? []).slice(-5).reverse().map((alert, index) => (
              <div className={`event ${alert.level === 'critical' ? 'danger' : 'warn'}`} key={index}>
                <strong>{alert.type ?? 'alert'}</strong>
                <span>{alert.message ?? '-'}</span>
                <small>{formatDate(alert.createdAt)}</small>
              </div>
            ))}
            {(device.alerts ?? []).length === 0 && <p className="muted">Keine Alerts</p>}
            {(device.alerts ?? []).length > 0 && <button className="secondary" onClick={clearAlerts}>Alerts leeren</button>}
          </Panel>

          <Panel title="Commands" icon={<Activity size={18} />}>
            {activeCommands.map((command, index) => (
              <div className={`command ${command.state ?? 'queued'}`} key={index}>
                <span className={`badge ${commandStateClass(command.state)}`}>{command.state ?? '-'}</span>
                <strong>{command.type ?? '-'}</strong>
                <small>{formatDate(command.updatedAt)}</small>
              </div>
            ))}
            {recentCommands.map((command, index) => (
              <div className={`command quiet ${command.state ?? 'done'}`} key={`recent-${index}`}>
                <span className={`badge ${commandStateClass(command.state)}`}>{command.state ?? '-'}</span>
                <strong>{command.type ?? '-'}</strong>
                <small>{formatDate(command.updatedAt)}</small>
              </div>
            ))}
            {(device.commands ?? []).length === 0 && <p className="muted">Keine Commands</p>}
            {(device.commands ?? []).some((command) => commandStateClass(command.state) !== 'warn') && (
              <button className="secondary" onClick={clearTerminalCommands}>Erledigte leeren</button>
            )}
          </Panel>
        </section>
      )}

      {tab === 'schedule' && (
        <Panel title="Fütterungszeiten" icon={<CalendarClock size={18} />} wide>
          <ConfigSyncNote sync={device.configSync} />
          {configDirty && <p className="draft-note">Ungespeicherte Änderungen</p>}
          <button className="primary save-top" onClick={saveConfig} disabled={!online}><Save size={18} />Speichern</button>
          <div className="table">
            {(configDraft.slots ?? []).map((slot, index) => (
              <div className="slot-row" key={index}>
                <label className="check"><input type="checkbox" checked={slot.on} onChange={(e) => updateSlot(index, { on: e.target.checked })} />Aktiv</label>
                <input type="number" value={slot.h} min={0} max={23} onChange={(e) => updateSlot(index, { h: Number(e.target.value) })} />
                <input type="number" value={slot.m} min={0} max={59} onChange={(e) => updateSlot(index, { m: Number(e.target.value) })} />
                <input type="number" value={slot.g} min={1} max={500} onChange={(e) => updateSlot(index, { g: Number(e.target.value) })} />
                <select value={slot.sv} onChange={(e) => updateSlot(index, { sv: Number(e.target.value) })}><option value={0}>Beide</option><option value={1}>Servo 1</option><option value={2}>Servo 2</option></select>
              </div>
            ))}
          </div>
          <button className="primary" onClick={saveConfig} disabled={!online}><Save size={18} />Speichern</button>
          {!online && <p className="muted action-note">Speichern ist offline gesperrt</p>}
        </Panel>
      )}

      {tab === 'calibration' && (
        <Panel title="Kalibrierung" icon={<Settings size={18} />} wide>
          <ConfigSyncNote sync={device.configSync} />
          {configDirty && <p className="draft-note">Ungespeicherte Änderungen</p>}
          <button className="primary save-top" onClick={saveConfig} disabled={!online}><Save size={18} />Speichern</button>
          <div className="settings-grid">
            <NumberField label="Default Gramm" value={configDraft.defaultGrams} onChange={(v) => updateConfigDraft({ ...configDraft, defaultGrams: v })} />
            <NumberField label="Steps pro Gramm" value={configDraft.stepsPerGram} onChange={(v) => updateConfigDraft({ ...configDraft, stepsPerGram: v })} />
            <NumberField label="Stepper Speed" value={configDraft.stepperSpeed} onChange={(v) => updateConfigDraft({ ...configDraft, stepperSpeed: v })} />
            <NumberField label="Blockade mA" value={configDraft.stepperBlockMA} onChange={(v) => updateConfigDraft({ ...configDraft, stepperBlockMA: v })} />
            <NumberField label="Servo Speed" value={configDraft.servoSpeedDPS} onChange={(v) => updateConfigDraft({ ...configDraft, servoSpeedDPS: v })} />
            <NumberField label="S1 Open" value={configDraft.s1Open} onChange={(v) => updateConfigDraft({ ...configDraft, s1Open: v })} />
            <NumberField label="S1 Close" value={configDraft.s1Close} onChange={(v) => updateConfigDraft({ ...configDraft, s1Close: v })} />
            <NumberField label="S2 Open" value={configDraft.s2Open} onChange={(v) => updateConfigDraft({ ...configDraft, s2Open: v })} />
            <NumberField label="S2 Close" value={configDraft.s2Close} onChange={(v) => updateConfigDraft({ ...configDraft, s2Close: v })} />
          </div>
          <button className="primary" onClick={saveConfig} disabled={!online}><Save size={18} />Speichern</button>
          {!online && <p className="muted action-note">Speichern ist offline gesperrt</p>}

          <PushSection
            pushConfig={pushConfig}
            pushState={pushState}
            onSubscribe={subscribePush}
            onUnsubscribe={unsubscribePush}
            onTest={sendPushTest}
          />
        </Panel>
      )}

      {tab === 'history' && (
        <>
          <Panel title="Feed-Events" icon={<History size={18} />} wide>
            {(device.feedLog ?? []).slice().reverse().map((event, index) => (
              <div className={`event ${event.aborted ? 'danger' : ''}`} key={index}>
                <strong>{event.type ?? 'feed'}</strong>
                <span>{event.grams ?? '-'} g · Servo {event.servo ?? '-'}</span>
                {event.blockRetryCount ? <span className="badge warn">{event.blockRetryCount}× Blockade</span> : null}
                <small>{String(event.ts ?? event.receivedAt ?? '')}</small>
              </div>
            ))}
            {(device.feedLog ?? []).length === 0 && <p className="muted">Noch keine Feed-Events</p>}
          </Panel>

          <Panel title="Audit-Log" icon={<Activity size={18} />} wide>
            <p className="muted" style={{ marginBottom: '8px', fontSize: '0.85em' }}>
              Alle signifikanten Ereignisse — Feed-Kommandos, Config-Änderungen, Konnektivität.
              Terminal Commands werden nicht aus der Datenbank gelöscht.
            </p>
            <button className="secondary" onClick={() => loadAudit()} style={{ marginBottom: '12px' }}>
              Aktualisieren
            </button>
            {audit.map((entry) => (
              <div className="audit-row" key={entry.id}>
                <span className={`badge ${auditActorClass(entry.actor)}`}>{entry.actor}</span>
                <span className={`badge ${auditCategoryClass(entry.category)}`}>{entry.category}</span>
                <strong className="audit-action">{entry.action}</strong>
                <small>{formatDate(entry.createdAt)}</small>
              </div>
            ))}
            {audit.length === 0 && <p className="muted">Kein Audit-Eintrag vorhanden</p>}
          </Panel>
        </>
      )}
    </main>
  );

  function updateSlot(index: number, patch: Partial<Slot>) {
    const slots = [...(configDraft.slots ?? defaultSlots())];
    slots[index] = { ...slots[index], ...patch };
    updateConfigDraft({ ...configDraft, slots });
  }
}

function PushSection({
  pushConfig,
  pushState,
  onSubscribe,
  onUnsubscribe,
  onTest,
}: {
  pushConfig: PushConfig;
  pushState: string;
  onSubscribe: () => void;
  onUnsubscribe: () => void;
  onTest: () => void;
}) {
  const isIOS = /iPad|iPhone|iPod/.test(navigator.userAgent);
  const isStandalone = Boolean((navigator as any).standalone);
  const iosNotPWA = isIOS && !isStandalone;

  return (
    <div style={{ marginTop: '24px', borderTop: '1px solid rgba(255,255,255,0.08)', paddingTop: '16px' }}>
      <h3 style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '12px', fontSize: '0.9em' }}>
        <Bell size={16} /> Benachrichtigungen
      </h3>

      {/* ntfy.sh */}
      {pushConfig.ntfyEnabled ? (
        <div className="metric" style={{ flexDirection: 'column', alignItems: 'flex-start', gap: '6px' }}>
          <span>ntfy.sh aktiv</span>
          <a href={pushConfig.ntfyUrl ?? ''} target="_blank" rel="noreferrer" style={{ fontSize: '0.8em', wordBreak: 'break-all' }}>
            {pushConfig.ntfyUrl}
          </a>
          <button className="secondary" onClick={onTest} style={{ marginTop: '4px' }}>Testen</button>
        </div>
      ) : (
        <p className="muted" style={{ fontSize: '0.82em', marginBottom: '8px' }}>
          ntfy.sh: <code>NTFY_URL=https://ntfy.sh/&lt;kanal&gt;</code> in .env setzen.
        </p>
      )}

      {/* Browser Push */}
      <div style={{ marginTop: '10px' }}>
        {iosNotPWA ? (
          <p className="muted" style={{ fontSize: '0.82em' }}>
            Browser-Push auf iOS: App zuerst zum Home-Bildschirm hinzufügen
            (Teilen → „Zum Home-Bildschirm"), dann hier aktivieren.
            Alternativ ntfy.sh über die ntfy-App nutzen.
          </p>
        ) : pushConfig.webPushEnabled ? (
          <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap', alignItems: 'center' }}>
            <span style={{ fontSize: '0.82em' }}>Browser-Push:</span>
            {pushState === 'subscribed' && (
              <>
                <span className="badge ok">Aktiv</span>
                <button className="secondary" onClick={onUnsubscribe}>Deaktivieren</button>
              </>
            )}
            {pushState === 'unknown' && (
              <button className="primary" onClick={onSubscribe}>Aktivieren</button>
            )}
            {pushState === 'denied' && (
              <span className="badge bad">Verweigert – in Browser-Einstellungen zurücksetzen</span>
            )}
            {pushState === 'unsupported' && (
              <span className="badge neutral">Nicht unterstützt</span>
            )}
            <button className="secondary" onClick={onTest}>Testen</button>
          </div>
        ) : (
          <p className="muted" style={{ fontSize: '0.82em' }}>
            Browser-Push: <code>VAPID_PUBLIC/PRIVATE_KEY</code> in .env setzen
            (<code>npx web-push generate-vapid-keys</code>).
          </p>
        )}
      </div>
    </div>
  );
}

function urlBase64ToUint8Array(base64: string) {
  const padding = '='.repeat((4 - (base64.length % 4)) % 4);
  const b64 = (base64 + padding).replace(/-/g, '+').replace(/_/g, '/');
  const raw = window.atob(b64);
  return Uint8Array.from([...raw].map((c) => c.charCodeAt(0)));
}

function auditActorClass(actor: string) {
  if (actor === 'device') return 'ok';
  if (actor === 'remote-ui') return 'info';
  return 'neutral';
}

function auditCategoryClass(category: string) {
  if (category === 'feed') return 'ok';
  if (category === 'alert') return 'bad';
  if (category === 'connectivity') return 'warn';
  if (category === 'push') return 'info';
  return 'neutral';
}

function commandStateClass(state?: string) {
  if (state === 'done' || state === 'accepted') return 'ok';
  if (state === 'timeout' || state === 'aborted' || state === 'rejected') return 'bad';
  return 'warn';
}

function configSyncClass(state?: string) {
  if (state === 'synced') return 'ok';
  if (state === 'drift') return 'bad';
  return 'warn';
}

function configSyncLabel(state?: string) {
  if (state === 'synced') return 'sync';
  if (state === 'pending') return 'pending';
  if (state === 'drift') return 'drift';
  return 'unknown';
}

function ConfigSyncBadge({ sync }: { sync?: ConfigSync }) {
  return <span className={`badge ${configSyncClass(sync?.state)}`}>{configSyncLabel(sync?.state)}</span>;
}

function ConfigSyncNote({ sync }: { sync?: ConfigSync }) {
  return (
    <div className={`sync-note ${configSyncClass(sync?.state)}`}>
      <ConfigSyncBadge sync={sync} />
      <span>{configSyncText(sync)}</span>
    </div>
  );
}

function configSyncText(sync?: ConfigSync) {
  if (sync?.state === 'synced') return `Reported passt zur Desired Config · ${formatDate(sync.reportedUpdatedAt)}`;
  if (sync?.state === 'pending') return `Warte auf Report vom ESP · ${formatDate(sync.desiredUpdatedAt)}`;
  if (sync?.state === 'drift') return `Desired und Reported unterscheiden sich · ${formatDate(sync.reportedUpdatedAt)}`;
  return 'Noch kein Config-Sync-Status vorhanden';
}

function formatAge(seconds?: number | null) {
  if (seconds === null || seconds === undefined) return 'nie';
  if (seconds < 60) return `${seconds}s`;
  return `${Math.round(seconds / 60)}min`;
}

function formatDate(value?: string) {
  if (!value) return '';
  return new Date(value).toLocaleString('de-DE', {
    day: '2-digit',
    month: '2-digit',
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
  });
}

function normalizeConfig(config?: Config): Config {
  return {
    ...config,
    defaultGrams: config?.defaultGrams ?? 20,
    stepsPerGram: config?.stepsPerGram ?? 300,
    stepperSpeed: config?.stepperSpeed ?? 1200,
    stepperBlockMA: config?.stepperBlockMA ?? 700,
    servoSpeedDPS: config?.servoSpeedDPS ?? 1000,
    s1Open: config?.s1Open ?? 90,
    s1Close: config?.s1Close ?? 0,
    s2Open: config?.s2Open ?? 90,
    s2Close: config?.s2Close ?? 0,
    slots: config?.slots ?? defaultSlots(),
  };
}

function defaultSlots(): Slot[] {
  return [
    { on: true, h: 7, m: 30, g: 25, sv: 0 },
    { on: true, h: 18, m: 0, g: 25, sv: 0 },
    { on: false, h: 12, m: 0, g: 20, sv: 0 },
    { on: false, h: 12, m: 0, g: 20, sv: 0 },
  ];
}

function Panel({ title, icon, wide, children }: { title: string; icon: React.ReactNode; wide?: boolean; children: React.ReactNode }) {
  return <section className={`panel ${wide ? 'wide' : ''}`}><h2>{icon}{title}</h2>{children}</section>;
}

function Metric({ label, value }: { label: string; value: React.ReactNode }) {
  return <div className="metric"><span>{label}</span><strong>{value}</strong></div>;
}

function NumberField({ label, value, onChange }: { label: string; value?: number; onChange: (value: number) => void }) {
  return <label>{label}<input type="number" value={value ?? 0} onChange={(event) => onChange(Number(event.target.value))} /></label>;
}

createRoot(document.getElementById('root')!).render(<App />);
