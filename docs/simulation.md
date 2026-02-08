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

## Use real data

You can provide real market returns and/or model signals via CSV files.

### Market returns CSV

Provide a CSV with a `return` column (decimal returns, e.g., 0.01 for 1%):

```csv
return
0.0025
-0.0012
0.0031
```

Run with:

```bash
python3 simulations/aille_simulation.py --market-csv path/to/market.csv
```

### Model signals CSV

Provide a CSV with one row per model per timestep:

```csv
step,model_id,prediction,confidence
0,0,0.0012,0.82
0,1,0.0007,0.76
1,0,-0.0020,0.64
1,1,-0.0014,0.70
```

Run with:

```bash
python3 simulations/aille_simulation.py --market-csv path/to/market.csv --signals-csv path/to/signals.csv
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
