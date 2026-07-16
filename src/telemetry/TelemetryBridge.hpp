/*
 * AILLEE Telemetry Bridge — Terminal Dashboard Integration
 * AI-Load Integrity and Layered Evaluation
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: MIT
 */

#pragma once

#include <atomic>
#include <cstdint>

struct DashboardStats {
    // Core activity
    std::atomic<uint64_t> total_queries{0};
    std::atomic<uint64_t> hft_decisions{0};

    // Trading + execution
    std::atomic<uint64_t> executions_sent{0};
    std::atomic<uint64_t> alerts_triggered{0};

    // Market + news
    std::atomic<uint64_t> market_updates{0};
    std::atomic<uint64_t> breaking_news_events{0};

    // Plugins
    std::atomic<uint64_t> active_plugins{0};
};

// Single global instance (you can wrap this later if you prefer)
inline DashboardStats g_dashboard_stats;

// Convenience functions to be called from interfaces

inline void telemetry_on_query() {
    g_dashboard_stats.total_queries.fetch_add(1);
}

inline void telemetry_on_hft_decision() {
    g_dashboard_stats.hft_decisions.fetch_add(1);
}

inline void telemetry_on_execution_sent() {
    g_dashboard_stats.executions_sent.fetch_add(1);
}

inline void telemetry_on_alert_triggered() {
    g_dashboard_stats.alerts_triggered.fetch_add(1);
}

inline void telemetry_on_market_update() {
    g_dashboard_stats.market_updates.fetch_add(1);
}

inline void telemetry_on_breaking_news() {
    g_dashboard_stats.breaking_news_events.fetch_add(1);
}

inline void telemetry_on_plugin_registered() {
    g_dashboard_stats.active_plugins.fetch_add(1);
}
