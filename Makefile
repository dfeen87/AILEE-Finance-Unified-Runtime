# AILLE Framework - Makefile
# One command to build everything

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra
OPTFLAGS = -O3 -march=native -flto
DEBUGFLAGS = -g -O0 -DDEBUG

# Targets
EXAMPLE_SRC = examples/example.cpp
BENCHMARK_SRC = benchmarks/benchmark.cpp
REST_API_SRC = examples/rest_api_server.cpp
REST_API_IMPL = extensions/aille_rest_api.cpp
UNIT_TESTS_SRC = tests/unit_tests.cpp
INCLUDES = -I.
HTTPLIB_INCLUDES = -I./external
THREAD_FLAGS = -pthread
all: demo

# Build the demo (default)
demo: $(EXAMPLE_SRC) aille.hpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OPTFLAGS) $(EXAMPLE_SRC) -o demo
	@echo ""
	@echo "✓ Demo compiled successfully!"
	@echo "  Run with: ./demo"
	@echo ""

# Debug build
debug: $(EXAMPLE_SRC) aille.hpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEBUGFLAGS) $(EXAMPLE_SRC) -o demo_debug
	@echo ""
	@echo "✓ Debug build ready"
	@echo "  Run with: gdb ./demo_debug"
	@echo ""

# Build REST API server
rest_api_server: $(REST_API_SRC) $(REST_API_IMPL) aille.hpp extensions/aille_rest_api.hpp external/httplib.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(HTTPLIB_INCLUDES) $(OPTFLAGS) $(THREAD_FLAGS) $(REST_API_SRC) $(REST_API_IMPL) -o rest_api_server
	@echo ""
	@echo "✓ REST API Server compiled successfully!"
	@echo "  Run with: ./rest_api_server [port] [host]"
	@echo "  Default:  ./rest_api_server (port 8080, host 0.0.0.0)"
	@echo ""

# Clean build artifacts
clean:
	rm -f demo demo_debug demo_hotpath demo_audit.csv benchmark rest_api_server rest_api_audit.csv test_suite test_audit.csv test_integrity.csv
	@echo "✓ Cleaned build artifacts"

# Run the demo
run: demo
	./demo

# Hotpath build
hotpath: $(EXAMPLE_SRC) aille.hpp
	$(CXX) -std=c++17 -Wall -Wextra $(INCLUDES) -fno-exceptions -DAILLE_HOTPATH -O3 -march=native $(EXAMPLE_SRC) -o demo_hotpath
	@echo ""
	@echo "✓ Hotpath build ready"
	@echo ""

# Build and run unit tests
test_suite: $(UNIT_TESTS_SRC) aille.hpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OPTFLAGS) $(UNIT_TESTS_SRC) -o test_suite
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
benchmark: $(BENCHMARK_SRC) aille.hpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OPTFLAGS) $(BENCHMARK_SRC) -o benchmark
	@echo ""
	@echo "✓ Benchmark compiled successfully!"
	@echo "  Run with: ./benchmark [iterations]"
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
	@echo "  make clean        - Remove build artifacts"
	@echo "  make install      - Install header system-wide"
	@echo "  make help         - Show this message"
	@echo ""
	@echo "Quick start:"
	@echo "  make && ./demo"
	@echo ""

.PHONY: all demo debug clean run test benchmark rest_api_server install uninstall help
