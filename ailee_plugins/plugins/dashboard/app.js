// Configuration
const colors = {
    bgCore: '#050a1f', // Deep navy
    electricBlue: '#00d2ff',
    neonOrange: '#ff6b00',
    gold: '#ffd700',
    silver: '#c0c0c0',
    indigo: '#4b0082',
    red: '#ff003c',
    green: '#00ff66'
};

// Distribute 9 nodes evenly around the circle
const nodesData = [
    { id: 'BTC', label: 'BTC', angle: Math.PI * (0 / 4.5), color: colors.neonOrange, risk: 0, weight: 0 },
    { id: 'ETH', label: 'ETH', angle: Math.PI * (1 / 4.5), color: colors.electricBlue, risk: 0, weight: 0 },
    { id: 'USD_FOREX', label: 'USD-FOREX', angle: Math.PI * (2 / 4.5), color: colors.electricBlue, risk: 0, weight: 0 },
    { id: 'OIL', label: 'OIL', angle: Math.PI * (3 / 4.5), color: colors.silver, risk: 0, weight: 0 },
    { id: 'GOLD', label: 'GOLD', angle: Math.PI * (4 / 4.5), color: colors.gold, risk: 0, weight: 0 },
    { id: 'SILVER', label: 'SILVER', angle: Math.PI * (5 / 4.5), color: colors.silver, risk: 0, weight: 0 },
    { id: 'COPPER', label: 'COPPER', angle: Math.PI * (6 / 4.5), color: colors.neonOrange, risk: 0, weight: 0 },
    { id: 'NATGAS', label: 'NATGAS', angle: Math.PI * (7 / 4.5), color: colors.electricBlue, risk: 0, weight: 0 },
    { id: 'PLATINUM', label: 'PLATINUM', angle: Math.PI * (8 / 4.5), color: colors.silver, risk: 0, weight: 0 }
];

const canvas = document.getElementById('ailleCanvas');
const textCanvas = document.getElementById('textCanvas');

// Attempt to initialize WebGL first, fallback to 2D
let webGLRenderer = null;
let ctx = null;
let textCtx = null;

try {
    webGLRenderer = new WebGLRenderer(canvas, colors, nodesData);
} catch (e) {
    console.warn("WebGL Renderer failed to initialize, falling back to Canvas 2D.", e);
}

if (!webGLRenderer || !webGLRenderer.gl) {
    ctx = canvas.getContext('2d');
    webGLRenderer = null;
    textCanvas.style.display = 'none';
} else {
    textCtx = textCanvas.getContext('2d');
}

let width, height, centerX, centerY;

let macroData = { risk: 0, weight: 0 };
let totalExposure = 0;
let time = 0;

function resize() {
    width = window.innerWidth;
    height = window.innerHeight;
    centerX = width / 2;
    centerY = height / 2;

    if (webGLRenderer) {
        webGLRenderer.resize();
        textCanvas.width = width;
        textCanvas.height = height;
    } else {
        canvas.width = width;
        canvas.height = height;
    }
}

window.addEventListener('resize', resize);
resize();

// WebSocket connection to LiveAdvisoryObserver
function connectWebSocket() {
    const ws = new WebSocket('ws://localhost:9002');

    ws.onopen = () => {
        console.log('Connected to AILLE Engine WebSocket');
    };

    ws.onmessage = (event) => {
        const data = JSON.parse(event.data);
        const advisories = data.advisories;
        const balancing = data.exposure_balancing;

        if (advisories) {
            nodesData.forEach(node => {
                if (advisories[node.id]) {
                    node.risk = advisories[node.id].risk_score;
                    node.weight = advisories[node.id].recommended_weight;
                }
            });
            if (advisories.MacroSignal) {
                macroData.risk = advisories.MacroSignal.risk_score;
                macroData.weight = advisories.MacroSignal.recommended_weight;
            }
        }
        if (balancing) {
            totalExposure = balancing.total_exposure;
        }

        if (webGLRenderer) {
            webGLRenderer.updateData(advisories, balancing);
        }
    };

    ws.onerror = (err) => {
        console.error('WebSocket Error:', err);
    };

    ws.onclose = () => {
        console.log('WebSocket connection closed. Retrying in 2 seconds...');
        setTimeout(connectWebSocket, 2000);
    };
}

connectWebSocket();

