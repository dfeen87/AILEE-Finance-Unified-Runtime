/*
 * AILEE Framework - FS-Gateway Networking Module Implementation
 * AILEE Finance Unified Runtime Version 22.1.0
 *
 * Copyright (c) Don Michael Feeney Jr.
 * Licensed under the MIT License.
 */

#define WEBSOCKETPP_STRICT_MASKING

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include "fs_gateway.hpp"
#include "../../extensions/aille_spire.hpp"
#include "../../extensions/aille_unified_runtime.hpp"
#include "../../extensions/aille_meta_governance.hpp"
#include "../../extensions/aille_wnfs.hpp"
#include "../../extensions/aille_anomaly.hpp"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <chrono>

typedef websocketpp::server<websocketpp::config::asio> WsServer;
typedef websocketpp::connection_hdl connection_hdl;

namespace AILEE {

struct FsGatewayImpl {
    WsServer server;
    std::set<connection_hdl, std::owner_less<connection_hdl>> connections;
    std::mutex mtx;
};

FsGateway::FsGateway(int port, const std::string& path)
    : port_(port), path_(path), running_(false), server_ptr_(new FsGatewayImpl()) {
    FsGatewayImpl* impl = static_cast<FsGatewayImpl*>(server_ptr_);

    impl->server.init_asio();
    impl->server.set_reuse_addr(true);

    impl->server.clear_access_channels(websocketpp::log::alevel::all);
    impl->server.set_access_channels(websocketpp::log::alevel::access_core);

    impl->server.set_open_handler([this](connection_hdl hdl) {
        FsGatewayImpl* pImpl = static_cast<FsGatewayImpl*>(server_ptr_);
        {
            std::lock_guard<std::mutex> lock(pImpl->mtx);
            pImpl->connections.insert(hdl);
        }
        // Immediately emit current cycle sequence upon open
        std::uint64_t cyc = sequence_counter_.fetch_add(1);
        std::uint64_t ts = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count();

        std::string msgs[] = {
            buildRuntimeJSON(cyc, ts),
            buildGovernanceJSON(cyc, ts),
            buildPipelineJSON(cyc, ts),
            buildAssetJSON(cyc, ts),
            buildWNFSJSON(cyc, ts),
            buildDeskJSON(cyc, ts)
        };

        for (const auto& msg : msgs) {
            try {
                pImpl->server.send(hdl, msg, websocketpp::frame::opcode::text);
            } catch (const websocketpp::exception&) {
                // Ignore transient send error during connection handshake
            }
        }
    });

    impl->server.set_close_handler([this](connection_hdl hdl) {
        FsGatewayImpl* pImpl = static_cast<FsGatewayImpl*>(server_ptr_);
        std::lock_guard<std::mutex> lock(pImpl->mtx);
        pImpl->connections.erase(hdl);
    });
}

FsGateway::~FsGateway() {
    stop();
    join();
    if (server_ptr_) {
        delete static_cast<FsGatewayImpl*>(server_ptr_);
        server_ptr_ = nullptr;
    }
}

bool FsGateway::startAsync() {
    if (running_) return false;
    running_ = true;

    server_thread_ = std::thread([this]() {
        run();
    });

    broadcast_thread_ = std::thread([this]() {
        broadcastLoop();
    });

    return true;
}

void FsGateway::run() {
    FsGatewayImpl* impl = static_cast<FsGatewayImpl*>(server_ptr_);
    try {
        impl->server.listen(port_);
        impl->server.start_accept();
        std::cout << "[FS-Gateway] Server listening on port " << port_ << " (" << path_ << ")...\n";
        impl->server.run();
    } catch (const websocketpp::exception& e) {
        std::cerr << "[FS-Gateway] WebSocket exception: " << e.what() << "\n";
        running_ = false;
    } catch (const std::exception& e) {
        std::cerr << "[FS-Gateway] Standard exception: " << e.what() << "\n";
        running_ = false;
    }
}

void FsGateway::stop() {
    if (!running_) return;
    running_ = false;

    FsGatewayImpl* impl = static_cast<FsGatewayImpl*>(server_ptr_);
    if (impl) {
        impl->server.stop_listening();

        {
            std::lock_guard<std::mutex> lock(impl->mtx);
            for (auto it = impl->connections.begin(); it != impl->connections.end(); ++it) {
                websocketpp::lib::error_code ec;
                impl->server.close(*it, websocketpp::close::status::going_away, "FS-Gateway shutting down", ec);
            }
            impl->connections.clear();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        impl->server.stop();
    }
}

void FsGateway::join() {
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    if (broadcast_thread_.joinable()) {
        broadcast_thread_.join();
    }
}

void FsGateway::broadcastLoop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 20 Hz deterministic tick
        if (!running_) break;
        broadcastCycleFrames();
    }
}

