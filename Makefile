# AILLE Framework - Makefile
# One command to build everything

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra
OPTFLAGS = -O3 -march=native -flto
DEBUGFLAGS = -g -O0 -DDEBUG

# Targets
EXAMPLE_SRC = examples/example.cpp
BENCHMARK_SRC = benchmarks/benchmark.cpp
INCLUDES = -I.
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

# Clean build artifacts
clean:
	rm -f demo demo_debug demo_audit.csv benchmark
	@echo "✓ Cleaned build artifacts"

# Run the demo
run: demo
	./demo

# Integration test
test: demo
	@echo "Running integration test..."
	./demo > test_output.txt
	@if grep -q "PASSED" test_output.txt; then \
		echo "✓ All tests passed"; \
	else \
		echo "✗ Tests failed"; \
		exit 1; \
	fi
	@rm test_output.txt

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
	@echo "  make          - Build demo (optimized)"
	@echo "  make debug    - Build with debug symbols"
	@echo "  make run      - Build and run demo"
	@echo "  make test     - Run integration tests"
	@echo "  make benchmark- Build benchmark harness"
	@echo "  make clean    - Remove build artifacts"
	@echo "  make install  - Install header system-wide"
	@echo "  make help     - Show this message"
	@echo ""
	@echo "Quick start:"
	@echo "  make && ./demo"
	@echo ""

.PHONY: all demo debug clean run test benchmark install uninstall help
