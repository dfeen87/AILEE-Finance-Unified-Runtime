# Simulation Harness

This repository now includes a reproducible Python simulation harness to
compare the AILLE decision engine against a naive baseline strategy using
synthetic market returns.

## What the simulation does

- Generates synthetic daily returns with regime shifts and occasional crash
  events.
- Produces multiple noisy model signals per timestep.
- Runs the AILLE safety/consensus/fallback logic to generate a position.
- Runs a naive baseline (simple average of model signals).
- Computes performance metrics for both strategies.

## Run the simulation

```bash
python3 simulations/aille_simulation.py
```

### Customize parameters

```bash
python3 simulations/aille_simulation.py --steps 5000 --seed 42 --models 4
```

### Export results to CSV

```bash
python3 simulations/aille_simulation.py --output results.csv
```

## Metrics reported

- Total return
- Annualized return
- Sharpe ratio
- Max drawdown
- Volatility
- Catastrophic trades (returns below -5%)

> Note: These results are synthetic and intended for reproducible benchmarking
> of the AILLE mechanics rather than real-market performance claims.
