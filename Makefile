# AILLE Framework - Makefile
# One command to build everything

CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -Wpedantic -O3

# Paths & Common Sources
INCLUDES = -I. \
           -I./ailee_plugins \
           -I./extensions \
           -I./telemetry \
           -I./src \
           -I./examples

HTTPLIB_INCLUDES = -I./external
THREAD_FLAGS = -pthread

# Core Extension Sources
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

# Target Source Files
REST_API_SRC = examples/rest_api_server.cpp \
               aille_framework.cpp \
               aille_audit.cpp \
               $(EXT_SRCS)

# Build REST API server
rest_api_server: $(REST_API_SRC)
    $(CXX) $(CXXFLAGS) $(INCLUDES) $(HTTPLIB_INCLUDES) $(THREAD_FLAGS) \
        $(REST_API_SRC) -o rest_api_server

# Clean
clean:
    rm -f rest_api_server

.PHONY: all demo debug clean run test benchmark rest_api_server dashboard_server install uninstall help spire_demo lantern_demo crown_walk_demo weathering_demo pilgrimage_demo websocket_server

all: demo

# Build the demo (default)
demo: $(EXAMPLE_SRC) aille.hpp $(EXT_SRCS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OPTFLAGS) $(EXAMPLE_SRC) $(EXT_SRCS) -o demo
	@echo ""
	@echo "✓ Demo compiled successfully!"
	@echo "  Run with: ./demo"
	@echo ""

# Debug build
debug: $(EXAMPLE_SRC) aille.hpp $(EXT_SRCS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEBUGFLAGS) $(EXAMPLE_SRC) $(EXT_SRCS) -o demo_debug
	@echo ""
	@echo "✓ Debug build ready"
	@echo "  Run with: gdb ./demo_debug"
	@echo ""

# Build REST API server
rest_api_server: $(REST_API_SRC) $(REST_API_IMPL) aille_framework.cpp aille_audit.cpp aille.hpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(HTTPLIB_INCLUDES) $(OPTFLAGS) $(THREAD_FLAGS) \
		$(REST_API_SRC) $(REST_API_IMPL) aille_framework.cpp aille_audit.cpp \
		-o rest_api_server
	@echo ""
	@echo "✓ REST API Server compiled successfully!"
	@echo "  Run with: ./rest_api_server [port] [host]"
	@echo "  Default:  ./rest_api_server (port 8080, host 0.0.0.0)"
	@echo ""

# Clean build artifacts
clean:
	rm -f websocket_server
	rm -f demo demo_debug demo_hotpath demo_audit.csv benchmark rest_api_server rest_api_audit.csv test_suite test_audit.csv test_integrity.csv dashboard_server spire_demo lantern_demo crown_walk_demo weathering_demo pilgrimage_demo
	@echo "✓ Cleaned build artifacts"

# Run the demo
run: demo
	./demo

# Hotpath build
hotpath: $(EXAMPLE_SRC) aille.hpp $(EXT_SRCS)
	$(CXX) -std=c++17 -Wall -Wextra $(INCLUDES) -fno-exceptions -DAILLE_HOTPATH -O3 -march=native $(EXAMPLE_SRC) $(EXT_SRCS) -o demo_hotpath
	@echo ""
	@echo "✓ Hotpath build ready"
	@echo ""

# Spire Demo target
spire_demo: $(SPIRE_DEMO_SRC) aille.hpp $(EXT_SRCS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OPTFLAGS) $(SPIRE_DEMO_SRC) $(EXT_SRCS) -o spire_demo
	@echo ""
	@echo "✓ Spire Demo compiled successfully!"
	@echo "  Run with: ./spire_demo"
	@echo ""

# Lantern Demo target
lantern_demo: examples/v7_5_lantern_demo.cpp aille.hpp $(EXT_SRCS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OPTFLAGS) examples/v7_5_lantern_demo.cpp $(EXT_SRCS) -o lantern_demo
	@echo ""
	@echo "✓ Lantern Demo compiled successfully!"
	@echo "  Run with: ./lantern_demo"
	@echo ""

# Crown Walk Demo target
crown_walk_demo: examples/v7_6_crown_walk_demo.cpp \
                 extensions/aille_crown_walk.cpp \
                 extensions/aille_spire.cpp \
                 extensions/aille_lantern.cpp \
                 extensions/v7_3_pipeline.cpp \
                 extensions/v7_2_pipeline.cpp \
                 extensions/aille_weathering.cpp \
                 aille.hpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OPTFLAGS) examples/v7_6_crown_walk_demo.cpp extensions/aille_crown_walk.cpp extensions/aille_spire.cpp extensions/aille_lantern.cpp extensions/v7_3_pipeline.cpp extensions/v7_2_pipeline.cpp extensions/aille_weathering.cpp -o crown_walk_demo
	@echo ""
	@echo "✓ Crown Walk Demo compiled successfully!"
	@echo "  Run with: ./crown_walk_demo"
	@echo ""

