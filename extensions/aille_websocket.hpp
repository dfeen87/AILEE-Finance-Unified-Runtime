#ifndef AILLE_WEBSOCKET_HPP
#define AILLE_WEBSOCKET_HPP

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <set>
#include <chrono>
#include <memory>

// We only forward declare the server type to avoid pulling websocketpp headers into the public API
namespace websocketpp {
    class server_base;
    namespace config {
        struct asio;
    }
    template <typename config>
    class server;
}

namespace AILLE {

// A WebSocket server that wraps Spire and broadcasts data
class WebSocketServer {
public:
    WebSocketServer(int port = 9002);
    ~WebSocketServer();

    // Start the server in a background thread
    bool startAsync();

    // Stop the server
    void stop();

    // Wait for the background thread
    void join();

    bool isRunning() const { return running_; }

private:
    void run();
    void broadcastLoop();
    std::string buildSpireJSON();

    int port_;
    std::atomic<bool> running_;
    std::thread server_thread_;
    std::thread broadcast_thread_;

    // To hide the ASIO/WebSocket++ types from the header, we use an opaque pointer
    // In C++17, void* is the safest way to completely hide dependencies without incomplete type issues on delete
    void* server_ptr_;
};

} // namespace AILLE

#endif // AILLE_WEBSOCKET_HPP
