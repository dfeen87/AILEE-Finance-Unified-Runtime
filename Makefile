# ============================================================
# AILLEE Framework - Unified Makefile (No INCLUDES)
# ============================================================

CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -Wpedantic -O3

HTTPLIB_INCLUDES = -I./external
THREAD_FLAGS = -pthread

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

EXAMPLE_SRC      = examples/example.cpp
BENCHMARK_SRC    = benchmarks/benchmark.cpp
REST_API_SRC     = examples/rest_api_server.cpp
REST_API_IMPL    = extensions/aille_rest_api.cpp
UNIT_TESTS_SRC   = tests/unit_tests.cpp
SPIRE_DEMO_SRC   = examples/v7_4_spire_demo.cpp

.PHONY: all demo debug clean run test benchmark rest_api_server dashboard_server websocket_server \
        spire_demo lantern_demo crown_walk_demo weathering_demo pilgrimage_demo install uninstall help

all: demo

demo: $(EXAMPLE_SRC) aille.hpp $(EXT_SRCS)
	$(CXX) $(CXXFLAGS) $(EXAMPLE_SRC) $(EXT_SRCS) -o demo
	@echo ""
	@echo "✓ Demo compiled successfully!"
	@echo ""

debug: $(EXAMPLE_SRC) aille.hpp $(EXT_SRCS)
	$(CXX) $(CXXFLAGS) -g $(EXAMPLE_SRC) $(EXT_SRCS) -o demo_debug
	@echo ""
	@echo "✓ Debug build ready"
	@echo ""

rest_api_server: $(REST_API_SRC) $(REST_API_IMPL) aille_framework.cpp aille_audit.cpp aille.hpp
	$(CXX) $(CXXFLAGS) $(HTTPLIB_INCLUDES) $(THREAD_FLAGS) \
		$(REST_API_SRC) $(REST_API_IMPL) aille_framework.cpp aille_audit.cpp \
		-o rest_api_server
	@echo ""
	@echo "✓ REST API Server compiled successfully!"
	@echo ""

spire_demo: $(SPIRE_DEMO_SRC) aille.hpp $(EXT_SRCS)
	$(CXX) $(CXXFLAGS) $(SPIRE_DEMO_SRC) $(EXT_SRCS) -o spire_demo
	@echo ""
	@echo "✓ Spire Demo compiled successfully!"
	@echo ""

lantern_demo: examples/v7_5_lantern_demo.cpp aille.hpp $(EXT_SRCS)
	$(CXX) $(CXXFLAGS) examples/v7_5_lantern_demo.cpp $(EXT_SRCS) -o lantern_demo
	@echo ""
	@echo "✓ Lantern Demo compiled successfully!"
	@echo ""

crown_walk_demo: examples/v7_6_crown_walk_demo.cpp \
                 extensions/aille_crown_walk.cpp \
                 extensions/aille_spire.cpp \
                 extensions/aille_lantern.cpp \
                 extensions/v7_3_pipeline.cpp \
                 extensions/v7_2_pipeline.cpp \
                 extensions/aille_weathering.cpp \
                 aille.hpp
	$(CXX) $(CXXFLAGS) examples/v7_6_crown_walk_demo.cpp \
		extensions/aille_crown_walk.cpp extensions/aille_spire.cpp \
		extensions/aille_lantern.cpp extensions/v7_3_pipeline.cpp \
		extensions/v7_2_pipeline.cpp extensions/aille_weathering.cpp \
		-o crown_walk_demo
	@echo ""
	@echo "✓ Crown Walk Demo compiled successfully!"
	@echo ""

