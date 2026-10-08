// Copyright (c) Don Michael Feeney Jr.
// Licensed under the MIT License.
#include "extensions/aille_rest_api.hpp"
#include "external/httplib.h"
#include <cstdlib>
#include <iostream>

static void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "REST lifecycle regression: " << message << std::endl;
        std::_Exit(EXIT_FAILURE);
    }
}

static void healthy(int port) {
    httplib::Client client("127.0.0.1", port);
    client.set_connection_timeout(2);
    client.set_read_timeout(2);
    auto response = client.Get("/health");
    require(response && response->status == 200, "started server must serve health requests");
}

static int free_port() {
    httplib::Server probe;
    const int port = probe.bind_to_any_port("127.0.0.1");
    require(port > 0, "cannot reserve a local test port");
    std::thread listener([&]() { probe.listen_after_bind(); });
    probe.wait_until_ready();
    probe.stop();
    listener.join();
    return port;
}

static void run_case(const std::string& mode, int port) {
    AILLE::AILLEEngine engine;
    if (mode == "readiness" || mode == "restart") {
        AILLE::RestAPIServer server(engine, port);
        for (int iteration = 0; iteration < 3; ++iteration) {
            server.startAsync();
            require(server.isRunning(), "startAsync must complete startup before returning");
            healthy(port);
            if (mode == "restart") server.startAsync();
            server.stop();
            server.stop();
            server.join();
            server.join();
            require(!server.isRunning(), "stopped server must not remain running");
        }
    } else if (mode == "destructor") {
        {
            AILLE::RestAPIServer server(engine, port);
            server.startAsync();
            // Polling here isolates destructor cleanup from startup handoff.
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            while (!server.isRunning() && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            healthy(port);
        }
    } else if (mode == "collision") {
        // httplib enables SO_REUSEPORT by default on Linux, which permits two
        // httplib listeners to share a port. This fixture reserves it exclusively.
        httplib::Server owner;
        owner.set_socket_options([](socket_t) {});
        owner.Get("/health", [](const httplib::Request&, httplib::Response& response) {
            response.status = 200;
        });
        require(owner.bind_to_port("127.0.0.1", port), "cannot reserve collision port");
        std::thread owner_thread([&]() { owner.listen_after_bind(); });
        owner.wait_until_ready();
        healthy(port);
        AILLE::RestAPIServer collision(engine, port);
        collision.startAsync();
        collision.join();
        require(!collision.isRunning(), "bind failure must not publish running state");
        collision.startAsync();
        collision.join();
        collision.stop();
        healthy(port);
        owner.stop();
        owner_thread.join();
    } else if (mode == "blocking") {
        AILLE::RestAPIServer server(engine, port);
        std::thread listener([&]() { require(server.start(), "blocking start must succeed"); });
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!server.isRunning() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        healthy(port);
        server.stop();
        listener.join();
        require(!server.isRunning(), "completed blocking listener must clear running state");
    } else {
        std::cerr << "unknown lifecycle mode: " << mode << '\n';
        std::_Exit(EXIT_FAILURE);
    }
    std::cout << "REST lifecycle " << mode << " passed\n";
}

int main(int argc, char** argv) {
    if (argc == 1) {
        for (const auto* mode : {"readiness", "restart", "destructor", "collision", "blocking"}) {
            run_case(mode, free_port());
        }
    } else if (argc == 3) {
        run_case(argv[1], std::stoi(argv[2]));
    } else {
        std::cerr << "usage: rest_lifecycle [MODE PORT]\n";
        return EXIT_FAILURE;
    }
}
