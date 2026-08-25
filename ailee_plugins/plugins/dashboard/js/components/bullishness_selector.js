/**
 * AILEE FINANCE UNIFIED RUNTIME — BULLISHNESS MODE SELECTOR COMPONENT
 * Purely UI/UX Presentational Layer
 *
 * MODES:
 *  - STANDARD (Default, lens off, neutral visual presentation)
 *  - CONSERVATIVE (Soft green highlights, upward vol/liquidity depth, trust-gated signals)
 *  - HYPER (Vibrant green overlays, momentum corridors, high animation intensity, fast-path bullish windows)
 *
 * SAFETY INVARIANT:
 *  This component MUST NOT alter raw runtime objects, governance state, or execution logic.
 *  All transformations are strictly presentational overlays applied prior to DOM/Canvas rendering.
 */

(function () {
    'use strict';

    const STORAGE_KEY = 'ailee_bullishness_mode';

    const MODES = {
        STANDARD: 'STANDARD',
        CONSERVATIVE: 'CONSERVATIVE',
        HYPER: 'HYPER'
    };

    class BullishnessSelector {
        constructor() {
            this.activeMode = this.loadPersistedMode();
            this.listeners = [];
        }

        loadPersistedMode() {
            try {
                const saved = localStorage.getItem(STORAGE_KEY);
                if (saved && MODES[saved]) {
                    return saved;
                }
            } catch (e) {
                console.warn('[BullishnessSelector] Could not read localStorage:', e);
            }
            return MODES.STANDARD;
        }

        setMode(mode) {
            if (!MODES[mode]) return;
            this.activeMode = mode;
            try {
                localStorage.setItem(STORAGE_KEY, mode);
            } catch (e) {
                console.warn('[BullishnessSelector] Could not save to localStorage:', e);
            }
            this.notifyListeners(mode);
        }

        getMode() {
            return this.activeMode;
        }

        onModeChange(callback) {
            if (typeof callback === 'function') {
                this.listeners.push(callback);
            }
        }

        notifyListeners(mode) {
            this.listeners.forEach(fn => {
                try {
                    fn(mode);
                } catch (err) {
                    console.error('[BullishnessSelector] Listener error:', err);
                }
            });
        }

        /**
         * Returns presentational parameters derived from current mode.
         */
        getVisualConfig() {
            switch (this.activeMode) {
                case MODES.CONSERVATIVE:
                    return {
                        mode: MODES.CONSERVATIVE,
                        badgeText: 'CONSERVATIVE BULLISH',
                        badgeClass: 'badge-bullish-conservative',
                        matrixOverlayClass: 'bullish-matrix-conservative',
                        upwardVolMultiplier: 1.08,
                        liquidityEmphasis: 1.10,
                        glowIntensity: 0.4,
                        accentColor: '#10b981', // emerald green
                        fastPathWindowColor: 'rgba(16, 185, 129, 0.25)',
                        upwardMomentumLayers: [5, 6, 7], // L5 HF-AT, L6 Bullish Bias, L7 VAM
                        deskBuyGlow: '0 0 10px rgba(16, 185, 129, 0.3)'
                    };

                case MODES.HYPER:
                    return {
                        mode: MODES.HYPER,
                        badgeText: 'HYPER BULLISH MODE',
                        badgeClass: 'badge-bullish-hyper',
                        matrixOverlayClass: 'bullish-matrix-hyper',
                        upwardVolMultiplier: 1.25,
                        liquidityEmphasis: 1.30,
                        glowIntensity: 0.9,
                        accentColor: '#00ff88', // neon electric green
                        fastPathWindowColor: 'rgba(0, 255, 136, 0.45)',
                        upwardMomentumLayers: [2, 5, 6, 7, 8, 17], // Ingestion, HF-AT, Bias, VAM, Arbit, Chart Intel
                        deskBuyGlow: '0 0 18px rgba(0, 255, 136, 0.65)'
                    };

                case MODES.STANDARD:
                default:
                    return {
                        mode: MODES.STANDARD,
                        badgeText: 'STANDARD LENS',
                        badgeClass: 'badge-bullish-standard',
                        matrixOverlayClass: '',
                        upwardVolMultiplier: 1.0,
                        liquidityEmphasis: 1.0,
                        glowIntensity: 0.0,
                        accentColor: '#00e5ff', // cyan default
                        fastPathWindowColor: 'rgba(0, 229, 255, 0.15)',
                        upwardMomentumLayers: [],
                        deskBuyGlow: 'none'
                    };
            }
        }

        /**
         * UI-side non-mutating transformation wrapper for asset evaluation matrix items.
         * Wraps item rendering properties for analysis display without touching raw model objects.
         */
        transformAssetForDisplay(asset) {
            const cfg = this.getVisualConfig();
            if (cfg.mode === MODES.STANDARD) {
                return { ...asset, displayVol: asset.vol, displayDepth: asset.depth, rowClass: '' };
            }

            // Determine if asset exhibits positive/upward characteristics
            const isTrustPassed = asset.trust && asset.trust.includes('PASS');
            const isNominal = asset.trigger === 'NOMINAL';
            const isPositiveTrend = isTrustPassed && isNominal;

            let rowClass = '';
            let displayVol = asset.vol;
            let displayDepth = asset.depth;

            if (isPositiveTrend) {
                if (cfg.mode === MODES.CONSERVATIVE) {
                    rowClass = 'row-bullish-conservative';
                    displayVol = asset.vol * cfg.upwardVolMultiplier;
                    displayDepth = asset.depth * cfg.liquidityEmphasis;
                } else if (cfg.mode === MODES.HYPER) {
                    rowClass = 'row-bullish-hyper momentum-corridor';
                    displayVol = asset.vol * cfg.upwardVolMultiplier;
                    displayDepth = asset.depth * cfg.liquidityEmphasis;
                }
            } else if (cfg.mode === MODES.CONSERVATIVE && asset.trigger !== 'NOMINAL') {
                rowClass = 'row-softened-bearish';
            }

            return {
                ...asset,
                displayVol,
                displayDepth,
                rowClass
            };
        }

        /**
         * UI-side non-mutating transformation wrapper for trading desk execution items.
         */
        transformDeskForDisplay(desk) {
            const cfg = this.getVisualConfig();
            if (cfg.mode === MODES.STANDARD) {
                return { ...desk, displayBuyPressure: desk.buy_pressure, displayIntensity: desk.decision_intensity, deskGlowStyle: '' };
            }

            let displayBuyPressure = desk.buy_pressure;
            let displayIntensity = desk.decision_intensity;
            let deskGlowStyle = '';

            if (desk.buy_pressure > desk.sell_pressure) {
                if (cfg.mode === MODES.CONSERVATIVE) {
                    displayBuyPressure = Math.min(1.0, desk.buy_pressure * 1.10);
                    displayIntensity = Math.min(1.0, desk.decision_intensity * 1.12);
                    deskGlowStyle = `box-shadow: ${cfg.deskBuyGlow}; border-left: 3px solid #10b981;`;
                } else if (cfg.mode === MODES.HYPER) {
                    displayBuyPressure = Math.min(1.0, desk.buy_pressure * 1.25);
                    displayIntensity = Math.min(1.0, desk.decision_intensity * 1.35);
                    deskGlowStyle = `box-shadow: ${cfg.deskBuyGlow}; border-left: 4px solid #00ff88; animation: pulseGlow 1.5s infinite alternate;`;
                }
            }

            return {
                ...desk,
                displayBuyPressure,
                displayIntensity,
                deskGlowStyle
            };
        }
    }

    // Attach singleton to window scope
    window.bullishnessSelector = new BullishnessSelector();
})();
