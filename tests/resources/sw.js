// Service Worker for automated testing
// This service worker handles push notifications and notification clicks

self.addEventListener('install', (event) => {
    console.log('[SW] Install event');
    self.skipWaiting();
});

self.addEventListener('activate', (event) => {
    console.log('[SW] Activate event');
    event.waitUntil(clients.claim());
});

self.addEventListener('push', (event) => {
    console.log('[SW] Push event received:', event);

    let title = 'Push Notification';
    let options = {
        body: 'This is a test push notification from service worker',
        icon: '/icon.png',
        badge: '/badge.png',
        tag: 'push-notification',
        requireInteraction: false
    };

    if (event.data) {
        try {
            const payload = event.data.json();
            title = payload.title || title;
            options.body = payload.body || options.body;
        } catch (e) {
            options.body = event.data.text();
        }
    }

    event.waitUntil(
        self.registration.showNotification(title, options)
    );
});

self.addEventListener('notificationclick', (event) => {
    console.log('[SW] Notification clicked:', event.notification.tag);
    event.notification.close();

    event.waitUntil(
        clients.openWindow('/')
    );
});
