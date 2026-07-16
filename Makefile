# ============================================================
# AILLEE Framework - Unified Makefile
# One command to build everything
# ============================================================

CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -Wpedantic -O3

# ============================================================
# Include Paths
# ============================================================
INCLUDES = -I. \
           -I./ailee_plugins \
           -I./extensions \
           -I./telemetry \
           -I./src \
           -I./examples

HTTPLIB_INCLUDES = -I./external
THREAD_FLAGS = -pthread

# ============================================================
# Core Extension Sources
# ============================================================
EXT_SRCS = extensions/aille_btc.cpp \
           extensions/aille_eth.cpp \
           extensions/aille_oil.cpp \
           extensions/aille_gold.cpp \
           extensions/aille_silver.cpp \
           extensions/aille_copper.cpp \
           extensions/aille_natgas.cpp \
           extensions/aille_platinum.cpp \
           extensions/aille_forex_usd.cpp \
           extensions/aille_macro.cpp \
           extensions/v7_2_pipeline.cpp \
           extensions/v7_3_pipeline.cpp \
           extensions/aille_spire.cpp \
           extensions/aille_lantern.cpp \
           extensions/aille_weathering.cpp

# ============================================================
# Source Files
# ============================================================
EXAMPLE_SRC      = examples/example.cpp
BENCHMARK_SRC    = benchmarks/benchmark.cpp
REST_API_SRC     = examples/rest_api_server.cpp
REST_API_IMPL    = extensions/aille_rest_api.cpp
UNIT_TESTS_SRC   = tests/unit_tests.cpp
SPIRE_DEMO_SRC   = examples/v7_4_spire_demo.cpp

# ============================================================
# Default Target
# ============================================================
.PHONY: all demo debug clean run test benchmark rest_api_server dashboard_server websocket_server \
        spire_demo lantern_demo crown_walk_demo weathering_demo pilgrimage_demo install uninstall help

all: demo

# ============================================================
# Demo Build
# ============================================================
demo: $(EXAMPLE_SRC) aille.hpp $(EXT_SRCS)
    $(CXX) $(CXXFLAGS) $(INCLUDES) $(EXAMPLE_SRC) $(EXT_SRCS) -o demo
    @echo ""
    @echo "✓ Demo compiled successfully!"
    @echo "  Run with: ./demo"
    @echo ""

# ============================================================
# Debug Build
# ============================================================
debug: $(EXAMPLE_SRC) aille.hpp $(EXT_SRCS)
    $(CXX) $(CXXFLAGS) $(INCLUDES) -g $(EXAMPLE_SRC) $(EXT_SRCS) -o demo_debug
    @echo ""
    @echo "✓ Debug build ready"
    @echo "  Run with: gdb ./demo_debug"
    @echo ""

# ============================================================
# REST API Server
# ============================================================
rest_api_server: $(REST_API_SRC) $(REST_API_IMPL) aille_framework.cpp aille_audit.cpp aille.hpp
    $(CXX) $(CXXFLAGS) $(INCLUDES) $(HTTPLIB_INCLUDES) $(THREAD_FLAGS) \
        $(REST_API_SRC) $(REST_API_IMPL) aille_framework.cpp aille_audit.cpp \
        -o rest_api_server
    @echo ""
    @echo "✓ REST API Server compiled successfully!"
    @echo "  Run with: ./rest_api_server [port] [host]"
    @echo ""

# ============================================================
# Spire Demo
# ============================================================
spire_demo: $(SPIRE_DEMO_SRC) aille.hpp $(EXT_SRCS)
    $(CXX) $(CXXFLAGS) $(INCLUDES) $(SPIRE_DEMO_SRC) $(EXT_SRCS) -o spire_demo
    @echo ""
    @echo "✓ Spire Demo compiled successfully!"
    @echo "  Run with: ./spire_demo"
    @echo ""

# ============================================================
# Lantern Demo
# ============================================================
lantern_demo: examples/v7_5_lantern_demo.cpp aille.hpp $(EXT_SRCS)
    $(CXX) $(CXXFLAGS) $(INCLUDES) examples/v7_5_lantern_demo.cpp $(EXT_SRCS) -o lantern_demo
    @echo ""
    @echo "✓ Lantern Demo compiled successfully!"
    @echo "  Run with: ./lantern_demo"
    @echo ""

# ============================================================
# Crown Walk Demo
# ============================================================
crown_walk_demo: examples/v7_6_crown_walk_demo.cpp \
                 extensions/aille_crown_walk.cpp \
                 extensions/aille_spire.cpp \
                 extensions/aille_lantern.cpp \
                 extensions/v7_3_pipeline.cpp \
                 extensions/v7_2_pipeline.cpp \
                 extensions/aille_weathering.cpp \
                 aille.hpp
    $(CXX) $(CXXFLAGS) $(INCLUDES) examples/v7_6_crown_walk_demo.cpp \
        extensions/aille_crown_walk.cpp extensions/aille_spire.cpp \
        extensions/aille_lantern.cpp extensions/v7_3_pipeline.cpp \
        extensions/v7_2_pipeline.cpp extensions/aille_weathering.cpp \
        -o crown_walk_demo
    @echo ""
    @echo "✓ Crown Walk Demo compiled successfully!"
    @echo ""

