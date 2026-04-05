# AILLE Metrics Extension – Reference Thresholds
## Illustrative Defaults for Monitoring & Alerting

---

## Purpose

This document provides **illustrative reference thresholds** for teams deploying the AILLE Metrics Extension.

These values are:
- Not prescriptive
- Not universal
- Intended as starting points only

All thresholds must be tuned to:
- Strategy type
- Market regime
- Risk tolerance
- Regulatory context

---

## Threshold Philosophy

Metrics should:
- Trigger investigation, not panic
- Be evaluated over time windows
- Be correlated with other signals

**Single-point breaches should not drive automated action.**

---

## Reference Profiles

### Conservative Profile
*(Regulated funds, pensions, capital-preserving strategies)*

| Metric | Threshold |
|--------|-----------|
| Fallback Activation Rate | > 5% |
| Consensus Failure Rate | > 10% |
| Avg Confidence Drop | Sustained decline > 15% |
| Invalid Inputs | > 0 (immediate review) |

Recommended response:
- Investigate models
- Review inputs
- Consider temporary exposure reduction

---

### Balanced Profile
*(General trading, hedge funds, diversified strategies)*

| Metric | Threshold |
|--------|-----------|
| Fallback Activation Rate | > 8–10% |
| Consensus Failure Rate | > 15% |
| Avg Confidence Drop | Sustained decline > 20% |
| Invalid Inputs | > 0 |

Recommended response:
- Monitor closely
- Correlate with volatility and regime signals

---

### Aggressive Profile
*(High-frequency, proprietary, alpha-seeking systems)*

| Metric | Threshold |
|--------|-----------|
| Fallback Activation Rate | > 12–15% |
| Consensus Failure Rate | > 20% |
| Avg Confidence Drop | Sustained decline > 25% |
| Invalid Inputs | > 0 |

Recommended response:
- Diagnose model disagreement
- Validate data integrity
- Avoid reflexive shutdowns

---

## Time Window Guidance

Metrics should be evaluated over:
- Rolling windows (e.g., 5m, 30m, 1h)
- Multiple sessions for context

Avoid:
- Tick-by-tick reactions
- Single-decision alerts

---

## Alerting Best Practices

- Use **rate-of-change** alerts, not absolute values only
- Combine multiple metrics before escalation
- Log context alongside alerts

Example:
> “Fallback rate rising + confidence declining + volatility spike”

This combination is more actionable than any single metric.

---

## Regulatory Considerations

In regulated environments:
- Document threshold rationale
- Track threshold changes
- Maintain auditability of responses

AILLE metrics support oversight — they do not replace governance.

---

## Summary

These thresholds are **guides**, not guarantees.

Effective teams:
- Tune conservatively
- Review periodically
- Adjust with evidence

AILLE provides visibility.  
Sound judgment provides safety.