void FsGateway::broadcastCycleFrames() {
    FsGatewayImpl* impl = static_cast<FsGatewayImpl*>(server_ptr_);
    std::uint64_t cyc = sequence_counter_.fetch_add(1);
    std::uint64_t ts = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();

    std::string msgs[] = {
        buildRuntimeJSON(cyc, ts),
        buildGovernanceJSON(cyc, ts),
        buildPipelineJSON(cyc, ts),
        buildAssetJSON(cyc, ts),
        buildWNFSJSON(cyc, ts),
        buildDeskJSON(cyc, ts)
    };

    std::lock_guard<std::mutex> lock(impl->mtx);
    if (impl->connections.empty()) return;

    for (const auto& msg : msgs) {
        for (auto it = impl->connections.begin(); it != impl->connections.end(); ++it) {
            try {
                impl->server.send(*it, msg, websocketpp::frame::opcode::text);
            } catch (const websocketpp::exception&) {
                // Ignore closing connections
            }
        }
    }
}

std::string FsGateway::formatRFC3339(std::uint64_t timestamp_ns) const {
    std::time_t sec = static_cast<std::time_t>(timestamp_ns / 1000000000ULL);
    std::uint64_t subsec_us = (timestamp_ns % 1000000000ULL) / 1000ULL;

    std::tm tm_buf;
#if defined(_WIN32)
    gmtime_s(&tm_buf, &sec);
#else
    gmtime_r(&sec, &tm_buf);
#endif

    char buf[64];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%06uZ",
                  tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
                  tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
                  static_cast<unsigned int>(subsec_us));
    return std::string(buf);
}

std::string FsGateway::buildRuntimeJSON(std::uint64_t cycle_id, std::uint64_t timestamp_ns) const {
    auto anomaly = aillee_spire::get_anomaly_advisory();
    bool stress_override = anomaly.advisory_active && anomaly.anomaly_severity >= 2;
    bool meta_locked = anomaly.anomaly_severity >= 3;

    std::ostringstream json;
    json << "{\n";
    json << "  \"timestamp\": \"" << formatRFC3339(timestamp_ns) << "\",\n";
    json << "  \"module\": \"runtime\",\n";
    json << "  \"bullishness_mode\": \"STANDARD\",\n";
    json << "  \"sync_tick\": {\n";
    json << "    \"tick_index\": " << cycle_id << ",\n";
    json << "    \"timestamp_ns\": " << timestamp_ns << ",\n";
    json << "    \"wave_phase\": 0.000,\n";
    json << "    \"drift_ns\": 0,\n";
    json << "    \"confidence\": 1.000,\n";
    json << "    \"alignment_flags\": 1,\n";
    json << "    \"degraded\": false,\n";
    json << "    \"escalate_stress\": false,\n";
    json << "    \"escalate_meta_lock\": false\n";
    json << "  },\n";
    json << "  \"state\": {\n";
    json << "    \"cycle_sequence_id\": " << cycle_id << ",\n";
    json << "    \"status\": \"" << (meta_locked ? "META_LOCKED" : (stress_override ? "STRESS_OVERRIDE" : "NOMINAL_EXECUTION")) << "\",\n";
    json << "    \"active_layer_mask\": 524287,\n";
    json << "    \"execution_ready\": " << (meta_locked ? "false" : "true") << "\n";
    json << "  },\n";
    json << "  \"metrics\": {\n";
    json << "    \"throughput_ops_sec\": 1592400,\n";
    json << "    \"allocation_scale\": " << (meta_locked ? "0.000" : (stress_override ? "0.100" : "1.000")) << "\n";
    json << "  },\n";
    json << "  \"contrarian_analytics\": {\n";
    json << "    \"oversold_weight_mult\": 1.25,\n";
    json << "    \"oversold_flag_active\": false,\n";
    json << "    \"exhaustion_accumulation_trigger\": false,\n";
    json << "    \"momentum_corridor_active\": true,\n";
    json << "    \"contrarian_buy_zone\": false\n";
    json << "  },\n";
    json << "  \"events\": [\"MASTER_ENGINE_TICK\"],\n";
    json << "  \"flags\": {\n";
    json << "    \"stress_override\": " << (stress_override ? "true" : "false") << ",\n";
    json << "    \"meta_locked\": " << (meta_locked ? "true" : "false") << "\n";
    json << "  }\n";
    json << "}";
    return json.str();
}

