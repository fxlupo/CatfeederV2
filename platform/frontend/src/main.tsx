import React, { useEffect, useMemo, useState } from 'react';
import { createRoot } from 'react-dom/client';
import {
  Activity,
  AlertTriangle,
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
  seenAt?: string;
  status?: Record<string, any>;
  telemetry?: Record<string, any>;
  config?: Config;
  feedLog?: Record<string, any>[];
  alerts?: Record<string, any>[];
  commands?: Record<string, any>[];
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

  async function loadDevice(id = deviceId) {
    const data = await api.get<Device>(`/api/devices/${id}`);
    setDevice(data);
    setConfigDraft(normalizeConfig(data.config));
  }

  useEffect(() => {
    loadDevice().catch(() => undefined);
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

  const online = useMemo(() => {
    if (!device.seenAt) return false;
    return Date.now() - new Date(device.seenAt).getTime() < 30000;
  }, [device.seenAt]);

  async function feedNow() {
    await api.send(`/api/devices/${deviceId}/feed`, 'POST', { grams: feedGrams, servo: feedServo });
    setNotice('Fütterung gesendet');
  }

  async function saveConfig() {
    await api.send(`/api/devices/${deviceId}/config`, 'PUT', configDraft);
    setNotice('Konfiguration gesendet');
  }

  return (
    <main>
      <header className="topbar">
        <div>
          <h1>CatFeeder</h1>
          <p>{online ? 'Online' : 'Wartet auf Device'} · {device.status?.fw ?? device.config?.fw ?? 'Firmware unbekannt'}</p>
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
            <button className="primary" onClick={feedNow}><Play size={18} />Start</button>
          </Panel>

          <Panel title="Alerts" icon={<AlertTriangle size={18} />}>
            {(device.alerts ?? []).slice(-5).reverse().map((alert, index) => (
              <div className="event danger" key={index}>{alert.message ?? alert.type}</div>
            ))}
            {(device.alerts ?? []).length === 0 && <p className="muted">Keine Alerts</p>}
          </Panel>
        </section>
      )}

      {tab === 'schedule' && (
        <Panel title="Fütterungszeiten" icon={<CalendarClock size={18} />} wide>
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
          <button className="primary" onClick={saveConfig}><Save size={18} />Speichern</button>
        </Panel>
      )}

      {tab === 'calibration' && (
        <Panel title="Kalibrierung" icon={<Settings size={18} />} wide>
          <div className="settings-grid">
            <NumberField label="Default Gramm" value={configDraft.defaultGrams} onChange={(v) => setConfigDraft({ ...configDraft, defaultGrams: v })} />
            <NumberField label="Steps pro Gramm" value={configDraft.stepsPerGram} onChange={(v) => setConfigDraft({ ...configDraft, stepsPerGram: v })} />
            <NumberField label="Stepper Speed" value={configDraft.stepperSpeed} onChange={(v) => setConfigDraft({ ...configDraft, stepperSpeed: v })} />
            <NumberField label="Blockade mA" value={configDraft.stepperBlockMA} onChange={(v) => setConfigDraft({ ...configDraft, stepperBlockMA: v })} />
            <NumberField label="Servo Speed" value={configDraft.servoSpeedDPS} onChange={(v) => setConfigDraft({ ...configDraft, servoSpeedDPS: v })} />
            <NumberField label="S1 Open" value={configDraft.s1Open} onChange={(v) => setConfigDraft({ ...configDraft, s1Open: v })} />
            <NumberField label="S1 Close" value={configDraft.s1Close} onChange={(v) => setConfigDraft({ ...configDraft, s1Close: v })} />
            <NumberField label="S2 Open" value={configDraft.s2Open} onChange={(v) => setConfigDraft({ ...configDraft, s2Open: v })} />
            <NumberField label="S2 Close" value={configDraft.s2Close} onChange={(v) => setConfigDraft({ ...configDraft, s2Close: v })} />
          </div>
          <button className="primary" onClick={saveConfig}><Save size={18} />Speichern</button>
        </Panel>
      )}

      {tab === 'history' && (
        <Panel title="Historie" icon={<History size={18} />} wide>
          {(device.feedLog ?? []).slice().reverse().map((event, index) => (
            <div className={`event ${event.aborted ? 'danger' : ''}`} key={index}>
              <strong>{event.type ?? 'feed'}</strong>
              <span>{event.grams ?? '-'} g · Servo {event.servo ?? '-'}</span>
              <small>{event.ts ?? event.receivedAt ?? ''}</small>
            </div>
          ))}
          {(device.feedLog ?? []).length === 0 && <p className="muted">Noch keine Feed-Events</p>}
        </Panel>
      )}
    </main>
  );

  function updateSlot(index: number, patch: Partial<Slot>) {
    const slots = [...(configDraft.slots ?? defaultSlots())];
    slots[index] = { ...slots[index], ...patch };
    setConfigDraft({ ...configDraft, slots });
  }
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
