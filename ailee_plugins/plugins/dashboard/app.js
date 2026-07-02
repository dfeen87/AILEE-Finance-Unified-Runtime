const canvas = document.getElementById('ailleCanvas');
const ctx = canvas.getContext('2d');

let width, height, centerX, centerY;

// Configuration
const colors = {
    bgCore: '#050a1f', // Deep navy
    electricBlue: '#00d2ff',
    neonOrange: '#ff6b00',
    gold: '#ffd700',
    silver: '#c0c0c0',
    indigo: '#4b0082'
};

const nodesData = [
    { label: 'BTC', angle: Math.PI * 1.25, color: colors.neonOrange },
    { label: 'ETH', angle: Math.PI * 1.75, color: colors.electricBlue },
    { label: 'Commodities', angle: Math.PI * 0.25, color: colors.silver },
    { label: 'USD-FOREX', angle: Math.PI * 0.75, color: colors.electricBlue }
];

let time = 0;

function resize() {
    width = window.innerWidth;
    height = window.innerHeight;
    canvas.width = width;
    canvas.height = height;
    centerX = width / 2;
    centerY = height / 2;
}

window.addEventListener('resize', resize);
resize();

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
        let y = centerY + Math.sin(x * 0.01 + time * 2) * 50 * Math.sin(time * 0.5);
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

        // Connecting Circuit Lines
        ctx.beginPath();
        ctx.moveTo(centerX, centerY);
        ctx.lineTo(nx, ny);
        ctx.strokeStyle = 'rgba(192, 192, 192, 0.3)';
        ctx.lineWidth = 1;
        ctx.stroke();

        // Animated Data Flow on Lines
        const flowPos = (Math.sin(time * 2 + node.angle) + 1) / 2; // 0 to 1
        const fx = centerX + (nx - centerX) * flowPos;
        const fy = centerY + (ny - centerY) * flowPos;

        ctx.beginPath();
        ctx.arc(fx, fy, 3, 0, Math.PI * 2);
        ctx.fillStyle = node.color;
        ctx.shadowBlur = 10;
        ctx.shadowColor = node.color;
        ctx.fill();
        ctx.shadowBlur = 0;

        // Node Shape
        ctx.beginPath();
        ctx.arc(nx, ny, 15, 0, Math.PI * 2);
        ctx.fillStyle = colors.bgCore;
        ctx.fill();

        ctx.beginPath();
        ctx.arc(nx, ny, 15, 0, Math.PI * 2);
        ctx.strokeStyle = node.color;
        ctx.lineWidth = 3;
        ctx.shadowBlur = 15;
        ctx.shadowColor = node.color;
        ctx.stroke();
        ctx.shadowBlur = 0;

        // Node Label
        ctx.fillStyle = colors.silver;
        ctx.font = "12px sans-serif";
        ctx.textAlign = "center";
        ctx.fillText(node.label, nx, ny - 25);
    });
}

function drawMacroSignal() {
    // MacroSignal (MSM) orbiting ring / golden halo
    const orbitRadius = Math.min(width, height) * 0.4;

    ctx.beginPath();
    ctx.arc(centerX, centerY, orbitRadius, 0, Math.PI * 2);
    ctx.strokeStyle = 'rgba(255, 215, 0, 0.1)';
    ctx.lineWidth = 1;
    ctx.stroke();

    const msmAngle = time * 0.5;
    const mx = centerX + Math.cos(msmAngle) * orbitRadius;
    const my = centerY + Math.sin(msmAngle) * orbitRadius;

    ctx.beginPath();
    ctx.arc(mx, my, 8, 0, Math.PI * 2);
    ctx.fillStyle = colors.gold;
    ctx.shadowBlur = 20;
    ctx.shadowColor = colors.gold;
    ctx.fill();
    ctx.shadowBlur = 0;

    // Label for MSM
    ctx.fillStyle = colors.gold;
    ctx.font = "10px sans-serif";
    ctx.fillText("MacroSignal", mx + 20, my);
}

function render() {
    ctx.clearRect(0, 0, width, height);

    time += 0.02;

    drawBackground();
    drawNodes();
    drawCentralCore();
    drawMacroSignal();

    requestAnimationFrame(render);
}

// Start loop
render();