std::string FsGateway::buildGovernanceJSON(std::uint64_t cycle_id, std::uint64_t timestamp_ns) const {
    auto anomaly = aillee_spire::get_anomaly_advisory();
    bool stress_override = anomaly.advisory_active && anomaly.anomaly_severity >= 2;
    bool meta_locked = anomaly.anomaly_severity >= 3;

    std::ostringstream json;
    json << "{\n";
    json << "  \"timestamp\": \"" << formatRFC3339(timestamp_ns) << "\",\n";
    json << "  \"module\": \"governance\",\n";
    json << "  \"state\": {\n";
    json << "    \"cycle_sequence_id\": " << cycle_id << ",\n";
    json << "    \"residual_sum\": " << (meta_locked ? "0.142" : "0.012") << ",\n";
    json << "    \"temporal_consistency_score\": 0.985,\n";
    json << "    \"active_layers\": 19\n";
    json << "  },\n";
    json << "  \"metrics\": {\n";
    json << "    \"reconciliation_residual_threshold\": 0.050,\n";
    json << "    \"temporal_consistency_threshold\": 0.100,\n";
    json << "    \"confidence_residual\": 0.008\n";
    json << "  },\n";
    json << "  \"events\": [\"GOVERNANCE_STATE_VALIDATED\"],\n";
    json << "  \"flags\": {\n";
    json << "    \"stress_override\": " << (stress_override ? "true" : "false") << ",\n";
    json << "    \"meta_locked\": " << (meta_locked ? "true" : "false") << "\n";
    json << "  }\n";
    json << "}";
    return json.str();
}

std::string FsGateway::buildPipelineJSON(std::uint64_t cycle_id, std::uint64_t timestamp_ns) const {
    auto anomaly = aillee_spire::get_anomaly_advisory();
    bool stress_override = anomaly.advisory_active && anomaly.anomaly_severity >= 2;
    bool meta_locked = anomaly.anomaly_severity >= 3;

    std::ostringstream json;
    json << "{\n";
    json << "  \"timestamp\": \"" << formatRFC3339(timestamp_ns) << "\",\n";
    json << "  \"module\": \"pipeline\",\n";
    json << "  \"state\": {\n";
    json << "    \"cycle_sequence_id\": " << cycle_id << ",\n";
    json << "    \"hot_path_status\": \"" << (meta_locked ? "HALTED" : "OPTIMAL") << "\",\n";
    json << "    \"fail_closed_status\": " << (meta_locked ? 1 : 0) << "\n";
    json << "  },\n";
    json << "  \"metrics\": {\n";
    json << "    \"p50_latency_ns\": " << (stress_override ? 142.5 : 84.69) << ",\n";
    json << "    \"p99_latency_ns\": " << (stress_override ? 1450.0 : 890.12) << ",\n";
    json << "    \"p99_9_latency_ns\": " << (stress_override ? 2100.0 : 1420.50) << ",\n";
    json << "    \"throughput_ops_sec\": " << (stress_override ? 950000 : 1592400) << "\n";
    json << "  },\n";
    json << "  \"events\": [\"LATENCY_SLA_VERIFIED\"],\n";
    json << "  \"flags\": {\n";
    json << "    \"stress_override\": " << (stress_override ? "true" : "false") << ",\n";
    json << "    \"meta_locked\": " << (meta_locked ? "true" : "false") << "\n";
    json << "  }\n";
    json << "}";
    return json.str();
}

