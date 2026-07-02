// WebGL Renderer for AILLEE 7.0.0
// Hardware-accelerated visualization kernel

class WebGLRenderer {
    constructor(canvas, colors, nodesData) {
        this.canvas = canvas;
        this.gl = canvas.getContext('webgl', { antialias: true, alpha: false });
        if (!this.gl) {
            console.error("WebGL not supported, falling back to Canvas 2D");
            return;
        }

        this.colors = this.parseColors(colors);
        this.nodesData = nodesData;
        this.time = 0;
        this.totalExposure = 0;
        this.macroData = { risk: 0, weight: 0 };
        this.centerX = 0;
        this.centerY = 0;

        this.initShaders();
        this.initBuffers();
        this.resize();
    }

    parseColors(colors) {
        const parsed = {};
        for (const [key, hex] of Object.entries(colors)) {
            parsed[key] = this.hexToRgb(hex);
        }
        return parsed;
    }

    hexToRgb(hex) {
        const result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
        return result ? [
            parseInt(result[1], 16) / 255,
            parseInt(result[2], 16) / 255,
            parseInt(result[3], 16) / 255,
            1.0
        ] : [0, 0, 0, 1];
    }

    initShaders() {
        const vsSource = `
            attribute vec2 aVertexPosition;
            attribute vec4 aVertexColor;
            attribute float aPointSize;

            uniform vec2 uResolution;

            varying vec4 vColor;

            void main() {
                vec2 zeroToOne = aVertexPosition / uResolution;
                vec2 zeroToTwo = zeroToOne * 2.0;
                vec2 clipSpace = zeroToTwo - 1.0;

                gl_Position = vec4(clipSpace * vec2(1, -1), 0, 1);
                gl_PointSize = aPointSize;
                vColor = aVertexColor;
            }
        `;

        const fsPointSource = `
            precision mediump float;
            varying vec4 vColor;

            void main() {
                vec2 coord = gl_PointCoord - vec2(0.5);
                if(length(coord) > 0.5) {
                    discard;
                }
                gl_FragColor = vColor;
            }
        `;

        const fsShapeSource = `
            precision mediump float;
            varying vec4 vColor;

            void main() {
                gl_FragColor = vColor;
            }
        `;

        this.pointProgram = this.createProgram(vsSource, fsPointSource);
        this.shapeProgram = this.createProgram(vsSource, fsShapeSource);

        this.pointAttribs = {
            position: this.gl.getAttribLocation(this.pointProgram, 'aVertexPosition'),
            color: this.gl.getAttribLocation(this.pointProgram, 'aVertexColor'),
            size: this.gl.getAttribLocation(this.pointProgram, 'aPointSize'),
            resolution: this.gl.getUniformLocation(this.pointProgram, 'uResolution')
        };

        this.shapeAttribs = {
            position: this.gl.getAttribLocation(this.shapeProgram, 'aVertexPosition'),
            color: this.gl.getAttribLocation(this.shapeProgram, 'aVertexColor'),
            resolution: this.gl.getUniformLocation(this.shapeProgram, 'uResolution')
        };
    }

    createProgram(vsSource, fsSource) {
        const vertexShader = this.loadShader(this.gl.VERTEX_SHADER, vsSource);
        const fragmentShader = this.loadShader(this.gl.FRAGMENT_SHADER, fsSource);

        const program = this.gl.createProgram();
        this.gl.attachShader(program, vertexShader);
        this.gl.attachShader(program, fragmentShader);
        this.gl.linkProgram(program);

        if (!this.gl.getProgramParameter(program, this.gl.LINK_STATUS)) {
            console.error('Shader init error: ' + this.gl.getProgramInfoLog(program));
            return null;
        }
        return program;
    }

    loadShader(type, source) {
        const shader = this.gl.createShader(type);
        this.gl.shaderSource(shader, source);
        this.gl.compileShader(shader);
        return shader;
    }