function getColorForRisk(baseColor, risk) {
    if (risk > 60) return colors.red;
    if (risk < 30) return colors.green;
    return baseColor;
}

function drawBackground() {
    // Radial gradient background
    const gradient = ctx.createRadialGradient(centerX, centerY, 50, centerX, centerY, width / 1.5);
    gradient.addColorStop(0, '#0a1930');
    gradient.addColorStop(1, colors.bgCore);
    ctx.fillStyle = gradient;
    ctx.fillRect(0, 0, width, height);

    // Subtle market volatility waves
    ctx.beginPath();
    for (let x = 0; x < width; x += 10) {
        // use macro risk to scale volatility
        let volScale = 50 + (macroData.risk / 2);
        let y = centerY + Math.sin(x * 0.01 + time * 2) * volScale * Math.sin(time * 0.5);
        ctx.lineTo(x, y);
    }
    ctx.strokeStyle = 'rgba(0, 210, 255, 0.05)';
    ctx.lineWidth = 1;
    ctx.stroke();

    // Background Tickers/Candlesticks (Abstract representations)
    ctx.fillStyle = 'rgba(192, 192, 192, 0.03)';
    for(let i=0; i < 20; i++) {
        let h = 20 + Math.abs(Math.sin(time * 3 + i) * 60);
        ctx.fillRect(width * 0.1 + i * 40, height * 0.8 - h/2, 5, h);
    }
}

function drawCentralCore() {
    // Glowing central core (AILLEEngine)
    const coreRadius = 40;

    ctx.shadowBlur = 30;
    ctx.shadowColor = colors.electricBlue;

    ctx.beginPath();
    ctx.arc(centerX, centerY, coreRadius, 0, Math.PI * 2);
    ctx.fillStyle = '#ffffff';
    ctx.fill();

    ctx.beginPath();
    ctx.arc(centerX, centerY, coreRadius + 10 + Math.sin(time * 4) * 5, 0, Math.PI * 2);
    ctx.strokeStyle = colors.electricBlue;
    ctx.lineWidth = 2;
    ctx.stroke();

    ctx.shadowBlur = 0; // Reset shadow
}

function drawNodes() {
    const radius = Math.min(width, height) * 0.3;

    nodesData.forEach(node => {
        const nx = centerX + Math.cos(node.angle) * radius;
        const ny = centerY + Math.sin(node.angle) * radius;
        const nodeColor = getColorForRisk(node.color, node.risk);

        // Connecting Circuit Lines
        ctx.beginPath();
        ctx.moveTo(centerX, centerY);
        ctx.lineTo(nx, ny);
        ctx.strokeStyle = 'rgba(192, 192, 192, 0.3)';
        ctx.lineWidth = 1;
        ctx.stroke();

        // Animated Data Flow on Lines (speed based on risk)
        const speed = 2 + (node.risk / 50);
        const flowPos = (Math.sin(time * speed + node.angle) + 1) / 2; // 0 to 1
        const fx = centerX + (nx - centerX) * flowPos;
        const fy = centerY + (ny - centerY) * flowPos;

        ctx.beginPath();
        ctx.arc(fx, fy, 3, 0, Math.PI * 2);
        ctx.fillStyle = nodeColor;
        ctx.shadowBlur = 10;
        ctx.shadowColor = nodeColor;
        ctx.fill();
        ctx.shadowBlur = 0;

        // Node Shape
        ctx.beginPath();
        ctx.arc(nx, ny, 15 + (node.weight * 5), 0, Math.PI * 2); // Size depends on weight
        ctx.fillStyle = colors.bgCore;
        ctx.fill();

        ctx.beginPath();
        ctx.arc(nx, ny, 15 + (node.weight * 5), 0, Math.PI * 2);
        ctx.strokeStyle = nodeColor;
        ctx.lineWidth = 3;
        ctx.shadowBlur = 15;
        ctx.shadowColor = nodeColor;
        ctx.stroke();
        ctx.shadowBlur = 0;

        // Node Label
        ctx.fillStyle = colors.silver;
        ctx.font = "12px sans-serif";
        ctx.textAlign = "center";
        ctx.fillText(`${node.label} (${node.risk.toFixed(1)}%)`, nx, ny - 30);
    });
}

