/**
 * AILEE Finance V18 — AF-WSX WebSocket Runtime Client
 * Auto-connects and deterministically reconnects to the FS-Gateway module.
 * Endpoint: ws://<host>:9002/ailee/finance/runtime
 */

(function () {
    'use strict';

    class AFWSXClient {
        constructor(options = {}) {
            this.host = options.host || window.location.hostname || 'localhost';
            this.port = options.port || 9002;
            this.path = options.path || '/ailee/finance/runtime';
            this.reconnectIntervalMs = options.reconnectIntervalMs || 2000;

            this.ws = null;
            this.connected = false;
            this.reconnectTimer = null;
            this.messageHandlers = [];
            this.statusChangeHandlers = [];

            this.connect();
        }

        get url() {
            return `ws://${this.host}:${this.port}${this.path}`;
        }

        connect() {
            if (this.ws && (this.ws.readyState === WebSocket.CONNECTING || this.ws.readyState === WebSocket.OPEN)) {
                return;
            }

            try {
                this.ws = new WebSocket(this.url);

                this.ws.onopen = () => {
                    this.connected = true;
                    if (this.reconnectTimer) {
                        clearTimeout(this.reconnectTimer);
                        this.reconnectTimer = null;
                    }
                    this.notifyStatusChange(true);
                };

                this.ws.onmessage = (event) => {
                    try {
                        const data = JSON.parse(event.data);
                        this.handleMessage(data);
                    } catch (err) {
                        console.error('[AF-WSX] JSON parse error:', err);
                    }
                };

                this.ws.onerror = (err) => {
                    // Handled gracefully in onclose
                };

                this.ws.onclose = () => {
                    const wasConnected = this.connected;
                    this.connected = false;
                    this.ws = null;

                    if (wasConnected) {
                        this.notifyStatusChange(false);
                    }

                    this.scheduleReconnect();
                };
            } catch (e) {
                this.scheduleReconnect();
            }
        }

        scheduleReconnect() {
            if (!this.reconnectTimer) {
                this.reconnectTimer = setTimeout(() => {
                    this.reconnectTimer = null;
                    this.connect();
                }, this.reconnectIntervalMs);
            }
        }

        handleMessage(data) {
            for (const handler of this.messageHandlers) {
                try {
                    handler(data);
                } catch (e) {
                    console.error('[AF-WSX] Handler error:', e);
                }
            }
        }

        onMessage(handler) {
            if (typeof handler === 'function') {
                this.messageHandlers.push(handler);
            }
        }

        onStatusChange(handler) {
            if (typeof handler === 'function') {
                this.statusChangeHandlers.push(handler);
            }
        }

        notifyStatusChange(isConnected) {
            for (const handler of this.statusChangeHandlers) {
                try {
                    handler(isConnected, this.url);
                } catch (e) {
                    console.error('[AF-WSX] Status handler error:', e);
                }
            }
        }
    }

    // Attach globally
    window.AFWSXClient = AFWSXClient;

})();