# ============================================================
# Weathering Demo
# ============================================================
weathering_demo: examples/v7_7_weathering_demo.cpp \
                 extensions/aille_weathering.cpp \
                 extensions/aille_crown_walk.cpp \
                 extensions/aille_spire.cpp \
                 extensions/aille_lantern.cpp \
                 extensions/v7_3_pipeline.cpp \
                 extensions/v7_2_pipeline.cpp \
                 aille.hpp
    $(CXX) $(CXXFLAGS) $(INCLUDES) examples/v7_7_weathering_demo.cpp \
        extensions/aille_weathering.cpp extensions/aille_crown_walk.cpp \
        extensions/aille_spire.cpp extensions/aille_lantern.cpp \
        extensions/v7_3_pipeline.cpp extensions/v7_2_pipeline.cpp \
        -o weathering_demo
    @echo ""
    @echo "✓ Weathering Demo compiled successfully!"
    @echo ""

# ============================================================
# Pilgrimage Demo
# ============================================================
pilgrimage_demo: examples/v7_8_pilgrimage_demo.cpp \
                 extensions/aille_pilgrimage.cpp \
                 extensions/aille_weathering.cpp \
                 extensions/aille_crown_walk.cpp \
                 extensions/aille_spire.cpp \
                 extensions/aille_lantern.cpp \
                 extensions/v7_3_pipeline.cpp \
                 aille.hpp
    $(CXX) $(CXXFLAGS) $(INCLUDES) examples/v7_8_pilgrimage_demo.cpp \
        extensions/aille_pilgrimage.cpp extensions/aille_weathering.cpp \
        extensions/aille_crown_walk.cpp extensions/aille_spire.cpp \
        extensions/aille_lantern.cpp extensions/v7_3_pipeline.cpp \
        -o pilgrimage_demo
    @echo ""
    @echo "✓ Pilgrimage Demo compiled successfully!"
    @echo ""

# ============================================================
# Unit Tests
# ============================================================
test_suite: $(UNIT_TESTS_SRC) aille.hpp $(EXT_SRCS)
    $(CXX) $(CXXFLAGS) $(INCLUDES) $(UNIT_TESTS_SRC) $(EXT_SRCS) -o test_suite
    @echo ""
    @echo "✓ Test Suite compiled successfully!"
    @echo ""

# ============================================================
# Integration Test
# ============================================================
test: demo test_suite
    @echo "Running integration test..."
    ./demo > test_output.txt
    @if grep -q "PASSED" test_output.txt; then \
        echo "✓ Demo integration passed"; \
    else \
        echo "✗ Demo integration failed"; \
        exit 1; \
    fi
    @rm test_output.txt
    @echo "Running unit tests..."
    ./test_suite

# ============================================================
# Benchmark
# ============================================================
benchmark: $(BENCHMARK_SRC) aille.hpp $(EXT_SRCS)
    $(CXX) $(CXXFLAGS) $(INCLUDES) $(BENCHMARK_SRC) $(EXT_SRCS) -o benchmark
    @echo ""
    @echo "✓ Benchmark compiled successfully!"
    @echo ""

# ============================================================
# Dashboard Server
# ============================================================
dashboard_server: examples/dashboard_server.cpp ailee_plugins/plugins/dashboard/LiveAdvisoryObserver.cpp aille.hpp $(EXT_SRCS)
    $(CXX) $(CXXFLAGS) $(INCLUDES) examples/dashboard_server.cpp \
        ailee_plugins/plugins/dashboard/LiveAdvisoryObserver.cpp \
        $(EXT_SRCS) -o dashboard_server
    @echo ""
    @echo "✓ Dashboard Server compiled successfully!"
    @echo ""

# ============================================================
# WebSocket Server
# ============================================================
websocket_server: examples/websocket_server.cpp extensions/aille_websocket.cpp extensions/aille_websocket.hpp aille.hpp $(EXT_SRCS)
    $(CXX) $(CXXFLAGS) $(INCLUDES) examples/websocket_server.cpp \
        extensions/aille_websocket.cpp $(EXT_SRCS) -o websocket_server
    @echo ""
    @echo "✓ WebSocket Server compiled successfully!"
    @echo ""

# ============================================================
# Clean
# ============================================================
clean:
    rm -f websocket_server demo demo_debug demo_hotpath demo_audit.csv benchmark \
          rest_api_server rest_api_audit.csv test_suite test_audit.csv \
          test_integrity.csv dashboard_server spire_demo lantern_demo \
          crown_walk_demo weathering_demo pilgrimage_demo
    @echo "✓ Cleaned build artifacts"

# ============================================================
# Install Header
# ============================================================
install: aille.hpp
    @echo "Installing AILLE header..."
    sudo cp aille.hpp /usr/local/include/
    @echo "✓ Header installed to /usr/local/include/aille.hpp"

# ============================================================
# Uninstall Header
# ============================================================
uninstall:
    sudo rm -f /usr/local/include/aille.hpp
    @echo "✓ AILLE header removed"

# ============================================================
# Help
# ============================================================
help:
    @echo "AILLE Framework - Build Targets"
    @echo "================================"
    @echo ""
    @echo "  make                 - Build demo"
    @echo "  make debug           - Debug build"
    @echo "  make run             - Run demo"
    @echo "  make test            - Integration + unit tests"
    @echo "  make benchmark       - Benchmark harness"
    @echo "  make rest_api_server - REST API server"
    @echo "  make websocket_server- WebSocket server"
    @echo "  make dashboard_server- Advisory dashboard"
    @echo "  make spire_demo      - Spire v7.4 demo"
    @echo "  make lantern_demo    - Lantern v7.5 demo"
    @echo "  make crown_walk_demo - Crown Walk v7.6 demo"
    @echo "  make weathering_demo - Weathering v7.7 demo"
    @echo "  make pilgrimage_demo - Pilgrimage v7.8 demo"
    @echo "  make clean           - Remove build artifacts"
    @echo "  make install         - Install header"
    @echo "  make help            - Show this message"
