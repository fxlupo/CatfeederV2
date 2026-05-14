const CACHE_NAME = 'catfeeder-ui-0.7.1';
const APP_SHELL = ['/', '/index.html', '/manifest.webmanifest', '/icons/catfeeder-icon.svg'];

self.addEventListener('install', (event) => {
  event.waitUntil(caches.open(CACHE_NAME).then((cache) => cache.addAll(APP_SHELL)));
  self.skipWaiting();
});

self.addEventListener('activate', (event) => {
  event.waitUntil(
    caches.keys().then((keys) =>
      Promise.all(keys.filter((key) => key !== CACHE_NAME).map((key) => caches.delete(key))),
    ),
  );
  self.clients.claim();
});

// ── Push-Benachrichtigungen ──────────────────────────────────────────────────

self.addEventListener('push', (event) => {
  if (!event.data) return;
  let data;
  try { data = event.data.json(); } catch { data = { title: 'CatFeeder', body: event.data.text() }; }
  const options = {
    body: data.body ?? '',
    icon: '/icons/catfeeder-icon.svg',
    badge: '/icons/catfeeder-icon.svg',
    tag: 'catfeeder-alert',
    renotify: true,
    requireInteraction: data.priority === 'high' || data.priority === 'urgent',
  };
  event.waitUntil(self.registration.showNotification(data.title ?? 'CatFeeder', options));
});

self.addEventListener('notificationclick', (event) => {
  event.notification.close();
  event.waitUntil(
    clients.matchAll({ type: 'window', includeUncontrolled: true }).then((wins) => {
      if (wins.length > 0) return wins[0].focus();
      return clients.openWindow('/');
    }),
  );
});

// ── App-Shell-Cache ──────────────────────────────────────────────────────────

self.addEventListener('fetch', (event) => {
  const { request } = event;
  const url = new URL(request.url);

  if (request.method !== 'GET') return;
  if (url.pathname.startsWith('/api/')) return;

  event.respondWith(
    fetch(request)
      .then((response) => {
        const copy = response.clone();
        caches.open(CACHE_NAME).then((cache) => cache.put(request, copy));
        return response;
      })
      .catch(() => caches.match(request).then((cached) => cached || caches.match('/'))),
  );
});
