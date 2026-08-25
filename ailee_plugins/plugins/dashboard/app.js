/**
 * AILEE FINANCE V18 — UNIFIED RUNTIME INSTITUTIONAL TERMINAL LOGIC
 * Core Application Logic & Cross-Evaluation Engine
 */

(function () {
    'use strict';

    // GLOBAL TERMINAL STATE
    const state = {
        runtimeMode: 'NOMINAL_EXECUTION', // NOMINAL_EXECUTION | STRESS_OVERRIDE | META_LOCKED
        failClosedStatus: 0, // 0 = OK, 1 = TRIGGERED
        metaGovernanceLocked: 0, // 0 = UNLOCKED, 1 = LOCKED
        cycleSequenceId: 324902,
        throughputOpsSec: 1592400,
        p50LatencyNs: 84.69,
        p99LatencyNs: 890.12,
        p999LatencyNs: 1420.50,
        allocationScale: 1.000,
        activeLayerMask: 0x7ffff, // 19 layers active (bits 0..18 set)

        // Filters
        activeAssetClassFilter: 'ALL',
        activeLiquidityTierFilter: 'ALL',
        activeVolatilityBandFilter: 'ALL',
        activeGovernanceFlagFilter: 'ALL',
        searchTerm: '',

        // Tab selection
        activeDevTab: 'sandbox',
        activeVisTab: 'latency',

        // WebSocket Stream
        wsConnected: false,
        wsUrl: 'ws://localhost:8081/stream',
        wsSocket: null,
        autoRunLoop: false,
        autoRunInterval: null,

        // Fault Injection State
        faults: {
            wnfsGap: false,
            crisisOverride: false,
            priceShockPct: 0.0,
            liquidityDegraded: false
        },

        // Assets Inventory across classes
        assets: [
            // EQUITIES
            { symbol: 'SPY', class: 'EQUITIES', price: 512.40, vol: 12.4, depth: 420.5, trust: 'PASS (98.4%)', residual: 0.012, trigger: 'NOMINAL', tier: 'TIER_1', scale: 1.0 },
            { symbol: 'QQQ', class: 'EQUITIES', price: 438.10, vol: 16.2, depth: 310.2, trust: 'PASS (97.1%)', residual: 0.015, trigger: 'NOMINAL', tier: 'TIER_1', scale: 1.0 },
            { symbol: 'NVDA', class: 'EQUITIES', price: 875.20, vol: 34.8, depth: 180.4, trust: 'PASS (95.0%)', residual: 0.021, trigger: 'NOMINAL', tier: 'TIER_1', scale: 1.0 },
            { symbol: 'AAPL', class: 'EQUITIES', price: 172.50, vol: 14.1, depth: 290.0, trust: 'PASS (99.0%)', residual: 0.008, trigger: 'NOMINAL', tier: 'TIER_1', scale: 1.0 },

            // FX
            { symbol: 'EUR/USD', class: 'FX', price: 1.0850, vol: 6.2, depth: 850.0, trust: 'PASS (99.5%)', residual: 0.004, trigger: 'NOMINAL', tier: 'TIER_1', scale: 1.0 },
            { symbol: 'USD/JPY', class: 'FX', price: 151.20, vol: 8.5, depth: 720.0, trust: 'PASS (98.9%)', residual: 0.006, trigger: 'NOMINAL', tier: 'TIER_1', scale: 1.0 },
            { symbol: 'GBP/USD', class: 'FX', price: 1.2640, vol: 7.8, depth: 540.0, trust: 'PASS (98.2%)', residual: 0.009, trigger: 'NOMINAL', tier: 'TIER_1', scale: 1.0 },

            // CRYPTO
            { symbol: 'BTC/USD', class: 'CRYPTO', price: 67450.00, vol: 48.2, depth: 95.4, trust: 'PASS (92.1%)', residual: 0.028, trigger: 'NOMINAL', tier: 'TIER_2', scale: 1.0 },
            { symbol: 'ETH/USD', class: 'CRYPTO', price: 3520.00, vol: 52.4, depth: 68.2, trust: 'PASS (91.0%)', residual: 0.031, trigger: 'NOMINAL', tier: 'TIER_2', scale: 1.0 },
            { symbol: 'SOL/USD', class: 'CRYPTO', price: 185.30, vol: 68.0, depth: 28.5, trust: 'WARN (84.5%)', residual: 0.042, trigger: 'ADVISORY', tier: 'TIER_2', scale: 0.8 },

            // COMMODITIES
            { symbol: 'XAU/USD', class: 'COMMODITIES', price: 2165.40, vol: 11.2, depth: 380.0, trust: 'PASS (99.1%)', residual: 0.007, trigger: 'NOMINAL', tier: 'TIER_1', scale: 1.0 },
            { symbol: 'XAG/USD', class: 'COMMODITIES', price: 24.80, vol: 22.1, depth: 140.0, trust: 'PASS (96.5%)', residual: 0.018, trigger: 'NOMINAL', tier: 'TIER_1', scale: 1.0 },
            { symbol: 'CL_OIL', class: 'COMMODITIES', price: 81.20, vol: 28.4, depth: 210.0, trust: 'PASS (95.8%)', residual: 0.019, trigger: 'NOMINAL', tier: 'TIER_1', scale: 1.0 },
            { symbol: 'NG_GAS', class: 'COMMODITIES', price: 1.82, vol: 45.0, depth: 42.0, trust: 'PASS (91.4%)', residual: 0.034, trigger: 'ADVISORY', tier: 'TIER_2', scale: 0.85 },

            // DERIVATIVES
            { symbol: 'ES_FUT', class: 'DERIVATIVES', price: 5180.25, vol: 13.0, depth: 620.0, trust: 'PASS (99.4%)', residual: 0.005, trigger: 'NOMINAL', tier: 'TIER_1', scale: 1.0 },
            { symbol: 'NQ_FUT', class: 'DERIVATIVES', price: 18240.50, vol: 17.5, depth: 480.0, trust: 'PASS (98.0%)', residual: 0.011, trigger: 'NOMINAL', tier: 'TIER_1', scale: 1.0 },
            { symbol: 'SPX_OPT_C5200', class: 'DERIVATIVES', price: 24.50, vol: 38.0, depth: 15.2, trust: 'PASS (93.2%)', residual: 0.025, trigger: 'NOMINAL', tier: 'TIER_3', scale: 1.0 },

            // SYNTHETICS
            { symbol: 'SYNTH_AI_BASKET', class: 'SYNTHETICS', price: 1045.80, vol: 32.0, depth: 85.0, trust: 'PASS (94.0%)', residual: 0.022, trigger: 'NOMINAL', tier: 'TIER_2', scale: 1.0 },
            { symbol: 'SYNTH_USD_CRYPTO', class: 'SYNTHETICS', price: 98.40, vol: 26.5, depth: 110.0, trust: 'PASS (96.1%)', residual: 0.016, trigger: 'NOMINAL', tier: 'TIER_2', scale: 1.0 }
        ],

        // 19 Architecture Layers
        layers: [
            { num: 1, name: 'L1: Hardware Ingress Parsing', status: 'ACTIVE', state: 'OK' },
            { num: 2, name: 'L2: WNFS Lock-Free Ingestion', status: 'ACTIVE', state: 'OK' },
            { num: 3, name: 'L3: Stream Gap & Corruption', status: 'ACTIVE', state: 'OK' },
            { num: 4, name: 'L4: Multi-Clone Wave Consensus', status: 'ACTIVE', state: 'OK' },
            { num: 5, name: 'L5: HF-AT Delta-V Impulse Engine', status: 'ACTIVE', state: 'OK' },
            { num: 6, name: 'L6: Controlled Bullish Bias', status: 'ACTIVE', state: 'OK' },
            { num: 7, name: 'L7: Intraday Volume Advisory', status: 'ACTIVE', state: 'OK' },
            { num: 8, name: 'L8: Cross-Asset Arbitration', status: 'ACTIVE', state: 'OK' },
            { num: 9, name: 'L9: Deterministic Routing', status: 'ACTIVE', state: 'OK' },
            { num: 10, name: 'L10: Multi-Gov Reconciliation', status: 'ACTIVE', state: 'OK' },
            { num: 11, name: 'L11: Portfolio-Wide Constraints', status: 'ACTIVE', state: 'OK' },
            { num: 12, name: 'L12: Temporal Consistency Guard', status: 'ACTIVE', state: 'OK' },
            { num: 13, name: 'L13: Stress-Regime Override', status: 'ACTIVE', state: 'OK' },
            { num: 14, name: 'L14: Meta-Governance Lock', status: 'ACTIVE', state: 'OK' },
            { num: 15, name: 'L15: Deformable Membrane', status: 'ACTIVE', state: 'OK' },
            { num: 16, name: 'L16: Anomaly Detection Layer', status: 'ACTIVE', state: 'OK' },
            { num: 17, name: 'L17: Real-Time Chart Intel', status: 'ACTIVE', state: 'OK' },
            { num: 18, name: 'L18: Spire Integration Interface', status: 'ACTIVE', state: 'OK' },
            { num: 19, name: 'L19: Unified Master Orchestrator', status: 'ACTIVE', state: 'OK' }
        ],

        // ABI Struct Definitions for Inspector
        abiStructs: {
            UnifiedRuntimeState: `// C++ Struct Definition (alignas(64))
struct alignas(64) UnifiedRuntimeState {
    uint64_t cycle_sequence_id;       // Offset 0x00 (8 bytes)
    uint64_t timestamp_ns;           // Offset 0x08 (8 bytes)
    uint32_t active_layer_mask;      // Offset 0x10 (4 bytes)
    float    p50_latency_ns;          // Offset 0x14 (4 bytes)
    float    p99_latency_ns;          // Offset 0x18 (4 bytes)
    float    p99_9_latency_ns;        // Offset 0x1C (4 bytes)
    float    allocation_scale;        // Offset 0x20 (4 bytes)
    uint32_t fail_closed_status;     // Offset 0x24 (4 bytes)
    uint32_t meta_governance_locked; // Offset 0x28 (4 bytes)
    uint8_t  padding[20];            // Offset 0x2C (20 bytes padding to 64B)
};
static_assert(sizeof(UnifiedRuntimeState) == 64, "UnifiedRuntimeState must be exactly 64 bytes");`,

            MetaGovernanceState: `// C++ Struct Definition (alignas(64))
struct alignas(64) MetaGovernanceState {
    uint64_t cycle_sequence_id;       // Offset 0x00 (8 bytes)
    float    residual_sum;            // Offset 0x08 (4 bytes) - Threshold: 0.05
    float    temporal_consistency_score; // Offset 0x0C (4 bytes)
    uint32_t remaining_violations;    // Offset 0x10 (4 bytes)
    uint32_t lock_state;              // Offset 0x14 (4 bytes) - 1 = LOCKED
    uint32_t governor_mask;           // Offset 0x18 (4 bytes)
    float    confidence_residual;     // Offset 0x1C (4 bytes)
    uint8_t  padding[32];            // Offset 0x20 (32 bytes padding to 64B)
};
static_assert(sizeof(MetaGovernanceState) == 64, "MetaGovernanceState must be exactly 64 bytes");`,

            WNFSFrame: `// C++ Struct Definition (alignas(64))
struct alignas(64) WNFSFrame {
    uint64_t frame_sequence_id;       // Offset 0x00 (8 bytes)
    uint64_t timestamp_ns;           // Offset 0x08 (8 bytes)
    float    bid_price;               // Offset 0x10 (4 bytes)
    float    ask_price;               // Offset 0x14 (4 bytes)
    float    volume;                  // Offset 0x18 (4 bytes)
    uint32_t channel_id;              // Offset 0x1C (4 bytes)
    uint32_t clone_status_mask;      // Offset 0x20 (4 bytes)
    uint32_t gap_detected;            // Offset 0x24 (4 bytes)
    uint8_t  padding[24];            // Offset 0x28 (24 bytes padding to 64B)
};
static_assert(sizeof(WNFSFrame) == 64, "WNFSFrame must be exactly 64 bytes");`,

            VolumeState: `// C++ Struct Definition (alignas(64))
struct alignas(64) VolumeState {
    float    smoothed_volume;         // Offset 0x00 (4 bytes)
    float    vwap_deviation;          // Offset 0x04 (4 bytes)
    float    volume_anomaly_ratio;    // Offset 0x08 (4 bytes)
    float    oversold_score;          // Offset 0x0C (4 bytes)
    float    delta_v_impulse;         // Offset 0x10 (4 bytes)
    uint32_t contrarian_signal;       // Offset 0x14 (4 bytes)
    float    bullish_weight_scale;    // Offset 0x18 (4 bytes)
    uint8_t  padding[36];            // Offset 0x1C (36 bytes padding to 64B)
};
static_assert(sizeof(VolumeState) == 64, "VolumeState must be exactly 64 bytes");`,

            StressOverrideRules: `// C++ Struct Definition (alignas(64))
struct alignas(64) StressOverrideRules {
    float    volatility_threshold;    // Offset 0x00 (4 bytes)
    float    drawdown_threshold;      // Offset 0x04 (4 bytes)
    float    crash_dampening_scale;   // Offset 0x08 (4 bytes)
    uint32_t exposure_freeze_flag;    // Offset 0x0C (4 bytes)
    uint32_t fallback_compression;    // Offset 0x10 (4 bytes)
    uint8_t  padding[44];            // Offset 0x14 (44 bytes padding to 64B)
};
static_assert(sizeof(StressOverrideRules) == 64, "StressOverrideRules must be exactly 64 bytes");`
        }
    };

    // DOM ELEMENTS
    const dom = {
        runtimeModeBadge: document.getElementById('runtimeModeBadge'),
        failClosedBadge: document.getElementById('failClosedBadge'),
        metaGovBadge: document.getElementById('metaGovBadge'),
        headerLatencyVal: document.getElementById('headerLatencyVal'),
        headerThroughputVal: document.getElementById('headerThroughputVal'),
        headerAllocVal: document.getElementById('headerAllocVal'),
        systemClock: document.getElementById('systemClock'),
        streamToggleBtn: document.getElementById('streamToggleBtn'),

        // Filters
        assetClassFilterGroup: document.getElementById('assetClassFilterGroup'),
        assetSearchInput: document.getElementById('assetSearchInput'),
        liquidityTierFilter: document.getElementById('liquidityTierFilter'),
        volatilityBandFilter: document.getElementById('volatilityBandFilter'),
        governanceFlagFilter: document.getElementById('governanceFlagFilter'),
        assetMatrixBody: document.getElementById('assetMatrixBody'),

        // Dev Lab
        tabButtons: document.querySelectorAll('.panel-tabs .tab-btn[data-tab]'),
        tabContents: document.querySelectorAll('.tab-content'),
        abiStructSelect: document.getElementById('abiStructSelect'),
        abiStructDefinition: document.getElementById('abiStructDefinition'),
        runtimeCallPreview: document.getElementById('runtimeCallPreview'),
        deterministicTraceLog: document.getElementById('deterministicTraceLog'),
        btnClearTrace: document.getElementById('btnClearTrace'),

        // Sandbox Buttons
        btnInjectSequenceGap: document.getElementById('btnInjectSequenceGap'),
        btnInjectStressOverride: document.getElementById('btnInjectStressOverride'),
        btnInjectPriceShockUp: document.getElementById('btnInjectPriceShockUp'),
        btnInjectPriceShockDown: document.getElementById('btnInjectPriceShockDown'),
        btnInjectLiquidityDrop: document.getElementById('btnInjectLiquidityDrop'),
        btnResetNominal: document.getElementById('btnResetNominal'),
        btnRunSingleCycle: document.getElementById('btnRunSingleCycle'),
        btnRunBatchCycles: document.getElementById('btnRunBatchCycles'),
        btnToggleLoop: document.getElementById('btnToggleLoop'),

        // Visual Tabs
        visTabButtons: document.querySelectorAll('.panel-tabs .tab-btn[data-vistab]'),
        visCanvases: document.querySelectorAll('.vis-canvas'),

        // Layer Grid & Health
        layerStackGrid: document.getElementById('layerStackGrid'),
        layerActiveCount: document.getElementById('layerActiveCount'),
        wnfsHealthVal: document.getElementById('wnfsHealthVal'),
        wnfsHealthSub: document.getElementById('wnfsHealthSub'),
        stressStateVal: document.getElementById('stressStateVal'),
        stressStateSub: document.getElementById('stressStateSub'),
        metaLockVal: document.getElementById('metaLockVal'),
        metaLockSub: document.getElementById('metaLockSub'),
        hftEngineVal: document.getElementById('hftEngineVal'),
        hftEngineSub: document.getElementById('hftEngineSub'),

        // Footer Ticker
        tickerTrack: document.getElementById('tickerTrack'),
        wsServerStatus: document.getElementById('wsServerStatus')
    };

    // SYSTEM CLOCK
    function updateClock() {
        const now = new Date();
        const iso = now.toISOString().replace('T', ' ').replace('Z', ' UTC');
        if (dom.systemClock) dom.systemClock.textContent = iso.substring(11);
    }

    // TRACE LOGGING
    function logTrace(msg, type = 'info') {
        if (!dom.deterministicTraceLog) return;
        const line = document.createElement('div');
        line.className = `trace-line ${type}`;
        const ts = new Date().toISOString().substring(11, 23);
        line.textContent = `[${ts}] [CYC:${state.cycleSequenceId}] ${msg}`;
        dom.deterministicTraceLog.appendChild(line);
        dom.deterministicTraceLog.scrollTop = dom.deterministicTraceLog.scrollHeight;
    }

    // EVALUATE ENGINE CYCLE (DETERMINISTIC SIMULATION STEP)
    function stepExecutionCycle(count = 1) {
        for (let c = 0; c < count; c++) {
            state.cycleSequenceId++;

            // Tick asset prices with deterministic jitter
            state.assets.forEach(a => {
                let jitter = (Math.random() - 0.48) * (a.vol / 100.0) * (a.price * 0.01);
                if (state.faults.priceShockPct !== 0.0) {
                    a.price *= (1.0 + state.faults.priceShockPct);
                }
                a.price += jitter;
                if (a.price <= 0.01) a.price = 0.01;

                if (state.faults.liquidityDegraded) {
                    a.depth = Math.max(1.0, a.depth * 0.7);
                }
            });

            // Reset one-time price shocks
            state.faults.priceShockPct = 0.0;

            // Recalculate Runtime State
            if (state.faults.wnfsGap || state.faults.crisisOverride) {
                state.runtimeMode = 'STRESS_OVERRIDE';
                state.allocationScale = 0.10; // Forced dampening

                if (state.faults.crisisOverride) {
                    state.metaGovernanceLocked = 1;
                    state.runtimeMode = 'META_LOCKED';
                    state.allocationScale = 0.00; // Hard execution freeze
                }
            } else {
                state.runtimeMode = 'NOMINAL_EXECUTION';
                state.metaGovernanceLocked = 0;
                state.allocationScale = 1.000;
            }

            // Latency Jitter
            if (state.runtimeMode === 'NOMINAL_EXECUTION') {
                state.p50LatencyNs = 80.0 + Math.random() * 8.0;
                state.p99LatencyNs = 850.0 + Math.random() * 80.0;
                state.throughputOpsSec = 1580000 + Math.floor(Math.random() * 30000);
            } else {
                state.p50LatencyNs = 140.0 + Math.random() * 20.0;
                state.p99LatencyNs = 1450.0 + Math.random() * 120.0;
                state.throughputOpsSec = 950000 + Math.floor(Math.random() * 20000);
            }
        }

        // Render Updates
        renderHeader();
        renderAssetMatrix();
        renderLayerStack();
        renderSubsystemHealth();
        renderRuntimeCallPreview();
        renderTicker();

        // Canvas update trigger
        if (window.AILEEV17_WebGL && window.AILEEV17_WebGL.updateData) {
            window.AILEEV17_WebGL.updateData(state);
        }
    }

    // RENDER HEADER
    function renderHeader() {
        if (state.runtimeMode === 'NOMINAL_EXECUTION') {
            dom.runtimeModeBadge.className = 'badge badge-nominal';
            dom.runtimeModeBadge.textContent = 'NOMINAL_EXECUTION';
            dom.failClosedBadge.className = 'badge badge-ok';
            dom.failClosedBadge.textContent = 'FAIL_CLOSED_READY';
            dom.metaGovBadge.className = 'badge badge-ok';
            dom.metaGovBadge.textContent = 'EXECUTION_READY';
        } else if (state.runtimeMode === 'STRESS_OVERRIDE') {
            dom.runtimeModeBadge.className = 'badge badge-warn';
            dom.runtimeModeBadge.textContent = 'STRESS_OVERRIDE';
            dom.failClosedBadge.className = 'badge badge-warn';
            dom.failClosedBadge.textContent = 'EXPOSURE_FROZEN';
            dom.metaGovBadge.className = 'badge badge-warn';
            dom.metaGovBadge.textContent = 'RESIDUAL_ELEVATED';
        } else if (state.runtimeMode === 'META_LOCKED') {
            dom.runtimeModeBadge.className = 'badge badge-stress';
            dom.runtimeModeBadge.textContent = 'META_LOCKED';
            dom.failClosedBadge.className = 'badge badge-stress';
            dom.failClosedBadge.textContent = 'FAIL_CLOSED_ACTIVE';
            dom.metaGovBadge.className = 'badge badge-stress';
            dom.metaGovBadge.textContent = 'META_LOCK_ENGAGED';
        }

        dom.headerLatencyVal.textContent = `${state.p50LatencyNs.toFixed(1)} ns / ${state.p99LatencyNs.toFixed(0)} ns`;
        dom.headerThroughputVal.textContent = `${state.throughputOpsSec.toLocaleString()} ops/s`;
        dom.headerAllocVal.textContent = `${state.allocationScale.toFixed(3)}x`;
    }

    // RENDER CROSS-ASSET EVALUATION MATRIX
    function renderAssetMatrix() {
        if (!dom.assetMatrixBody) return;

        let filtered = state.assets.filter(a => {
            if (state.activeAssetClassFilter !== 'ALL' && a.class !== state.activeAssetClassFilter) return false;
            if (state.activeLiquidityTierFilter !== 'ALL' && a.tier !== state.activeLiquidityTierFilter) return false;
            if (state.activeVolatilityBandFilter !== 'ALL') {
                if (state.activeVolatilityBandFilter === 'LOW' && a.vol >= 15) return false;
                if (state.activeVolatilityBandFilter === 'MEDIUM' && (a.vol < 15 || a.vol > 40)) return false;
                if (state.activeVolatilityBandFilter === 'HIGH' && a.vol <= 40) return false;
            }
            if (state.activeGovernanceFlagFilter !== 'ALL' && a.trigger !== state.activeGovernanceFlagFilter) return false;
            if (state.searchTerm) {
                let term = state.searchTerm.toLowerCase();
                if (!a.symbol.toLowerCase().includes(term) && !a.class.toLowerCase().includes(term)) return false;
            }
            return true;
        });

        dom.assetMatrixBody.innerHTML = filtered.map(a => {
            let triggerBadge = 'badge-nominal';
            if (a.trigger === 'ADVISORY') triggerBadge = 'badge-advisory';
            if (a.trigger === 'STRESS') triggerBadge = 'badge-stress';

            return `
                <tr>
                    <td class="sym-col">${a.symbol}</td>
                    <td class="class-col">${a.class}</td>
                    <td>$${a.price.toFixed(a.price < 10 ? 4 : 2)}</td>
                    <td>${a.vol.toFixed(1)}%</td>
                    <td>$${a.depth.toFixed(1)}M</td>
                    <td><span class="badge badge-ok" style="font-size:8px;">${a.trust}</span></td>
                    <td>${a.residual.toFixed(3)}</td>
                    <td><span class="badge ${triggerBadge}">${a.trigger}</span></td>
                    <td>
                        <button class="btn btn-xs btn-primary tune-btn" data-sym="${a.symbol}">TUNE (${a.scale.toFixed(2)}x)</button>
                    </td>
                </tr>
            `;
        }).join('');

        // Attach tune buttons
        document.querySelectorAll('.tune-btn').forEach(btn => {
            btn.addEventListener('click', (e) => {
                let sym = e.target.getAttribute('data-sym');
                let asset = state.assets.find(x => x.symbol === sym);
                if (asset) {
                    asset.scale = asset.scale === 1.0 ? 0.5 : (asset.scale === 0.5 ? 0.0 : 1.0);
                    logTrace(`Tuned scale factor for asset ${sym} to ${asset.scale.toFixed(2)}x`, 'warn');
                    renderAssetMatrix();
                }
            });
        });
    }

    // RENDER 19-LAYER GOVERNANCE STATE STACK
    function renderLayerStack() {
        if (!dom.layerStackGrid) return;

        dom.layerStackGrid.innerHTML = state.layers.map(l => {
            let cardClass = 'layer-card active';
            let dotClass = 'layer-status-dot';
            let statusText = 'NOMINAL';

            if (state.runtimeMode === 'STRESS_OVERRIDE') {
                if (l.num >= 13) {
                    cardClass = 'layer-card stress';
                    dotClass = 'layer-status-dot warn';
                    statusText = 'OVERRIDE';
                }
            } else if (state.runtimeMode === 'META_LOCKED') {
                if (l.num >= 13) {
                    cardClass = 'layer-card meta-locked';
                    dotClass = 'layer-status-dot locked';
                    statusText = 'META_LOCKED';
                }
            }

            return `
                <div class="${cardClass}">
                    <div style="display:flex; justify-content:space-between; align-items:center;">
                        <span class="layer-num">L${l.num}</span>
                        <span class="${dotClass}"></span>
                    </div>
                    <div class="layer-name" title="${l.name}">${l.name}</div>
                    <div style="font-size:8px; color:var(--text-muted); font-family:var(--font-mono); display:flex; justify-content:space-between;">
                        <span>STATE:</span>
                        <strong style="color:var(--text-main);">${statusText}</strong>
                    </div>
                </div>
            `;
        }).join('');

        dom.layerActiveCount.textContent = '19 / 19 ACTIVE';
    }

    // RENDER SUBSYSTEM HEALTH
    function renderSubsystemHealth() {
        if (state.faults.wnfsGap) {
            dom.wnfsHealthVal.textContent = 'DEGRADED (GAP DETECTED)';
            dom.wnfsHealthVal.className = 'card-value status-fault';
            dom.wnfsHealthSub.textContent = 'Sequence Gap Dropped Frame #0x04f2c1';
        } else {
            dom.wnfsHealthVal.textContent = '100.0% INTEGRITY';
            dom.wnfsHealthVal.className = 'card-value status-ok';
            dom.wnfsHealthSub.textContent = '0 Gap Drops • 0 Corrupt Frames';
        }

        if (state.runtimeMode === 'STRESS_OVERRIDE') {
            dom.stressStateVal.textContent = 'STRESS OVERRIDE ENGAGED';
            dom.stressStateVal.className = 'card-value status-warn';
            dom.stressStateSub.textContent = 'Crash Dampening: 0.10x Baseline Scale';
        } else if (state.runtimeMode === 'META_LOCKED') {
            dom.stressStateVal.textContent = 'CRISIS OVERRIDE ACTIVE';
            dom.stressStateVal.className = 'card-value status-fault';
            dom.stressStateSub.textContent = 'Exposure Freeze: 0.00x Hard Cap';
        } else {
            dom.stressStateVal.textContent = 'NOMINAL';
            dom.stressStateVal.className = 'card-value status-ok';
            dom.stressStateSub.textContent = 'Crash Dampening: 1.00x Baseline';
        }

        if (state.metaGovernanceLocked) {
            dom.metaLockVal.textContent = 'LOCKED (Residual Breached)';
            dom.metaLockVal.className = 'card-value status-fault';
            dom.metaLockSub.textContent = 'Residual Sum: 0.142 > 0.050 Ceiling';
        } else {
            dom.metaLockVal.textContent = 'UNLOCKED (0.012 Sum)';
            dom.metaLockVal.className = 'card-value status-ok';
            dom.metaLockSub.textContent = 'Threshold Ceiling: 0.050';
        }
    }

    // RENDER RUNTIME CALL IN/OUT PREVIEW
    function renderRuntimeCallPreview() {
        if (!dom.runtimeCallPreview) return;
        const inputObj = {
            cycle_sequence_id: state.cycleSequenceId,
            timestamp_ns: Date.now() * 1000000,
            layer_mask: state.activeLayerMask,
            wnfs_stream: {
                ticks_ingested: state.cycleSequenceId * 4,
                gap_detected: state.faults.wnfsGap
            },
            anomaly: {
                volatility_spike: state.faults.priceShockPct !== 0.0,
                liquidity_break: state.faults.liquidityDegraded
            },
            governance: {
                residual_sum: state.metaGovernanceLocked ? 0.142 : 0.012,
                stress_level: state.runtimeMode,
                allocation_scale: state.allocationScale
            }
        };
        dom.runtimeCallPreview.textContent = `// Execution Step Input/Output Preview\n` + JSON.stringify(inputObj, null, 2);
    }

    // RENDER BOTTOM TICKER
    function renderTicker() {
        if (!dom.tickerTrack) return;
        const items = state.assets.map(a => {
            const chg = (Math.random() - 0.48) * 1.5;
            const chgClass = chg >= 0 ? 'chg' : 'chg down';
            const sign = chg >= 0 ? '+' : '';
            return `
                <div class="ticker-item">
                    <span class="sym">${a.symbol}</span>:
                    <span class="val">$${a.price.toFixed(a.price < 10 ? 4 : 2)}</span>
                    <span class="${chgClass}">(${sign}${chg.toFixed(2)}%)</span>
                </div>
            `;
        }).join('');
        dom.tickerTrack.innerHTML = items + items; // duplicate for infinite scroll
    }

    // SETUP EVENT LISTENERS
    function setupEventListeners() {
        // Asset Filters
        dom.assetClassFilterGroup.addEventListener('click', (e) => {
            if (e.target.classList.contains('filter-btn')) {
                dom.assetClassFilterGroup.querySelectorAll('.filter-btn').forEach(b => b.classList.remove('active'));
                e.target.classList.add('active');
                state.activeAssetClassFilter = e.target.getAttribute('data-class');
                renderAssetMatrix();
            }
        });

        dom.assetSearchInput.addEventListener('input', (e) => {
            state.searchTerm = e.target.value;
            renderAssetMatrix();
        });

        dom.liquidityTierFilter.addEventListener('change', (e) => {
            state.activeLiquidityTierFilter = e.target.value;
            renderAssetMatrix();
        });

        dom.volatilityBandFilter.addEventListener('change', (e) => {
            state.activeVolatilityBandFilter = e.target.value;
            renderAssetMatrix();
        });

        dom.governanceFlagFilter.addEventListener('change', (e) => {
            state.activeGovernanceFlagFilter = e.target.value;
            renderAssetMatrix();
        });

        // Tab Switching (Dev Lab)
        dom.tabButtons.forEach(btn => {
            btn.addEventListener('click', () => {
                dom.tabButtons.forEach(b => b.classList.remove('active'));
                dom.tabContents.forEach(c => c.classList.remove('active'));
                btn.classList.add('active');
                const tabId = btn.getAttribute('data-tab');
                document.getElementById(`tab-${tabId}`).classList.add('active');
            });
        });

        // ABI Inspector Selection
        if (dom.abiStructSelect) {
            dom.abiStructSelect.addEventListener('change', (e) => {
                const selected = e.target.value;
                if (state.abiStructs[selected]) {
                    dom.abiStructDefinition.textContent = state.abiStructs[selected];
                }
            });
        }

        // Trace Clear
        if (dom.btnClearTrace) {
            dom.btnClearTrace.addEventListener('click', () => {
                if (dom.deterministicTraceLog) dom.deterministicTraceLog.innerHTML = '';
            });
        }

        // Fault Injection Controls
        dom.btnInjectSequenceGap.addEventListener('click', () => {
            state.faults.wnfsGap = true;
            logTrace('FAULT INJECTED: WNFS Stream Sequence Gap Detected (#0x04f2c1)', 'error');
            stepExecutionCycle();
        });

        dom.btnInjectStressOverride.addEventListener('click', () => {
            state.faults.crisisOverride = true;
            logTrace('FAULT INJECTED: Systemic Crisis Override & Meta-Governance Lock Activated', 'error');
            stepExecutionCycle();
        });

        dom.btnInjectPriceShockUp.addEventListener('click', () => {
            state.faults.priceShockPct = 0.05;
            logTrace('PRICE SHOCK INJECTED: +5.0% Volatility Expansion Impulse', 'warn');
            stepExecutionCycle();
        });

        dom.btnInjectPriceShockDown.addEventListener('click', () => {
            state.faults.priceShockPct = -0.10;
            logTrace('PRICE SHOCK INJECTED: -10.0% Structural Liquidity Breakdown Shock', 'error');
            stepExecutionCycle();
        });

        dom.btnInjectLiquidityDrop.addEventListener('click', () => {
            state.faults.liquidityDegraded = true;
            logTrace('LIQUIDITY INJECTED: 70% Order Book Liquidity Depth Deficit', 'warn');
            stepExecutionCycle();
        });

        dom.btnResetNominal.addEventListener('click', () => {
            state.faults.wnfsGap = false;
            state.faults.crisisOverride = false;
            state.faults.priceShockPct = 0.0;
            state.faults.liquidityDegraded = false;
            logTrace('SYSTEM RESET: Restored AILEE Unified Runtime to NOMINAL_EXECUTION State', 'success');
            stepExecutionCycle();
        });

        // Cycle Execution Controls
        dom.btnRunSingleCycle.addEventListener('click', () => {
            logTrace('Single Master Execution Cycle Triggered (Layers 1..19 Evaluated)', 'info');
            stepExecutionCycle(1);
        });

        dom.btnRunBatchCycles.addEventListener('click', () => {
            logTrace('Batch Execution Triggered: 100 Deterministic Cycles Evaluated', 'info');
            stepExecutionCycle(100);
        });

        dom.btnToggleLoop.addEventListener('click', () => {
            state.autoRunLoop = !state.autoRunLoop;
            if (state.autoRunLoop) {
                dom.btnToggleLoop.textContent = 'AUTORUN (ACTIVE)';
                dom.btnToggleLoop.className = 'btn btn-danger btn-sm';
                logTrace('AUTORUN LOOP ENGAGED (60 FPS Master Engine Cycle)', 'warn');
                state.autoRunInterval = setInterval(() => stepExecutionCycle(1), 16);
            } else {
                dom.btnToggleLoop.textContent = 'AUTORUN (60 FPS)';
                dom.btnToggleLoop.className = 'btn btn-secondary btn-sm';
                logTrace('AUTORUN LOOP DISENGAGED', 'info');
                clearInterval(state.autoRunInterval);
            }
        });

        // Visual Canvas Tabs
        dom.visTabButtons.forEach(btn => {
            btn.addEventListener('click', () => {
                dom.visTabButtons.forEach(b => b.classList.remove('active'));
                dom.visCanvases.forEach(c => c.classList.remove('active'));
                btn.classList.add('active');
                const visTab = btn.getAttribute('data-vistab');
                state.activeVisTab = visTab;

                const canvasMap = {
                    latency: 'visCanvasLatency',
                    throughput: 'visCanvasThroughput',
                    anomaly: 'visCanvasAnomaly',
                    recon: 'visCanvasRecon',
                    membrane: 'visCanvasMembrane'
                };
                const targetCanvas = document.getElementById(canvasMap[visTab]);
                if (targetCanvas) targetCanvas.classList.add('active');
            });
        });
    }

    // INITIALIZATION
    function init() {
        setInterval(updateClock, 200);
        setupEventListeners();
        logTrace('AILEE Finance Unified Runtime V18 Initialized', 'success');
        logTrace('Loaded 19-Layer Governance Engine Specs & ABI Alignments', 'info');
        stepExecutionCycle();
    }

    window.addEventListener('DOMContentLoaded', init);

})();
