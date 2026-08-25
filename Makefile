# ============================================================
# AILLEE Framework - Unified Makefile
# ============================================================

CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -Wpedantic -O3

SYSTEM_INCLUDES = -isystem ./external

HTTPLIB_INCLUDES = -isystem ./external
SSL_FLAGS = -lssl -lcrypto
THREAD_FLAGS = -pthread $(SSL_FLAGS)
WEBSOCKET_FLAGS = -std=c++17 -isystem ./external/websocketpp -isystem ./external/asio/asio/include -DASIO_STANDALONE -pthread

COMMON_INCLUDES = -I. -I./extensions -I./telemetry -I./ailee_plugins -I./examples -I./src -I./include

# ANSI Colors
COLOR_GREEN  = \033[32m
COLOR_RED    = \033[31m
COLOR_YELLOW = \033[33m
COLOR_RESET  = \033[0m

EXT_SRCS = extensions/aille_btc.cpp \
           extensions/aille_eth.cpp \
           extensions/aille_oil.cpp \
           extensions/aille_gold.cpp \
           extensions/aille_silver.cpp \
           extensions/aille_copper.cpp \
           extensions/aille_natgas.cpp \
           extensions/aille_platinum.cpp \
           extensions/aille_forex_usd.cpp \
           extensions/aille_volume_advisory.cpp \
           extensions/aille_macro.cpp \
           extensions/aille_stabilizer.cpp \
           extensions/v7_2_pipeline.cpp \
           extensions/v7_3_pipeline.cpp \
           extensions/aille_spire.cpp \
           extensions/aille_lantern.cpp \
           extensions/aille_weathering.cpp \
           extensions/aille_crown_walk.cpp \
           extensions/aille_pilgrimage.cpp \
           extensions/aille_arbitration.cpp \
           extensions/aille_routing.cpp \
           extensions/aille_governor_reconciliation.cpp \
           extensions/aille_portfolio_constraints.cpp \
           extensions/aille_temporal_consistency.cpp \
           extensions/aille_stress_regime_override.cpp \
           extensions/aille_meta_governance.cpp \
           extensions/aille_membrane.cpp \
           extensions/aille_anomaly.cpp \
           extensions/aille_chart_intelligence.cpp \
           extensions/aille_wnfs.cpp \
           extensions/aille_unified_runtime.cpp

PYTHON_FLAGS = $(shell python3-config --cflags --embed --ldflags 2>/dev/null || python3-config --cflags --ldflags 2>/dev/null || echo "")

EXT_SRCS_WITH_AUDIT = $(EXT_SRCS) aille_audit.cpp ailee_plugins/plugins/execution/alpaca/AlpacaExecution.cpp src/ailee_finance_governor.cpp ailee_runtime/fs_gateway/fs_gateway.cpp

EXAMPLE_SRC      = examples/example.cpp
BENCHMARK_SRC    = benchmarks/benchmark.cpp
REST_API_SRC     = examples/rest_api_server.cpp
REST_API_IMPL    = extensions/aille_rest_api.cpp
UNIT_TESTS_SRC   = tests/unit_tests.cpp
SPIRE_DEMO_SRC   = examples/v7_4_spire_demo.cpp

.PHONY: all demo debug clean run test benchmark rest_api_server dashboard_server websocket_server \
        spire_demo lantern_demo crown_walk_demo weathering_demo pilgrimage_demo wnfs_demo install uninstall help \
        check_deps release

all: demo

check_deps:
	@printf "$(COLOR_YELLOW)Checking dependencies...$(COLOR_RESET)\n"
	@if [ ! -f external/httplib.h ]; then \
		printf "$(COLOR_RED)=========================================================================$(COLOR_RESET)\n"; \
		printf "$(COLOR_RED)ERROR: Missing external/httplib.h$(COLOR_RESET)\n"; \
		printf "$(COLOR_YELLOW)Run ./setup_rest_api.sh to install cpp-httplib.$(COLOR_RESET)\n"; \
		printf "$(COLOR_RED)=========================================================================$(COLOR_RESET)\n"; \
		printf "$(COLOR_RED)✗ Build aborted — see above diagnostics.$(COLOR_RESET)\n\n"; \
		exit 1; \
	fi

fs_gateway_test:
	@printf "$(COLOR_YELLOW)=== Executing FS-Gateway Subsystem Tests ===$(COLOR_RESET)\n"
	@PYTHONPATH=. pytest tests/test_fs_gateway_schema.py tests/test_desk_stream_correctness.py tests/test_connection_resilience.py

test_v20:
	@printf "$(COLOR_YELLOW)=== Executing FS-Gateway Subsystem Tests ===$(COLOR_RESET)\n"
	@PYTHONPATH=. pytest tests/test_fs_gateway_schema.py tests/test_desk_stream_correctness.py tests/test_connection_resilience.py
	@printf "$(COLOR_YELLOW)=== Executing AILEE v20.0.0 Full Validation Suite ===$(COLOR_RESET)\n"
	@PYTHONPATH=. pytest tests/

demo: $(EXAMPLE_SRC) aille.hpp $(EXT_SRCS_WITH_AUDIT)
	@printf "$(COLOR_YELLOW)=== AILEE CORE v20.0.0 — Deterministic Build Console ===$(COLOR_RESET)\n"
	@$(MAKE) --no-print-directory check_deps
	@printf "$(COLOR_YELLOW)Compiling runtime modules...$(COLOR_RESET)\n"
	@if $(CXX) $(CXXFLAGS) $(COMMON_INCLUDES) $(SYSTEM_INCLUDES) $(HTTPLIB_INCLUDES) $(EXAMPLE_SRC) $(EXT_SRCS_WITH_AUDIT) $(SSL_FLAGS) $(PYTHON_FLAGS) -o demo; then \
		printf "$(COLOR_GREEN)✓ Build completed successfully — deterministic and governed.$(COLOR_RESET)\n\n"; \
	else \
		printf "$(COLOR_RED)✗ Build aborted — see above diagnostics.$(COLOR_RESET)\n\n"; \
		exit 1; \
	fi

debug: $(EXAMPLE_SRC) aille.hpp $(EXT_SRCS_WITH_AUDIT)
	$(CXX) $(CXXFLAGS) -g $(COMMON_INCLUDES) $(EXAMPLE_SRC) $(EXT_SRCS_WITH_AUDIT) -o demo_debug
	@echo ""
	@echo "✓ Debug build ready"
	@echo ""

rest_api_server: $(REST_API_SRC) $(REST_API_IMPL) aille_framework.cpp aille_audit.cpp aille.hpp $(EXT_SRCS)
	@printf "$(COLOR_YELLOW)=== AILEE CORE v17.0.0 — Deterministic Build Console ===$(COLOR_RESET)\n"
	@$(MAKE) --no-print-directory check_deps
	@printf "$(COLOR_YELLOW)Compiling runtime modules...$(COLOR_RESET)\n"
	@if $(CXX) $(CXXFLAGS) $(COMMON_INCLUDES) $(HTTPLIB_INCLUDES) $(THREAD_FLAGS) $(REST_API_SRC) $(REST_API_IMPL) aille_framework.cpp aille_audit.cpp $(EXT_SRCS) -o rest_api_server; then \
		printf "$(COLOR_GREEN)✓ Build completed successfully — deterministic and governed.$(COLOR_RESET)\n\n"; \
	else \
		printf "$(COLOR_RED)✗ Build aborted — see above diagnostics.$(COLOR_RESET)\n\n"; \
		exit 1; \
	fi

spire_demo: $(SPIRE_DEMO_SRC) aille.hpp $(EXT_SRCS_WITH_AUDIT)
	$(CXX) $(CXXFLAGS) $(COMMON_INCLUDES) $(SPIRE_DEMO_SRC) $(EXT_SRCS_WITH_AUDIT) -o spire_demo
	@echo ""
	@echo "✓ Spire Demo compiled successfully!"
	@echo ""

lantern_demo: examples/v7_5_lantern_demo.cpp aille.hpp $(EXT_SRCS_WITH_AUDIT)
	$(CXX) $(CXXFLAGS) $(COMMON_INCLUDES) examples/v7_5_lantern_demo.cpp $(EXT_SRCS_WITH_AUDIT) -o lantern_demo
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
	$(CXX) $(CXXFLAGS) $(COMMON_INCLUDES) examples/v7_6_crown_walk_demo.cpp \
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
	$(CXX) $(CXXFLAGS) $(COMMON_INCLUDES) examples/v7_7_weathering_demo.cpp \
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
	$(CXX) $(CXXFLAGS) $(COMMON_INCLUDES) examples/v7_8_pilgrimage_demo.cpp \
		extensions/aille_pilgrimage.cpp extensions/aille_weathering.cpp \
		extensions/aille_crown_walk.cpp extensions/aille_spire.cpp \
		extensions/aille_lantern.cpp extensions/v7_3_pipeline.cpp \
		-o pilgrimage_demo
	@echo ""
	@echo "✓ Pilgrimage Demo compiled successfully!"
	@echo ""

wnfs_demo: examples/wnfs_demo.cpp aille.hpp $(EXT_SRCS_WITH_AUDIT)
	$(CXX) $(CXXFLAGS) $(COMMON_INCLUDES) examples/wnfs_demo.cpp $(EXT_SRCS_WITH_AUDIT) $(SSL_FLAGS) $(PYTHON_FLAGS) -o wnfs_demo
	@echo ""
	@echo "✓ WNFS Demo compiled successfully!"
	@echo ""

unified_runtime_demo: examples/unified_runtime_demo.cpp aille.hpp $(EXT_SRCS_WITH_AUDIT)
	$(CXX) $(CXXFLAGS) $(COMMON_INCLUDES) examples/unified_runtime_demo.cpp $(EXT_SRCS_WITH_AUDIT) $(SSL_FLAGS) $(PYTHON_FLAGS) -o unified_runtime_demo
	@echo ""
	@echo "✓ Unified Runtime Demo compiled successfully!"
	@echo ""

test_suite: $(UNIT_TESTS_SRC) aille.hpp $(EXT_SRCS_WITH_AUDIT)
	@printf "$(COLOR_YELLOW)=== AILEE CORE v19.0.0 — Deterministic Build Console ===$(COLOR_RESET)\n"
	@$(MAKE) --no-print-directory check_deps
	@printf "$(COLOR_YELLOW)Compiling runtime modules...$(COLOR_RESET)\n"
	@if $(CXX) $(CXXFLAGS) $(COMMON_INCLUDES) $(WEBSOCKET_FLAGS) $(UNIT_TESTS_SRC) $(EXT_SRCS_WITH_AUDIT) $(SSL_FLAGS) $(PYTHON_FLAGS) -o test_suite; then \
		printf "$(COLOR_GREEN)✓ Build completed successfully — deterministic and governed.$(COLOR_RESET)\n\n"; \
	else \
		printf "$(COLOR_RED)✗ Build aborted — see above diagnostics.$(COLOR_RESET)\n\n"; \
		exit 1; \
	fi

benchmark: $(BENCHMARK_SRC) aille.hpp $(EXT_SRCS_WITH_AUDIT)
	@printf "$(COLOR_YELLOW)=== AILEE CORE v17.0.0 — Deterministic Build Console ===$(COLOR_RESET)\n"
	@$(MAKE) --no-print-directory check_deps
	@printf "$(COLOR_YELLOW)Compiling runtime modules...$(COLOR_RESET)\n"
	@if $(CXX) $(CXXFLAGS) $(COMMON_INCLUDES) $(BENCHMARK_SRC) $(EXT_SRCS_WITH_AUDIT) $(SSL_FLAGS) $(PYTHON_FLAGS) -o benchmark; then \
		printf "$(COLOR_GREEN)✓ Build completed successfully — deterministic and governed.$(COLOR_RESET)\n\n"; \
	else \
		printf "$(COLOR_RED)✗ Build aborted — see above diagnostics.$(COLOR_RESET)\n\n"; \
		exit 1; \
	fi

dashboard_server: examples/dashboard_server.cpp ailee_plugins/plugins/dashboard/LiveAdvisoryObserver.cpp aille.hpp $(EXT_SRCS_WITH_AUDIT)
	@printf "$(COLOR_YELLOW)=== AILEE CORE v17.0.0 — Deterministic Build Console ===$(COLOR_RESET)\n"
	@$(MAKE) --no-print-directory check_deps
	@printf "$(COLOR_YELLOW)Compiling runtime modules...$(COLOR_RESET)\n"
	@if $(CXX) $(CXXFLAGS) $(COMMON_INCLUDES) $(WEBSOCKET_FLAGS) examples/dashboard_server.cpp ailee_plugins/plugins/dashboard/LiveAdvisoryObserver.cpp $(EXT_SRCS_WITH_AUDIT) $(SSL_FLAGS) $(PYTHON_FLAGS) -o dashboard_server; then \
		printf "$(COLOR_GREEN)✓ Build completed successfully — deterministic and governed.$(COLOR_RESET)\n\n"; \
	else \
		printf "$(COLOR_RED)✗ Build aborted — see above diagnostics.$(COLOR_RESET)\n\n"; \
		exit 1; \
	fi

websocket_server: examples/websocket_server.cpp extensions/aille_websocket.cpp extensions/aille_websocket.hpp aille.hpp $(EXT_SRCS_WITH_AUDIT)
	@printf "$(COLOR_YELLOW)=== AILEE CORE v19.0.0 — Deterministic Build Console ===$(COLOR_RESET)\n"
	@$(MAKE) --no-print-directory check_deps
	@printf "$(COLOR_YELLOW)Compiling runtime modules...$(COLOR_RESET)\n"
	@if $(CXX) $(CXXFLAGS) $(COMMON_INCLUDES) $(WEBSOCKET_FLAGS) examples/websocket_server.cpp extensions/aille_websocket.cpp $(EXT_SRCS_WITH_AUDIT) $(SSL_FLAGS) $(PYTHON_FLAGS) -o websocket_server; then \
		printf "$(COLOR_GREEN)✓ Build completed successfully — deterministic and governed.$(COLOR_RESET)\n\n"; \
	else \
		printf "$(COLOR_RED)✗ Build aborted — see above diagnostics.$(COLOR_RESET)\n\n"; \
		exit 1; \
	fi

fs_gateway: ailee_runtime/fs_gateway/main.cpp ailee_runtime/fs_gateway/fs_gateway.cpp ailee_runtime/fs_gateway/fs_gateway.hpp aille.hpp $(EXT_SRCS_WITH_AUDIT)
	@printf "$(COLOR_YELLOW)=== AILEE CORE v20.0.0 — FS-Gateway Networking Module ===$(COLOR_RESET)\n"
	@$(MAKE) --no-print-directory check_deps
	@printf "$(COLOR_YELLOW)Compiling FS-Gateway server...$(COLOR_RESET)\n"
	@mkdir -p bin
	@if $(CXX) $(CXXFLAGS) $(COMMON_INCLUDES) $(WEBSOCKET_FLAGS) ailee_runtime/fs_gateway/main.cpp $(EXT_SRCS_WITH_AUDIT) $(SSL_FLAGS) $(PYTHON_FLAGS) -o bin/ailee_fs_gateway; then \
		printf "$(COLOR_GREEN)✓ FS-Gateway binary built successfully → bin/ailee_fs_gateway.$(COLOR_RESET)\n\n"; \
	else \
		printf "$(COLOR_RED)✗ Build aborted — see above diagnostics.$(COLOR_RESET)\n\n"; \
		exit 1; \
	fi

release:
	@printf "$(COLOR_YELLOW)=== AILEE CORE v19.0.0 — Deterministic Build Console ===$(COLOR_RESET)\n"
	@$(MAKE) --no-print-directory check_deps
	@printf "$(COLOR_YELLOW)Compiling runtime modules...$(COLOR_RESET)\n"
	@$(MAKE) --no-print-directory demo
	@$(MAKE) --no-print-directory fs_gateway
	@$(MAKE) --no-print-directory rest_api_server
	@$(MAKE) --no-print-directory websocket_server
	@$(MAKE) --no-print-directory dashboard_server
	@$(MAKE) --no-print-directory benchmark
	@$(MAKE) --no-print-directory test_suite
	@printf "$(COLOR_YELLOW)Running test suite...$(COLOR_RESET)\n"
	@if ./test_suite; then \
		printf "$(COLOR_GREEN)✓ Test suite passed!$(COLOR_RESET)\n"; \
	else \
		printf "$(COLOR_RED)✗ Test suite failed!$(COLOR_RESET)\n"; \
		printf "$(COLOR_RED)✗ Build aborted — see above diagnostics.$(COLOR_RESET)\n\n"; \
		exit 1; \
	fi
	@printf "$(COLOR_YELLOW)Creating release package...$(COLOR_RESET)\n"
	@mkdir -p release
	@for binary in demo rest_api_server websocket_server dashboard_server benchmark test_suite bin/ailee_fs_gateway; do \
		if cp $$binary release/ 2>/dev/null; then \
			printf "$(COLOR_GREEN)✓ Copying $$binary → release/$(COLOR_RESET)\n"; \
		else \
			printf "$(COLOR_RED)✗ Copying $$binary → release/$(COLOR_RESET)\n"; \
			printf "$(COLOR_RED)✗ Build aborted — see above diagnostics.$(COLOR_RESET)\n\n"; \
			exit 1; \
		fi; \
	done
	@echo "20.0.0" > release/VERSION
	@printf "$(COLOR_GREEN)✓ Stamped Version: 20.0.0$(COLOR_RESET)\n"
	@printf "$(COLOR_GREEN)AILEE CORE v20.0.0 Release Package Ready.$(COLOR_RESET)\n"
	@printf "$(COLOR_GREEN)=========================================================$(COLOR_RESET)\n"

clean:
	rm -rf websocket_server demo demo_debug demo_hotpath demo_audit.csv benchmark \
		rest_api_server rest_api_audit.csv test_suite test_audit.csv \
		test_integrity.csv dashboard_server spire_demo lantern_demo \
		crown_walk_demo weathering_demo pilgrimage_demo release
	@echo "✓ Cleaned build artifacts"

install: aille.hpp
	@echo "Installing AILLE v14.0.0 Framework..."
	@echo "Copying headers to system path..."
	sudo cp aille.hpp /usr/local/include/
	@echo "✓ Header installed (Version 14.0.0)"

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
	@echo "make release"
	@echo "make clean"
