# Mathematical evidence in AILEE Finance

The dependency-free `ailee_finance.math_engine` and C++17
`extensions/aille_delta_v.hpp` implement `ailee-delta-v/v1`:

```text
prefactor = Isp * eta * exp(-alpha * v0**2)
integrand(t) = P_input(t) * exp(-alpha * w(t)**2)
               * exp(2 * alpha * v0) * v(t) / M(t)
delta_v = prefactor * composite_trapezoidal_integral(integrand, time)
```

The numerator is **`exp(2 * alpha * v0) * v(t)`**. Velocity is outside that
exponential. Integration runs from the first supplied timestamp to the last;
the engine does not invent an unsampled initial interval. There is no `g0`,
logarithmic mass-ratio term, adaptive quadrature, or stochastic heuristic.

This ports the [Trust Layer reference](https://github.com/dfeen87/AILEE-Trust-Layer/blob/de238cd4a9edd53f978bc33a02f77f891887f263/docs/MATHEMATICAL_ENGINE.md)
and strengthens evidence validation and finite trapezoid arithmetic. Python
models/evidence are frozen dataclasses; computation snapshots normalized input
values. At least two and at most 100,000 samples, finite strictly increasing
times, `Isp > 0`, `eta` in `[0,1]`, `alpha >= 0`, nonnegative input power and
positive mass are required. Workload, initial velocity and sampled velocity
may be signed and must be finite. Python booleans/numeric strings are rejected.

Malformed input raises `MathValidationError`; conversion/exponential overflow
or non-finite arithmetic raises `MathNumericalError`. Mathematical failure is
explicit, before a SELL domain evaluation or audit write. Exponential underflow
to zero is legitimate. Ordinary binary rounding remains; a finite half-sum
avoids overflow in an otherwise representable trapezoid mean. Neither engine
clamps mathematical failure into a successful result.

## Workflow example

```python
from ailee_finance.core_min import AileeFinanceTrustPipeline
from ailee_finance.math_engine import DeltaVParameters, DeltaVSample

parameters = DeltaVParameters(300.0, 0.82, 0.002, 4.0)
samples = (
    DeltaVSample(0.0, 12.0, 1.0, 4.0, 100.0),
    DeltaVSample(2.0, 13.0, 1.2, 4.5, 99.0),
)
decision = AileeFinanceTrustPipeline().process_sell(
    {"position_size": 1000.0, "trust_score": 0.9,
     "market": {"liquidity": 1.0}, "feeds": [100.0, 100.0]},
    math_parameters=parameters, math_samples=samples,
)
assert decision.allowed_sell_amount == 800.0
assert decision.mathematical_evidence["formula_version"] == "ailee-delta-v/v1"
```

`compute_delta_v` can also be called directly. `build_delta_v_evidence` validates
result consistency, formula/integration identifiers, time bounds, count, finite
values and source-digest syntax. Python's compact evidence includes a SHA-256
digest covering all normalized parameters and samples. `attach_delta_v_evidence`
deep-copies caller context under `ailee_delta_v`; it does not retain the stream.
The hash detects a change in represented inputs; it is not a signature, sensor
authentication, or proof that a caller-constructed result was actually computed.

## Authority boundary

The optional SELL workflow computes its own mathematical result from supplied
parameters/samples. A caller's reserved-looking `ailee_delta_v` or `authorized`
fields cannot override that computation or any SELL governance decision. The
result is attached to the decision and its JSON audit entry. Validation,
intent, trust, manipulation, consensus, hard ceilings, and fallback retain the
same authority with or without mathematical evidence. Positive Δv alone does
not create a volume order, clear a lockout, or release a replay reservation.

The legacy `calculate_hft_delta_v` API remains a **rectangle-rule single-tick
strategy heuristic**. It accepts signed signal power and a positive mass floor;
it is not labeled canonical sampled mathematical evidence. Its Python/C++ tick
durations now agree, zero/negative mass or duration is rejected/marked invalid,
and C++ invalid numeric output forces zero execution weight and disables buy
flags. Existing strategy modulation remains separate from the evidence API.

Without calibrated units, independent measurements and an explicit system
model, the calculated quantity is an equation-defined optimization metric.
This implementation does not establish financial returns, empirical truth,
physical propulsion performance, or a theorem that system stability is conserved.