    initBuffers() {
        this.posBuffer = this.gl.createBuffer();
        this.colorBuffer = this.gl.createBuffer();
        this.sizeBuffer = this.gl.createBuffer();
    }

    resize() {
        this.canvas.width = window.innerWidth;
        this.canvas.height = window.innerHeight;
        this.centerX = this.canvas.width / 2;
        this.centerY = this.canvas.height / 2;
        this.gl.viewport(0, 0, this.canvas.width, this.canvas.height);
    }

    updateData(advisories, balancing) {
        if (advisories) {
            this.nodesData.forEach(node => {
                if (advisories[node.id]) {
                    node.risk = advisories[node.id].risk_score;
                    node.weight = advisories[node.id].recommended_weight;
                }
            });
            if (advisories.MacroSignal) {
                this.macroData.risk = advisories.MacroSignal.risk_score;
                this.macroData.weight = advisories.MacroSignal.recommended_weight;
            }
        }
        if (balancing) {
            this.totalExposure = balancing.total_exposure;
        }
    }

    getColorForRisk(baseColor, risk) {
        if (risk > 60) return this.colors.red;
        return baseColor;
    }

    render() {
        if (!this.gl) return;

        this.time += 0.02;

        this.gl.clearColor(this.colors.bgCore[0], this.colors.bgCore[1], this.colors.bgCore[2], 1.0);
        this.gl.clear(this.gl.COLOR_BUFFER_BIT);

        this.gl.enable(this.gl.BLEND);
        this.gl.blendFunc(this.gl.SRC_ALPHA, this.gl.ONE);

        const pointPos = [], pointCol = [], pointSize = [];
        const linePos = [], lineCol = [];
        const triPos = [], triCol = [];

        const radius = Math.min(this.canvas.width, this.canvas.height) * 0.3;

        // --- Nodes and Connections ---
        this.nodesData.forEach(node => {
            const nx = this.centerX + Math.cos(node.angle) * radius;
            const ny = this.centerY + Math.sin(node.angle) * radius;
            const nodeColor = this.getColorForRisk(this.parseColors({c: node.color}).c, node.risk);

            // Connection line
            linePos.push(this.centerX, this.centerY, nx, ny);
            lineCol.push(0.75, 0.75, 0.75, 0.3, 0.75, 0.75, 0.75, 0.3); // silver transparent

            // Draw Node Point
            pointPos.push(nx, ny);
            pointCol.push(...nodeColor);
            pointSize.push(15 + (node.weight * 5));

            // Data Flow Particle
            const speed = 2 + (node.risk / 50);
            const flowPos = (Math.sin(this.time * speed + node.angle) + 1) / 2;
            const fx = this.centerX + (nx - this.centerX) * flowPos;
            const fy = this.centerY + (ny - this.centerY) * flowPos;

            pointPos.push(fx, fy);
            pointCol.push(...nodeColor);
            pointSize.push(8);
        });

        // --- Central Core ---
        pointPos.push(this.centerX, this.centerY);
        pointCol.push(...this.colors.electricBlue);
        pointSize.push(40 + Math.sin(this.time * 4) * 5);

        // --- Macro Signal Halo (Lines) ---
        const orbitRadius = Math.min(this.canvas.width, this.canvas.height) * 0.4;
        const numSegments = 64;
        for(let i=0; i<numSegments; i++) {
            const a1 = (i / numSegments) * Math.PI * 2;
            const a2 = ((i+1) / numSegments) * Math.PI * 2;
            linePos.push(
                this.centerX + Math.cos(a1)*orbitRadius, this.centerY + Math.sin(a1)*orbitRadius,
                this.centerX + Math.cos(a2)*orbitRadius, this.centerY + Math.sin(a2)*orbitRadius
            );
            lineCol.push(1.0, 0.84, 0.0, 0.1, 1.0, 0.84, 0.0, 0.1); // Gold transparent
        }

        // --- Macro Signal Point ---
        const macroColor = this.getColorForRisk(this.colors.gold, this.macroData.risk);
        const msmAngle = this.time * (0.5 + this.macroData.risk / 100);
        const mx = this.centerX + Math.cos(msmAngle) * orbitRadius;
        const my = this.centerY + Math.sin(msmAngle) * orbitRadius;

        pointPos.push(mx, my);
        pointCol.push(...macroColor);
        pointSize.push(8 + (this.macroData.weight * 4));

        // --- Exposure Bars (Triangles) ---
        const startX = 20;
        const startY = this.canvas.height - 40 - (this.nodesData.length * 25) - 30;

        this.nodesData.forEach((node, i) => {
            const y = startY + 25 + (i * 25);
            const barWidth = 150 * node.weight;
            const barHeight = 12;
            const bx = startX + 80;
            const nodeColor = this.getColorForRisk(this.parseColors({c: node.color}).c, node.risk);

            // 2 Triangles for a rectangle. Note: in WebGL y is down in clip space, but our projection maps y to down natively? Wait.
            // My VS converts zeroToOne to clipSpace.
            // clipSpace = zeroToTwo - 1.0. then gl_Position = clipSpace * vec2(1, -1) -> this inverts Y so that 0 is top, height is bottom!
            // So drawing y and y+barHeight works intuitively like 2D canvas.
            triPos.push(
                bx, y - barHeight/2,
                bx + barWidth, y - barHeight/2,
                bx, y + barHeight/2,
                bx + barWidth, y - barHeight/2,
                bx + barWidth, y + barHeight/2,
                bx, y + barHeight/2
            );
            for(let j=0; j<6; j++) triCol.push(...nodeColor);
        });

        // 1. Draw Shapes (Triangles)
        if (triPos.length > 0) {
            this.gl.useProgram(this.shapeProgram);
            this.gl.uniform2f(this.shapeAttribs.resolution, this.canvas.width, this.canvas.height);
            this.drawArrays(this.shapeAttribs, this.gl.TRIANGLES, triPos, triCol);
        }

        // 2. Draw Lines
        if (linePos.length > 0) {
            this.gl.useProgram(this.shapeProgram);
            this.gl.uniform2f(this.shapeAttribs.resolution, this.canvas.width, this.canvas.height);
            this.drawArrays(this.shapeAttribs, this.gl.LINES, linePos, lineCol);
        }

        // 3. Draw Points
        if (pointPos.length > 0) {
            this.gl.useProgram(this.pointProgram);
            this.gl.uniform2f(this.pointAttribs.resolution, this.canvas.width, this.canvas.height);

            this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.sizeBuffer);
            this.gl.bufferData(this.gl.ARRAY_BUFFER, new Float32Array(pointSize), this.gl.DYNAMIC_DRAW);
            this.gl.enableVertexAttribArray(this.pointAttribs.size);
            this.gl.vertexAttribPointer(this.pointAttribs.size, 1, this.gl.FLOAT, false, 0, 0);

            this.drawArrays(this.pointAttribs, this.gl.POINTS, pointPos, pointCol);
        }
    }

    drawArrays(attribs, mode, posArr, colArr) {
        this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.posBuffer);
        this.gl.bufferData(this.gl.ARRAY_BUFFER, new Float32Array(posArr), this.gl.DYNAMIC_DRAW);
        this.gl.enableVertexAttribArray(attribs.position);
        this.gl.vertexAttribPointer(attribs.position, 2, this.gl.FLOAT, false, 0, 0);

        this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.colorBuffer);
        this.gl.bufferData(this.gl.ARRAY_BUFFER, new Float32Array(colArr), this.gl.DYNAMIC_DRAW);
        this.gl.enableVertexAttribArray(attribs.color);
        this.gl.vertexAttribPointer(attribs.color, 4, this.gl.FLOAT, false, 0, 0);

        this.gl.drawArrays(mode, 0, posArr.length / 2);
    }
}

window.WebGLRenderer = WebGLRenderer;