function drawExposureBars() {
    // Draw total exposure and per-asset exposure bars
    const startX = 20;
    const startY = Math.min(height - 40 - (nodesData.length * 25) - 30, height / 2 - 100);

    ctx.fillStyle = colors.silver;
    ctx.font = "14px sans-serif";
    ctx.textAlign = "left";
    ctx.fillText(`Total Exposure: ${totalExposure.toFixed(2)}`, startX, startY);

    nodesData.forEach((node, i) => {
        const y = startY + 25 + (i * 25);
        ctx.fillStyle = colors.silver;
        ctx.font = "12px sans-serif";
        ctx.fillText(node.label, startX, y + 10);

        const barWidth = 150 * node.weight;
        ctx.fillStyle = node.color;
        ctx.fillRect(startX + 80, y, barWidth, 12);

        ctx.fillStyle = colors.silver;
        ctx.fillText(node.weight.toFixed(2), startX + 80 + barWidth + 5, y + 10);
    });
}

function drawMacroSignal() {
    // MacroSignal (MSM) orbiting ring / golden halo
    const orbitRadius = Math.min(width, height) * 0.4;
    const macroColor = getColorForRisk(colors.gold, macroData.risk);

    ctx.beginPath();
    ctx.arc(centerX, centerY, orbitRadius, 0, Math.PI * 2);
    ctx.strokeStyle = 'rgba(255, 215, 0, 0.1)';
    ctx.lineWidth = 1;
    ctx.stroke();

    const msmAngle = time * (0.5 + macroData.risk / 100);
    const mx = centerX + Math.cos(msmAngle) * orbitRadius;
    const my = centerY + Math.sin(msmAngle) * orbitRadius;

    ctx.beginPath();
    ctx.arc(mx, my, 8 + (macroData.weight * 4), 0, Math.PI * 2);
    ctx.fillStyle = macroColor;
    ctx.shadowBlur = 20;
    ctx.shadowColor = macroColor;
    ctx.fill();
    ctx.shadowBlur = 0;

    // Label for MSM
    ctx.fillStyle = macroColor;
    ctx.font = "10px sans-serif";
    ctx.fillText(`MacroSignal (${macroData.risk.toFixed(1)}%)`, mx + 20, my);
}

function render() {
    if (webGLRenderer) {
        webGLRenderer.render();

        textCtx.clearRect(0, 0, width, height);

        // Draw labels on text canvas
        const radius = Math.min(width, height) * 0.3;
        nodesData.forEach(node => {
            const nx = centerX + Math.cos(node.angle) * radius;
            const ny = centerY + Math.sin(node.angle) * radius;
            textCtx.fillStyle = colors.silver;
            textCtx.font = "12px sans-serif";
            textCtx.textAlign = "center";
            textCtx.fillText(`${node.label} (${node.risk.toFixed(1)}%)`, nx, ny - 30);
        });

        // Draw MacroSignal label
        const orbitRadius = Math.min(width, height) * 0.4;
        const msmAngle = time * (0.5 + macroData.risk / 100);
        const mx = centerX + Math.cos(msmAngle) * orbitRadius;
        const my = centerY + Math.sin(msmAngle) * orbitRadius;
        textCtx.fillStyle = getColorForRisk(colors.gold, macroData.risk);
        textCtx.font = "10px sans-serif";
        textCtx.fillText(`MacroSignal (${macroData.risk.toFixed(1)}%)`, mx + 20, my);

        // Draw Exposure Text Overlay
        const startX = 20;
        const startY = Math.min(height - 40 - (nodesData.length * 25) - 30, height / 2 - 100);

        textCtx.fillStyle = colors.silver;
        textCtx.font = "14px sans-serif";
        textCtx.textAlign = "left";
        textCtx.fillText(`Total Exposure: ${totalExposure.toFixed(2)}`, startX, startY);

        nodesData.forEach((node, i) => {
            const y = startY + 25 + (i * 25);
            textCtx.fillStyle = colors.silver;
            textCtx.font = "12px sans-serif";
            textCtx.fillText(node.label, startX, y + 10);

            const barWidth = 150 * node.weight;
            textCtx.fillStyle = colors.silver;
            textCtx.fillText(node.weight.toFixed(2), startX + 80 + barWidth + 5, y + 10);
        });

        time += 0.02;

    } else {
        ctx.clearRect(0, 0, width, height);

        time += 0.02;

        drawBackground();
        drawNodes();
        drawCentralCore();
        drawMacroSignal();
        drawExposureBars();
    }

    requestAnimationFrame(render);
}

// Start loop
render();
