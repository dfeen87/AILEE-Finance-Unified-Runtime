# Test Plan: AILLE Framework

This plan covers local verification steps for the demo, integration test, and benchmark harness.

## Prerequisites

- C++17-compatible compiler (GCC 7+, Clang 6+, MSVC 2019+).
- Build tools (make).

## Build Verification

1. Build the demo:
   ```bash
   make
   ```
2. Build the benchmark harness:
   ```bash
   make benchmark
   ```

## Functional Checks

1. Run the demo to confirm decisions and audit logging:
   ```bash
   make run
   ```
2. Run the integration test:
   ```bash
   make test
   ```

## Performance Benchmark

1. Run the benchmark with the default iteration count:
   ```bash
   ./benchmark
   ```
2. Optional: specify iterations for a longer run:
   ```bash
   ./benchmark 1000000
   ```

## Simulation Harness (Optional)

1. Run the reproducible synthetic simulation:
   ```bash
   python3 simulations/aille_simulation.py
   ```
2. Optional: export results to CSV:
   ```bash
   python3 simulations/aille_simulation.py --output results.csv
   ```

## Cleanup

```bash
make clean
```