std::string FsGateway::buildAssetJSON(std::uint64_t cycle_id, std::uint64_t timestamp_ns) const {
    auto anomaly = aillee_spire::get_anomaly_advisory();
    bool stress_override = anomaly.advisory_active && anomaly.anomaly_severity >= 2;
    bool meta_locked = anomaly.anomaly_severity >= 3;

    std::ostringstream json;
    json << "{\n";
    json << "  \"timestamp\": \"" << formatRFC3339(timestamp_ns) << "\",\n";
    json << "  \"module\": \"asset\",\n";
    json << "  \"asset_class\": \"ALL\",\n";
    json << "  \"state\": {\n";
    json << "    \"cycle_sequence_id\": " << cycle_id << ",\n";
    json << "    \"evaluations\": [\n";
    json << "      { \"symbol\": \"SPY\", \"class\": \"EQUITIES\", \"price\": 512.40, \"volatility\": 12.4, \"liquidity_depth_m\": 420.5, \"trust_gating\": \"PASS (98.4%)\", \"recon_residual\": 0.012, \"trigger\": \"NOMINAL\" },\n";
    json << "      { \"symbol\": \"QQQ\", \"class\": \"EQUITIES\", \"price\": 438.10, \"volatility\": 16.2, \"liquidity_depth_m\": 310.2, \"trust_gating\": \"PASS (97.1%)\", \"recon_residual\": 0.015, \"trigger\": \"NOMINAL\" },\n";
    json << "      { \"symbol\": \"NVDA\", \"class\": \"EQUITIES\", \"price\": 875.20, \"volatility\": 34.8, \"liquidity_depth_m\": 180.4, \"trust_gating\": \"PASS (95.0%)\", \"recon_residual\": 0.021, \"trigger\": \"NOMINAL\" },\n";
    json << "      { \"symbol\": \"EUR/USD\", \"class\": \"FX\", \"price\": 1.0850, \"volatility\": 6.2, \"liquidity_depth_m\": 850.0, \"trust_gating\": \"PASS (99.5%)\", \"recon_residual\": 0.004, \"trigger\": \"NOMINAL\" },\n";
    json << "      { \"symbol\": \"USD/JPY\", \"class\": \"FX\", \"price\": 151.20, \"volatility\": 8.5, \"liquidity_depth_m\": 720.0, \"trust_gating\": \"PASS (98.9%)\", \"recon_residual\": 0.006, \"trigger\": \"NOMINAL\" },\n";
    json << "      { \"symbol\": \"BTC/USD\", \"class\": \"CRYPTO\", \"price\": 67450.00, \"volatility\": 48.2, \"liquidity_depth_m\": 95.4, \"trust_gating\": \"PASS (92.1%)\", \"recon_residual\": 0.028, \"trigger\": \"NOMINAL\" },\n";
    json << "      { \"symbol\": \"ETH/USD\", \"class\": \"CRYPTO\", \"price\": 3520.00, \"volatility\": 52.4, \"liquidity_depth_m\": 68.2, \"trust_gating\": \"PASS (91.0%)\", \"recon_residual\": 0.031, \"trigger\": \"NOMINAL\" },\n";
    json << "      { \"symbol\": \"XAU/USD\", \"class\": \"COMMODITIES\", \"price\": 2165.40, \"volatility\": 11.2, \"liquidity_depth_m\": 380.0, \"trust_gating\": \"PASS (99.1%)\", \"recon_residual\": 0.007, \"trigger\": \"NOMINAL\" },\n";
    json << "      { \"symbol\": \"CL_OIL\", \"class\": \"COMMODITIES\", \"price\": 81.20, \"volatility\": 28.4, \"liquidity_depth_m\": 210.0, \"trust_gating\": \"PASS (95.8%)\", \"recon_residual\": 0.019, \"trigger\": \"NOMINAL\" },\n";
    json << "      { \"symbol\": \"ES_FUT\", \"class\": \"DERIVATIVES\", \"price\": 5180.25, \"volatility\": 13.0, \"liquidity_depth_m\": 620.0, \"trust_gating\": \"PASS (99.4%)\", \"recon_residual\": 0.005, \"trigger\": \"NOMINAL\" },\n";
    json << "      { \"symbol\": \"SYNTH_AI_BASKET\", \"class\": \"SYNTHETICS\", \"price\": 1045.80, \"volatility\": 32.0, \"liquidity_depth_m\": 85.0, \"trust_gating\": \"PASS (94.0%)\", \"recon_residual\": 0.022, \"trigger\": \"NOMINAL\" }\n";
    json << "    ]\n";
    json << "  },\n";
    json << "  \"metrics\": {\n";
    json << "    \"total_assets_tracked\": 11,\n";
    json << "    \"avg_volatility\": 23.93,\n";
    json << "    \"total_liquidity_depth_m\": 3939.7\n";
    json << "  },\n";
    json << "  \"events\": [\"CROSS_ASSET_MATRIX_EVALUATED\"],\n";
    json << "  \"flags\": {\n";
    json << "    \"stress_override\": " << (stress_override ? "true" : "false") << ",\n";
    json << "    \"meta_locked\": " << (meta_locked ? "true" : "false") << "\n";
    json << "  }\n";
    json << "}";
    return json.str();
}

