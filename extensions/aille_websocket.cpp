/*
 * AILLE Framework - WebSocket API Implementation
 * WebSocket server implementation for AILLE Spire interface
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: MIT (see LICENSE)
 */

#define WEBSOCKETPP_STRICT_MASKING

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include "aille_websocket.hpp"
#include "aille_spire.hpp"
#include <iostream>
#include <sstream>

typedef websocketpp::server<websocketpp::config::asio> WsServer;
typedef websocketpp::connection_hdl connection_hdl;

// We need a way to store connection_hdls in our void* set without slicing.
// A simple pointer to the hdl works, but we can also just store the weak_ptr
// inside a dynamically allocated block. To avoid allocations per connection
// as much as possible, we could use the raw pointer to the connection block
// if websocketpp allows. Actually, the easiest way is just to keep a set of
// connection_hdls locally in the .cpp file, since the server is a singleton-like
// member of the class anyway.

namespace AILLE {

// Pimpl idiom to hide websocketpp completely
struct WsServerImpl {
    WsServer server;
    std::set<connection_hdl, std::owner_less<connection_hdl>> connections;
    std::mutex mtx;
};

WebSocketServer::WebSocketServer(int port) : port_(port), running_(false), server_ptr_(new WsServerImpl()) {
    WsServerImpl* impl = static_cast<WsServerImpl*>(server_ptr_);

    impl->server.init_asio();
    impl->server.set_reuse_addr(true);

    // Disable logging to avoid spam
    impl->server.clear_access_channels(websocketpp::log::alevel::all);
    impl->server.set_access_channels(websocketpp::log::alevel::access_core);

    impl->server.set_open_handler([this](connection_hdl hdl) {
        WsServerImpl* pImpl = static_cast<WsServerImpl*>(server_ptr_);
        {
            std::lock_guard<std::mutex> lock(pImpl->mtx);
            pImpl->connections.insert(hdl);
        }
        // Send initial state immediately upon connect
        std::string payload = buildSpireJSON();
        try {
            pImpl->server.send(hdl, payload, websocketpp::frame::opcode::text);
        } catch (const websocketpp::exception& e) {
            std::cerr << "WebSocket send error on connect: " << e.what() << "\n";
        }
    });

    impl->server.set_close_handler([this](connection_hdl hdl) {
        WsServerImpl* pImpl = static_cast<WsServerImpl*>(server_ptr_);
        std::lock_guard<std::mutex> lock(pImpl->mtx);
        pImpl->connections.erase(hdl);
    });
}

WebSocketServer::~WebSocketServer() {
    stop();
    join();
    if (server_ptr_) {
        delete static_cast<WsServerImpl*>(server_ptr_);
        server_ptr_ = nullptr;
    }
}

bool WebSocketServer::startAsync() {
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

void WebSocketServer::run() {
    WsServerImpl* impl = static_cast<WsServerImpl*>(server_ptr_);
    try {
        impl->server.listen(port_);
        impl->server.start_accept();
        std::cout << "WebSocket Server listening on port " << port_ << "...\n";
        impl->server.run();
    } catch (const websocketpp::exception& e) {
        std::cerr << "WebSocket exception: " << e.what() << "\n";
        running_ = false;
    } catch (const std::exception& e) {
        std::cerr << "Standard exception: " << e.what() << "\n";
        running_ = false;
    }
}

void WebSocketServer::stop() {
    if (!running_) return;
    running_ = false;

    WsServerImpl* impl = static_cast<WsServerImpl*>(server_ptr_);
    if (impl) {
        impl->server.stop_listening();

        // Close all connections
        {
            std::lock_guard<std::mutex> lock(impl->mtx);
            for (auto it = impl->connections.begin(); it != impl->connections.end(); ++it) {
                websocketpp::lib::error_code ec;
                impl->server.close(*it, websocketpp::close::status::going_away, "Server shutting down", ec);
            }
            impl->connections.clear();
        }

        // Give connections a brief moment to transmit the close frame
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        impl->server.stop();
    }
}

void WebSocketServer::join() {
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    if (broadcast_thread_.joinable()) {
        broadcast_thread_.join();
    }
}

void WebSocketServer::broadcastLoop() {
    WsServerImpl* impl = static_cast<WsServerImpl*>(server_ptr_);
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 100ms interval

        if (!running_) break;

        std::string payload = buildSpireJSON();

        std::lock_guard<std::mutex> lock(impl->mtx);
        for (auto it = impl->connections.begin(); it != impl->connections.end(); ++it) {
            try {
                impl->server.send(*it, payload, websocketpp::frame::opcode::text);
            } catch (const websocketpp::exception&) {
                // Connection might be closing
            }
        }
    }
}

std::string WebSocketServer::buildSpireJSON() {
    // Collect all data deterministically
    auto snapshot = aillee_spire::get_snapshot();
    auto lantern = aillee_spire::get_lantern();
    auto crown_walk = aillee_spire::get_crown_walk();
    auto weathering = aillee_spire::get_weathering();
    auto pilgrimage = aillee_spire::get_pilgrimage();

    // Use stringstream to avoid massive string reallocations manually,
    // though for ultimate low-latency, a fixed char array with snprintf could be used.
    std::ostringstream json;

    uint64_t timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();

    json << "{\n";
    json << "  \"timestamp_ns\": " << timestamp << ",\n";

    // Spire Snapshot
    json << "  \"spire\": {\n";
    json << "    \"resonance_bell\": " << snapshot.resonance_bell << ",\n";
    json << "    \"sync_tick\": " << snapshot.sync_tick << ",\n";
    json << "    \"dampened_state\": " << snapshot.dampened_state << "\n";
    json << "  },\n";

    // Lantern
    json << "  \"lantern\": {\n";
    json << "    \"ssi_high\": " << lantern.ssi_high << ",\n";
    json << "    \"ssi_low\": " << lantern.ssi_low << ",\n";
    json << "    \"pulse\": " << lantern.pulse << "\n";
    json << "  },\n";

    // Crown Walk
    json << "  \"crown_walk\": {\n";
    json << "    \"foundational_stability\": " << crown_walk.foundational_stability << ",\n";
    json << "    \"secondary_stability\": " << crown_walk.secondary_stability << ",\n";
    json << "    \"secondary_intelligence\": " << crown_walk.secondary_intelligence << ",\n";
    json << "    \"resonance_bell\": " << crown_walk.resonance_bell << ",\n";
    json << "    \"spire_pulse\": " << crown_walk.spire_pulse << ",\n";
    json << "    \"lantern_pulse\": " << crown_walk.lantern_pulse << "\n";
    json << "  },\n";

    // Weathering
    json << "  \"weathering\": {\n";
    json << "    \"shock\": {\n";
    json << "      \"resonance_surge\": " << weathering.shock.resonance_surge << ",\n";
    json << "      \"sync_distortion\": " << weathering.shock.sync_distortion << ",\n";
    json << "      \"dampening_anomaly\": " << weathering.shock.dampening_anomaly << "\n";
    json << "    },\n";
    json << "    \"stress\": {\n";
    json << "      \"structural_load\": " << weathering.stress.structural_load << ",\n";
    json << "      \"volatility_factor\": " << weathering.stress.volatility_factor << ",\n";
    json << "      \"resilience_score\": " << weathering.stress.resilience_score << "\n";
    json << "    }\n";
    json << "  },\n";

    // Pilgrimage
    json << "  \"pilgrimage\": {\n";
    json << "    \"handshake\": {\n";
    json << "      \"ssi_high\": " << pilgrimage.handshake.ssi_high << ",\n";
    json << "      \"ssi_low\": " << pilgrimage.handshake.ssi_low << ",\n";
    json << "      \"pulse\": " << pilgrimage.handshake.pulse << "\n";
    json << "    },\n";
    json << "    \"sync\": {\n";
    json << "      \"resonance_alignment\": " << pilgrimage.sync.resonance_alignment << ",\n";
    json << "      \"sync_alignment\": " << pilgrimage.sync.sync_alignment << ",\n";
    json << "      \"dampening_alignment\": " << pilgrimage.sync.dampening_alignment << "\n";
    json << "    }\n";
    json << "  }\n";

    json << "}";

    return json.str();
}

} // namespace AILLE