# Weathering Demo target
weathering_demo: examples/v7_7_weathering_demo.cpp \
                 extensions/aille_weathering.cpp \
                 extensions/aille_crown_walk.cpp \
                 extensions/aille_spire.cpp \
                 extensions/aille_lantern.cpp \
                 extensions/v7_3_pipeline.cpp \
                 extensions/v7_2_pipeline.cpp \
                 aille.hpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OPTFLAGS) examples/v7_7_weathering_demo.cpp extensions/aille_weathering.cpp extensions/aille_crown_walk.cpp extensions/aille_spire.cpp extensions/aille_lantern.cpp extensions/v7_3_pipeline.cpp extensions/v7_2_pipeline.cpp -o weathering_demo
	@echo ""
	@echo "✓ Weathering Demo compiled successfully!"
	@echo "  Run with: ./weathering_demo"
	@echo ""

# Pilgrimage Demo target
pilgrimage_demo: examples/v7_8_pilgrimage_demo.cpp \
                 extensions/aille_pilgrimage.cpp \
                 extensions/aille_weathering.cpp \
                 extensions/aille_crown_walk.cpp \
                 extensions/aille_spire.cpp \
                 extensions/aille_lantern.cpp \
                 extensions/v7_3_pipeline.cpp \
                 aille.hpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OPTFLAGS) examples/v7_8_pilgrimage_demo.cpp extensions/aille_pilgrimage.cpp extensions/aille_weathering.cpp extensions/aille_crown_walk.cpp extensions/aille_spire.cpp extensions/aille_lantern.cpp extensions/v7_3_pipeline.cpp -o pilgrimage_demo
	@echo ""
	@echo "✓ Pilgrimage Demo compiled successfully!"
	@echo "  Run with: ./pilgrimage_demo"
	@echo ""

# Build and run unit tests
test_suite: $(UNIT_TESTS_SRC) aille.hpp $(EXT_SRCS) extensions/aille_crown_walk.cpp extensions/aille_pilgrimage.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OPTFLAGS) $(UNIT_TESTS_SRC) $(EXT_SRCS) extensions/aille_crown_walk.cpp extensions/aille_pilgrimage.cpp -o test_suite
	@echo ""
	@echo "✓ Test Suite compiled successfully!"
	@echo "  Run with: ./test_suite"
	@echo ""

# Integration test
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

# Benchmark
benchmark: $(BENCHMARK_SRC) aille.hpp $(EXT_SRCS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OPTFLAGS) $(BENCHMARK_SRC) $(EXT_SRCS) -o benchmark
	@echo ""
	@echo "✓ Benchmark compiled successfully!"
	@echo "  Run with: ./benchmark [iterations]"
	@echo ""

# Build Live Advisory Dashboard Server
dashboard_server: examples/dashboard_server.cpp ailee_plugins/plugins/dashboard/LiveAdvisoryObserver.cpp aille.hpp $(EXT_SRCS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OPTFLAGS) examples/dashboard_server.cpp ailee_plugins/plugins/dashboard/LiveAdvisoryObserver.cpp $(EXT_SRCS) -o dashboard_server
	@echo ""
	@echo "✓ Dashboard Server compiled successfully!"
	@echo "  Run with: ./dashboard_server"
	@echo ""

# WebSocket Server target
websocket_server: examples/websocket_server.cpp extensions/aille_websocket.cpp extensions/aille_websocket.hpp aille.hpp $(EXT_SRCS) extensions/aille_crown_walk.cpp extensions/aille_pilgrimage.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OPTFLAGS) examples/websocket_server.cpp extensions/aille_websocket.cpp $(EXT_SRCS) extensions/aille_crown_walk.cpp extensions/aille_pilgrimage.cpp -o websocket_server
	@echo ""
	@echo "✓ WebSocket Server compiled successfully!"
	@echo "  Run with: ./websocket_server"
	@echo ""

# Install header (copy to /usr/local/include)
install: aille.hpp
	@echo "Installing AILLE header..."
	sudo cp aille.hpp /usr/local/include/
	@echo "✓ Header installed to /usr/local/include/aille.hpp"
	@echo "  You can now use: #include <aille.hpp>"

# Uninstall
uninstall:
	sudo rm -f /usr/local/include/aille.hpp
	@echo "✓ AILLE header removed"

# Help
help:
	@echo "AILLE Framework - Build Targets"
	@echo "================================"
	@echo ""
	@echo "  make              - Build demo (optimized)"
	@echo "  make debug        - Build with debug symbols"
	@echo "  make run          - Build and run demo"
	@echo "  make test         - Run integration tests"
	@echo "  make benchmark    - Build benchmark harness"
	@echo "  make rest_api_server - Build REST API server"
	@echo "  make websocket_server - Build WebSocket Spire server"
	@echo "  make dashboard_server - Build Live Dashboard server"
	@echo "  make spire_demo   - Build Spire v7.4 demo"
	@echo "  make lantern_demo - Build Lantern v7.5 demo"
	@echo "  make crown_walk_demo - Build Crown Walk v7.6 demo"
	@echo "  make weathering_demo - Build Weathering v7.7 demo"
	@echo "  make pilgrimage_demo - Build Pilgrimage v7.8 demo"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make install      - Install header system-wide"
	@echo "  make help         - Show this message"
	@echo ""
	@echo "Quick start:"
	@echo "  make && ./demo"
	@echo ""