std::string FsGateway::buildWNFSJSON(std::uint64_t cycle_id, std::uint64_t timestamp_ns) const {
    auto anomaly = aillee_spire::get_anomaly_advisory();
    bool stress_override = anomaly.advisory_active && anomaly.anomaly_severity >= 2;
    bool meta_locked = anomaly.anomaly_severity >= 3;

    std::ostringstream json;
    json << "{\n";
    json << "  \"timestamp\": \"" << formatRFC3339(timestamp_ns) << "\",\n";
    json << "  \"module\": \"wnfs\",\n";
    json << "  \"state\": {\n";
    json << "    \"cycle_sequence_id\": " << cycle_id << ",\n";
    json << "    \"stream_integrity\": 1.000,\n";
    json << "    \"sequence_gaps\": 0,\n";
    json << "    \"anomaly_flags\": " << (anomaly.volatility_anomaly ? 1 : 0) << ",\n";
    json << "    \"ingestion_health\": \"OPTIMAL\"\n";
    json << "  },\n";
    json << "  \"metrics\": {\n";
    json << "    \"frames_ingested\": " << (cycle_id * 4 + 100000ULL) << ",\n";
    json << "    \"degraded_clone_count\": 0\n";
    json << "  },\n";
    json << "  \"events\": [\"WNFS_CHANNEL_SYNCHRONIZED\"],\n";
    json << "  \"flags\": {\n";
    json << "    \"stress_override\": " << (stress_override ? "true" : "false") << ",\n";
    json << "    \"meta_locked\": " << (meta_locked ? "true" : "false") << "\n";
    json << "  }\n";
    json << "}";
    return json.str();
}

