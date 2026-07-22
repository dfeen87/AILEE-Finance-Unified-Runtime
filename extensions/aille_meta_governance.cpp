#include "aille_meta_governance.hpp"
#include <cstring>
#include <algorithm>

namespace AILLE {

MetaGovernanceState apply_meta_governance_lock(
    const ReconciledResult& decision,
    const PortfolioConstraintResult& constraints,
    const StressPortfolioState& stress_state,
    const StressTraceSteps& stress_trace,
    const TemporalPortfolioState& temporal_state,
    MetaGovernanceTraceSteps& trace
) {
    trace.count = 0;

    auto add_step = [&](uint32_t reason, const char* msg) {
        if (trace.count < MetaGovernanceTraceSteps::MAX_STEPS) {
            MetaGovernanceTraceStep& step = trace.steps[trace.count++];
            step.reason_code = reason;
            std::strncpy(step.log, msg, sizeof(step.log) - 1);
            step.log[sizeof(step.log) - 1] = '\0';
            std::memset(step.reserved, 0, sizeof(step.reserved));
        }
    };

    // 1. Meta-checks
    bool governor_conflict = (decision.summary.total_residual > META_GOVERNANCE_RESIDUAL_THRESHOLD);
    bool constraint_violation = (constraints.summary.remaining_violations > 0 ||
                                 constraints.summary.final_portfolio_risk > constraints.summary.max_risk_budget);

    // Note: StressMode::STRESS matches uint8_t 1, StressMode::CRISIS matches uint8_t 2
    bool stress_override_missing = (stress_state.stress_level >= 1 && stress_trace.count == 0);
    bool temporal_inconsistent = (temporal_state.residual_sum > TEMPORAL_RESIDUAL_CRITICAL_THRESHOLD);

    // 2. Log trace steps for any detected issues
    if (governor_conflict) {
        add_step(GOVERNOR_CONFLICT, "Governor conflict detected");
    }
    if (constraint_violation) {
        add_step(CONSTRAINT_VIOLATION, "Constraint violation detected");
    }
    if (stress_override_missing) {
        add_step(STRESS_OVERRIDE_MISSING, "Stress override missing");
    }
    if (temporal_inconsistent) {
        add_step(TEMPORAL_INCONSISTENT, "Temporal residual too high");
    }

    // 3. Determine execution_ready flag
    bool execution_ready = !governor_conflict && !constraint_violation &&
                           !stress_override_missing && !temporal_inconsistent;

    if (execution_ready) {
        add_step(0, "Meta-governance lock verified & ready");
    }

    // 4. Construct final locked state snapshot
    MetaGovernanceState final_state;
    final_state.final_portfolio_risk = constraints.summary.final_portfolio_risk;
    final_state.final_residual_sum = decision.summary.total_residual + temporal_state.residual_sum;
    final_state.final_stress_level = stress_state.stress_level;
    final_state.execution_ready = execution_ready ? 1 : 0;
    std::memset(final_state.reserved, 0, sizeof(final_state.reserved));

    return final_state;
}

} // namespace AILLE