weathering_demo: examples/v7_7_weathering_demo.cpp \
                 extensions/aille_weathering.cpp \
                 extensions/aille_crown_walk.cpp \
                 extensions/aille_spire.cpp \
                 extensions/aille_lantern.cpp \
                 extensions/v7_3_pipeline.cpp \
                 extensions/v7_2_pipeline.cpp \
                 aille.hpp
	$(CXX) $(CXXFLAGS) examples/v7_7_weathering_demo.cpp \
		extensions/aille_weathering.cpp extensions/aille_crown_walk.cpp \
		extensions/aille_spire.cpp extensions/aille_lantern.cpp \
		extensions/v7_3_pipeline.cpp extensions/v7_2_pipeline.cpp \
		-o weathering_demo
	@echo ""
	@echo "✓ Weathering Demo compiled successfully!"
	@echo ""

pilgrimage_demo: examples/v7_8_pilgrimage_demo.cpp \
                 extensions/aille_pilgrimage.cpp \
                 extensions/aille_weathering.cpp \
                 extensions/aille_crown_walk.cpp \
                 extensions/aille_spire.cpp \
                 extensions/aille_lantern.cpp \
                 extensions/v7_3_pipeline.cpp \
                 aille.hpp
	$(CXX) $(CXXFLAGS) examples/v7_8_pilgrimage_demo.cpp \
		extensions/aille_pilgrimage.cpp extensions/aille_weathering.cpp \
		extensions/aille_crown_walk.cpp extensions/aille_spire.cpp \
		extensions/aille_lantern.cpp extensions/v7_3_pipeline.cpp \
		-o pilgrimage_demo
	@echo ""
	@echo "✓ Pilgrimage Demo compiled successfully!"
	@echo ""

test_suite: $(UNIT_TESTS_SRC) aille.hpp $(EXT_SRCS)
	$(CXX) $(CXXFLAGS) $(UNIT_TESTS_SRC) $(EXT_SRCS) -o test_suite
	@echo ""
	@echo "✓ Test Suite compiled successfully!"
	@echo ""

benchmark: $(BENCHMARK_SRC) aille.hpp $(EXT_SRCS)
	$(CXX) $(CXXFLAGS) $(BENCHMARK_SRC) $(EXT_SRCS) -o benchmark
	@echo ""
	@echo "✓ Benchmark compiled successfully!"
	@echo ""

dashboard_server: examples/dashboard_server.cpp ailee_plugins/plugins/dashboard/LiveAdvisoryObserver.cpp aille.hpp $(EXT_SRCS)
	$(CXX) $(CXXFLAGS) examples/dashboard_server.cpp \
		ailee_plugins/plugins/dashboard/LiveAdvisoryObserver.cpp \
		$(EXT_SRCS) -o dashboard_server
	@echo ""
	@echo "✓ Dashboard Server compiled successfully!"
	@echo ""

websocket_server: examples/websocket_server.cpp extensions/aille_websocket.cpp extensions/aille_websocket.hpp aille.hpp $(EXT_SRCS)
	$(CXX) $(CXXFLAGS) examples/websocket_server.cpp \
		extensions/aille_websocket.cpp $(EXT_SRCS) -o websocket_server
	@echo ""
	@echo "✓ WebSocket Server compiled successfully!"
	@echo ""

clean:
	rm -f websocket_server demo demo_debug demo_hotpath demo_audit.csv benchmark \
		rest_api_server rest_api_audit.csv test_suite test_audit.csv \
		test_integrity.csv dashboard_server spire_demo lantern_demo \
		crown_walk_demo weathering_demo pilgrimage_demo
	@echo "✓ Cleaned build artifacts"

install: aille.hpp
	@echo "Installing AILLE header..."
	sudo cp aille.hpp /usr/local/include/
	@echo "✓ Header installed"

uninstall:
	sudo rm -f /usr/local/include/aille.hpp
	@echo "✓ Header removed"

help:
	@echo "AILLEE Framework - Build Targets"
	@echo "================================"
	@echo "make demo"
	@echo "make rest_api_server"
	@echo "make websocket_server"
	@echo "make dashboard_server"
	@echo "make spire_demo"
	@echo "make lantern_demo"
	@echo "make crown_walk_demo"
	@echo "make weathering_demo"
	@echo "make pilgrimage_demo"
	@echo "make benchmark"
	@echo "make test_suite"
	@echo "make clean"
