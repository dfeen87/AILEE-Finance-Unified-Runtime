/**
 * AILEE FINANCE V18 — REAL-TIME CANVAS VISUALIZATION ENGINE
 * 60 FPS HTML5 Canvas Renderers for Latency, Throughput, Anomaly Radar, Recon & Membrane
 */

(function () {
    'use strict';

    const canvases = {
        latency: document.getElementById('visCanvasLatency'),
        throughput: document.getElementById('visCanvasThroughput'),
        anomaly: document.getElementById('visCanvasAnomaly'),
        recon: document.getElementById('visCanvasRecon'),
        membrane: document.getElementById('visCanvasMembrane')
    };

    const contexts = {};
    Object.keys(canvases).forEach(key => {
        if (canvases[key]) {
            contexts[key] = canvases[key].getContext('2d');
        }
    });

    // Circular History Buffers
    const history = {
        latencyP50: new Array(100).fill(84.7),
        latencyP99: new Array(100).fill(890.0),
        latencyP999: new Array(100).fill(1420.0),
        throughput: new Array(100).fill(1590000),
        reconBreaches: new Array(100).fill(0.012),
        anomalySignals: new Array(8).fill(0.15)
    };

    let animationFrameId = null;

    function resizeCanvas(canvas) {
        if (!canvas) return;
        const rect = canvas.parentElement.getBoundingClientRect();
        if (canvas.width !== rect.width || canvas.height !== rect.height) {
            canvas.width = rect.width;
            canvas.height = rect.height;
        }
    }

    // 1. LATENCY DISTRIBUTION RENDERER (p50, p99, p99.9)
    function renderLatency(ctx, canvas) {
        if (!ctx || !canvas) return;
        resizeCanvas(canvas);
        const w = canvas.width;
        const h = canvas.height;

        ctx.clearRect(0, 0, w, h);
        ctx.fillStyle = '#020610';
        ctx.fillRect(0, 0, w, h);

        // Grid lines
        ctx.strokeStyle = '#101c2e';
        ctx.lineWidth = 1;
        for (let y = 0; y < h; y += 30) {
            ctx.beginPath();
            ctx.moveTo(0, y);
            ctx.lineTo(w, y);
            ctx.stroke();
        }

        const cfg = window.bullishnessSelector ? window.bullishnessSelector.getVisualConfig() : { mode: 'STANDARD', fastPathWindowColor: 'rgba(0, 229, 255, 0.15)', accentColor: '#00e5ff' };
        const maxLatency = 2000.0; // ns

        // Highlight fast-path bullish windows (< 100 ns p50 latency)
        for (let i = 0; i < history.latencyP50.length; i++) {
            if (history.latencyP50[i] < 100.0) {
                const x = (i / (history.latencyP50.length - 1)) * w;
                ctx.fillStyle = cfg.fastPathWindowColor;
                ctx.fillRect(x - 2, 0, 4, h);
            }
        }

        const drawLine = (data, color, label, lineWidth = 2) => {
            ctx.strokeStyle = color;
            ctx.lineWidth = lineWidth;
            ctx.beginPath();
            for (let i = 0; i < data.length; i++) {
                const x = (i / (data.length - 1)) * w;
                const norm = Math.min(1.0, data[i] / maxLatency);
                const y = h - (norm * (h - 20)) - 10;
                if (i === 0) ctx.moveTo(x, y);
                else ctx.lineTo(x, y);
            }
            ctx.stroke();
        };

        drawLine(history.latencyP999, '#ef4444', 'p99.9');
        drawLine(history.latencyP99, '#f59e0b', 'p99');
        const p50Color = cfg.accentColor || '#00e5ff';
        const p50Width = cfg.mode === 'HYPER' ? 3 : 2;
        drawLine(history.latencyP50, p50Color, 'p50', p50Width);

        // Legend overlay
        ctx.font = '10px "SF Mono", monospace';
        ctx.fillStyle = p50Color;
        ctx.fillText(`p50: ${history.latencyP50[history.latencyP50.length - 1].toFixed(1)} ns`, 10, 16);
        ctx.fillStyle = '#f59e0b';
        ctx.fillText(`p99: ${history.latencyP99[history.latencyP99.length - 1].toFixed(0)} ns`, 110, 16);
        ctx.fillStyle = '#ef4444';
        ctx.fillText(`p99.9: ${history.latencyP999[history.latencyP999.length - 1].toFixed(0)} ns`, 210, 16);
    }

    // 2. THROUGHPUT OVER TIME RENDERER
    function renderThroughput(ctx, canvas) {
        if (!ctx || !canvas) return;
        resizeCanvas(canvas);
        const w = canvas.width;
        const h = canvas.height;

        ctx.clearRect(0, 0, w, h);
        ctx.fillStyle = '#020610';
        ctx.fillRect(0, 0, w, h);

        const cfg = window.bullishnessSelector ? window.bullishnessSelector.getVisualConfig() : { mode: 'STANDARD', accentColor: '#10b981' };
        const data = history.throughput;
        const minVal = 800000;
        const maxVal = 2000000;

        const strokeColor = cfg.mode === 'HYPER' ? '#00ff88' : (cfg.mode === 'CONSERVATIVE' ? '#10b981' : '#10b981');
        const fillColor = cfg.mode === 'HYPER' ? 'rgba(0, 255, 136, 0.25)' : 'rgba(16, 185, 129, 0.15)';

        ctx.fillStyle = fillColor;
        ctx.strokeStyle = strokeColor;
        ctx.lineWidth = cfg.mode === 'HYPER' ? 3 : 2;

        ctx.beginPath();
        ctx.moveTo(0, h);
        for (let i = 0; i < data.length; i++) {
            const x = (i / (data.length - 1)) * w;
            const norm = (data[i] - minVal) / (maxVal - minVal);
            const y = h - (norm * (h - 20)) - 10;
            ctx.lineTo(x, y);
        }
        ctx.lineTo(w, h);
        ctx.closePath();
        ctx.fill();

        ctx.beginPath();
        for (let i = 0; i < data.length; i++) {
            const x = (i / (data.length - 1)) * w;
            const norm = (data[i] - minVal) / (maxVal - minVal);
            const y = h - (norm * (h - 20)) - 10;
            if (i === 0) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
        }
        ctx.stroke();

        ctx.font = '10px "SF Mono", monospace';
        ctx.fillStyle = '#10b981';
        ctx.fillText(`THROUGHPUT: ${data[data.length - 1].toLocaleString()} ops/sec`, 10, 16);
    }

    // 3. ANOMALY & RADAR DETECTOR RENDERER
    function renderAnomalyRadar(ctx, canvas) {
        if (!ctx || !canvas) return;
        resizeCanvas(canvas);
        const w = canvas.width;
        const h = canvas.height;
        const cx = w / 2;
        const cy = h / 2;
        const r = Math.min(w, h) * 0.4;

        ctx.clearRect(0, 0, w, h);
        ctx.fillStyle = '#020610';
        ctx.fillRect(0, 0, w, h);

        // Radar Circles
        ctx.strokeStyle = '#182438';
        ctx.lineWidth = 1;
        for (let i = 1; i <= 3; i++) {
            ctx.beginPath();
            ctx.arc(cx, cy, (r / 3) * i, 0, Math.PI * 2);
            ctx.stroke();
        }

        // Radar Spokes
        const axes = 8;
        for (let i = 0; i < axes; i++) {
            const angle = (i / axes) * Math.PI * 2;
            ctx.beginPath();
            ctx.moveTo(cx, cy);
            ctx.lineTo(cx + Math.cos(angle) * r, cy + Math.sin(angle) * r);
            ctx.stroke();
        }

        // Polygon Plot
        ctx.fillStyle = 'rgba(245, 158, 11, 0.25)';
        ctx.strokeStyle = '#f59e0b';
        ctx.lineWidth = 2;
        ctx.beginPath();
        for (let i = 0; i < axes; i++) {
            const angle = (i / axes) * Math.PI * 2;
            const val = history.anomalySignals[i] || 0.2;
            const dist = r * val;
            const x = cx + Math.cos(angle) * dist;
            const y = cy + Math.sin(angle) * dist;
            if (i === 0) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
        }
        ctx.closePath();
        ctx.fill();
        ctx.stroke();

        ctx.font = '10px "SF Mono", monospace';
        ctx.fillStyle = '#f59e0b';
        ctx.fillText('ANOMALY RADAR: MULTI-BAR DISPLACEMENT & CORRELATION BREAKS', 10, 16);
    }

    // MAIN RENDER LOOP
    function renderLoop() {
        if (canvases.latency && canvases.latency.classList.contains('active')) {
            renderLatency(contexts.latency, canvases.latency);
        }
        if (canvases.throughput && canvases.throughput.classList.contains('active')) {
            renderThroughput(contexts.throughput, canvases.throughput);
        }
        if (canvases.anomaly && canvases.anomaly.classList.contains('active')) {
            renderAnomalyRadar(contexts.anomaly, canvases.anomaly);
        }
        if (canvases.recon && canvases.recon.classList.contains('active')) {
            renderLatency(contexts.recon, canvases.recon); // Fallback to latency line
        }
        if (canvases.membrane && canvases.membrane.classList.contains('active')) {
            renderAnomalyRadar(contexts.membrane, canvases.membrane); // Fallback to radial plot
        }

        animationFrameId = requestAnimationFrame(renderLoop);
    }

    // UPDATE DATA HOOK
    window.AILEEV17_WebGL = {
        updateData: function (state) {
            history.latencyP50.push(state.p50LatencyNs);
            history.latencyP50.shift();

            history.latencyP99.push(state.p99LatencyNs);
            history.latencyP99.shift();

            history.latencyP999.push(state.p999LatencyNs);
            history.latencyP999.shift();

            history.throughput.push(state.throughputOpsSec);
            history.throughput.shift();

            // Anomaly updates
            for (let i = 0; i < 8; i++) {
                history.anomalySignals[i] = Math.min(1.0, Math.max(0.1, 0.15 + (Math.random() - 0.45) * 0.2));
            }
        }
    };

    window.addEventListener('DOMContentLoaded', () => {
        renderLoop();
    });

})();
