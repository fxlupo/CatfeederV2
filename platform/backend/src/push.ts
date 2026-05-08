import webPushLib from 'web-push';

export type PushPayload = {
  title: string;
  body: string;
  tags?: string[];
  priority?: 'min' | 'low' | 'default' | 'high' | 'urgent';
};

export type WebPushSubscription = {
  endpoint: string;
  keys: { p256dh: string; auth: string };
};

export class PushService {
  readonly vapidPublicKey: string;
  private readonly vapidEnabled: boolean;
  private readonly ntfyUrl: string | null;
  private readonly subs = new Map<string, WebPushSubscription>();

  constructor() {
    this.ntfyUrl = process.env.NTFY_URL ?? null;
    const pubKey = process.env.VAPID_PUBLIC_KEY ?? '';
    const privKey = process.env.VAPID_PRIVATE_KEY ?? '';
    const contact = process.env.VAPID_CONTACT ?? 'mailto:admin@catfeeder.local';
    this.vapidPublicKey = pubKey;
    this.vapidEnabled = !!(pubKey && privKey);

    if (this.vapidEnabled) {
      webPushLib.setVapidDetails(contact, pubKey, privKey);
      console.log('[push] Web Push (VAPID) aktiviert');
    } else {
      console.log('[push] Web Push deaktiviert – VAPID_PUBLIC_KEY/VAPID_PRIVATE_KEY nicht gesetzt');
      console.log('[push] Schlüssel generieren: npx web-push generate-vapid-keys');
    }
    if (this.ntfyUrl) {
      console.log(`[push] ntfy.sh aktiviert: ${this.ntfyUrl}`);
    }
  }

  hasNtfy() { return !!this.ntfyUrl; }
  hasVapid() { return this.vapidEnabled; }
  subscriptionCount() { return this.subs.size; }

  addSubscription(sub: WebPushSubscription) {
    this.subs.set(sub.endpoint, sub);
  }

  removeSubscription(endpoint: string) {
    this.subs.delete(endpoint);
  }

  loadSubscriptions(rows: Array<{ endpoint: string; p256dh: string; auth: string }>) {
    for (const row of rows) {
      this.subs.set(row.endpoint, {
        endpoint: row.endpoint,
        keys: { p256dh: row.p256dh, auth: row.auth },
      });
    }
    if (rows.length > 0) console.log(`[push] ${rows.length} Web-Push-Subscriptions geladen`);
  }

  async broadcast(payload: PushPayload): Promise<void> {
    const jobs: Promise<void>[] = [];
    if (this.ntfyUrl) jobs.push(this.sendNtfy(payload));
    if (this.vapidEnabled) {
      for (const [endpoint, sub] of this.subs) {
        jobs.push(this.sendWebPush(endpoint, sub, payload));
      }
    }
    await Promise.allSettled(jobs);
  }

  private async sendNtfy(payload: PushPayload): Promise<void> {
    try {
      const res = await fetch(this.ntfyUrl!, {
        method: 'POST',
        headers: {
          Title: payload.title,
          Tags: ['catfeeder', ...(payload.tags ?? [])].join(','),
          Priority: payload.priority ?? 'default',
          'Content-Type': 'text/plain; charset=utf-8',
        },
        body: payload.body,
      });
      if (!res.ok) console.warn(`[push/ntfy] ${res.status} ${res.statusText}`);
      else console.log('[push/ntfy] gesendet');
    } catch (err) {
      console.error('[push/ntfy]', err);
    }
  }

  private async sendWebPush(endpoint: string, sub: WebPushSubscription, payload: PushPayload): Promise<void> {
    try {
      await webPushLib.sendNotification(
        { endpoint, keys: sub.keys },
        JSON.stringify({
          title: payload.title,
          body: payload.body,
          icon: '/icons/catfeeder-icon.svg',
          badge: '/icons/catfeeder-icon.svg',
        }),
      );
      console.log(`[push/webpush] gesendet → ${endpoint.slice(-30)}`);
    } catch (err: any) {
      console.warn(`[push/webpush] ${err.message}`);
      // 410 Gone = Subscription abgelaufen, aus Liste entfernen
      if (err.statusCode === 410) {
        this.subs.delete(endpoint);
        console.log('[push/webpush] Subscription entfernt (expired)');
      }
    }
  }
}
