/*
 * AILEE Framework - FS-Gateway Networking Module (Layer 18 Live Data Layer)
 * AILEE Finance Unified Runtime Version 19.0.0
 *
 * Exposes WebSocket endpoint: ws://<host>:9002/ailee/finance/runtime
 * Streams deterministic AF-WSX JSON messages for runtime, governance,
 * pipeline, asset, wnfs, and trading desk modules.
 *
 * Copyright (c) Don Michael Feeney Jr.
 * Licensed under the MIT License.
 */

#ifndef AILEE_FS_GATEWAY_HPP
#define AILEE_FS_GATEWAY_HPP

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <set>
#include <chrono>
#include <memory>
#include <cstdint>

namespace websocketpp {
    class server_base;
    namespace config {
        struct asio;
    }
    template <typename config>
    class server;
}

namespace AILEE {

constexpr const char* FS_GATEWAY_VERSION = "20.0.0";
constexpr const char* FS_GATEWAY_DEFAULT_PATH = "/ailee/finance/runtime";
constexpr int FS_GATEWAY_DEFAULT_PORT = 9002;

struct TradingDeskState {
    const char* desk_id;
    const char* asset_class;
    float buy_pressure;
    float sell_pressure;
    float decision_intensity;
    uint32_t active_orders;
    uint32_t risk_level;
    const char* execution_readiness;
    const char* order_intent;
    const char* desk_state;
    float recon_threshold;
    float liquidity_depth_m;
    float volatility_pressure;
    bool anomaly_detected;
};

class FsGateway {
public:
    explicit FsGateway(int port = FS_GATEWAY_DEFAULT_PORT, const std::string& path = FS_GATEWAY_DEFAULT_PATH);
    ~FsGateway();

    // Start server async background threads
    bool startAsync();

    // Stop server
    void stop();

    // Join background threads
    void join();

    // Query status
    bool isRunning() const { return running_; }
    int getPort() const { return port_; }
    std::string getPath() const { return path_; }

    // Execute 1 cycle frame broadcast
    void broadcastCycleFrames();

private:
    void run();
    void broadcastLoop();

    // Deterministic JSON frame builders per module
    std::string formatRFC3339(std::uint64_t timestamp_ns) const;
    std::string buildRuntimeJSON(std::uint64_t cycle_id, std::uint64_t timestamp_ns) const;
    std::string buildGovernanceJSON(std::uint64_t cycle_id, std::uint64_t timestamp_ns) const;
    std::string buildPipelineJSON(std::uint64_t cycle_id, std::uint64_t timestamp_ns) const;
    std::string buildAssetJSON(std::uint64_t cycle_id, std::uint64_t timestamp_ns) const;
    std::string buildWNFSJSON(std::uint64_t cycle_id, std::uint64_t timestamp_ns) const;
    std::string buildDeskJSON(std::uint64_t cycle_id, std::uint64_t timestamp_ns) const;

    int port_;
    std::string path_;
    std::atomic<bool> running_;
    std::thread server_thread_;
    std::thread broadcast_thread_;

    // Pimpl opaque pointer to hide ASIO/WebSocketPP implementation types
    void* server_ptr_;
    mutable std::atomic<std::uint64_t> sequence_counter_{0};
};

} // namespace AILEE

#endif // AILEE_FS_GATEWAY_HPP