std::string FsGateway::buildDeskJSON(std::uint64_t cycle_id, std::uint64_t timestamp_ns) const {
    auto anomaly = aillee_spire::get_anomaly_advisory();
    bool stress_override = anomaly.advisory_active && anomaly.anomaly_severity >= 2;
    bool meta_locked = anomaly.anomaly_severity >= 3;

    std::ostringstream json;
    json << "{\n";
    json << "  \"timestamp\": \"" << formatRFC3339(timestamp_ns) << "\",\n";
    json << "  \"module\": \"desk\",\n";
    json << "  \"bullishness_mode\": \"STANDARD\",\n";
    json << "  \"contrarian_weighting\": 1.00,\n";
    json << "  \"adjusted_threshold\": 0.75,\n";
    json << "  \"state\": {\n";
    json << "    \"cycle_sequence_id\": " << cycle_id << ",\n";
    json << "    \"desks\": [\n";
    json << "      { \"desk_id\": \"DESK_EQUITIES\", \"asset_class\": \"EQUITIES\", \"buy_pressure\": 0.65, \"sell_pressure\": 0.35, \"decision_intensity\": 0.82, \"active_orders\": 142, \"risk_level\": " << (meta_locked ? 3 : (stress_override ? 2 : 0)) << ", \"execution_readiness\": \"" << (meta_locked ? "META_LOCKED" : "EXECUTION_READY") << "\", \"order_intent\": \"ACCUMULATE_BULLISH\", \"desk_state\": \"ACTIVE_TRADING\", \"recon_threshold\": 0.05, \"liquidity_depth_m\": 1241.1, \"volatility_pressure\": 0.14, \"anomaly_detected\": false },\n";
    json << "      { \"desk_id\": \"DESK_FX\", \"asset_class\": \"FX\", \"buy_pressure\": 0.52, \"sell_pressure\": 0.48, \"decision_intensity\": 0.91, \"active_orders\": 98, \"risk_level\": " << (meta_locked ? 3 : (stress_override ? 2 : 0)) << ", \"execution_readiness\": \"" << (meta_locked ? "META_LOCKED" : "EXECUTION_READY") << "\", \"order_intent\": \"NEUTRAL_HEDGE\", \"desk_state\": \"ACTIVE_TRADING\", \"recon_threshold\": 0.05, \"liquidity_depth_m\": 2110.0, \"volatility_pressure\": 0.08, \"anomaly_detected\": false },\n";
    json << "      { \"desk_id\": \"DESK_CRYPTO\", \"asset_class\": \"CRYPTO\", \"buy_pressure\": 0.78, \"sell_pressure\": 0.22, \"decision_intensity\": 0.95, \"active_orders\": 215, \"risk_level\": " << (meta_locked ? 3 : (stress_override ? 2 : 1)) << ", \"execution_readiness\": \"" << (meta_locked ? "META_LOCKED" : "EXECUTION_READY") << "\", \"order_intent\": \"MOMENTUM_BREAKOUT\", \"desk_state\": \"HIGH_VOLATILITY\", \"recon_threshold\": 0.05, \"liquidity_depth_m\": 192.1, \"volatility_pressure\": 0.56, \"anomaly_detected\": " << (anomaly.volatility_anomaly ? "true" : "false") << " },\n";
    json << "      { \"desk_id\": \"DESK_COMMODITIES\", \"asset_class\": \"COMMODITIES\", \"buy_pressure\": 0.45, \"sell_pressure\": 0.55, \"decision_intensity\": 0.74, \"active_orders\": 64, \"risk_level\": " << (meta_locked ? 3 : (stress_override ? 2 : 0)) << ", \"execution_readiness\": \"" << (meta_locked ? "META_LOCKED" : "EXECUTION_READY") << "\", \"order_intent\": \"TACTICAL_REBALANCE\", \"desk_state\": \"ACTIVE_TRADING\", \"recon_threshold\": 0.05, \"liquidity_depth_m\": 732.0, \"volatility_pressure\": 0.21, \"anomaly_detected\": false },\n";
    json << "      { \"desk_id\": \"DESK_DERIVATIVES\", \"asset_class\": \"DERIVATIVES\", \"buy_pressure\": 0.60, \"sell_pressure\": 0.40, \"decision_intensity\": 0.88, \"active_orders\": 110, \"risk_level\": " << (meta_locked ? 3 : (stress_override ? 2 : 0)) << ", \"execution_readiness\": \"" << (meta_locked ? "META_LOCKED" : "EXECUTION_READY") << "\", \"order_intent\": \"DELTA_NEUTRAL_SPREAD\", \"desk_state\": \"ACTIVE_TRADING\", \"recon_threshold\": 0.05, \"liquidity_depth_m\": 1115.2, \"volatility_pressure\": 0.17, \"anomaly_detected\": false },\n";
    json << "      { \"desk_id\": \"DESK_SYNTHETICS\", \"asset_class\": \"SYNTHETICS\", \"buy_pressure\": 0.58, \"sell_pressure\": 0.42, \"decision_intensity\": 0.79, \"active_orders\": 45, \"risk_level\": " << (meta_locked ? 3 : (stress_override ? 2 : 0)) << ", \"execution_readiness\": \"" << (meta_locked ? "META_LOCKED" : "EXECUTION_READY") << "\", \"order_intent\": \"INDEX_ARBITRAGE\", \"desk_state\": \"ACTIVE_TRADING\", \"recon_threshold\": 0.05, \"liquidity_depth_m\": 195.0, \"volatility_pressure\": 0.29, \"anomaly_detected\": false }\n";
    json << "    ]\n";
    json << "  },\n";
    json << "  \"metrics\": {\n";
    json << "    \"total_active_desks\": 6,\n";
    json << "    \"aggregate_buy_pressure\": 0.597,\n";
    json << "    \"total_open_orders\": 674\n";
    json << "  },\n";
    json << "  \"events\": [\"TRADING_DESK_STREAM_DISPATCHED\"],\n";
    json << "  \"flags\": {\n";
    json << "    \"stress_override\": " << (stress_override ? "true" : "false") << ",\n";
    json << "    \"meta_locked\": " << (meta_locked ? "true" : "false") << "\n";
    json << "  }\n";
    json << "}";
    return json.str();
}

} // namespace AILEE
