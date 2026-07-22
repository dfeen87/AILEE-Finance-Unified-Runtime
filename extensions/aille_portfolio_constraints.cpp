#include "aille_portfolio_constraints.hpp"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <algorithm>

namespace AILLE {

SectorId map_asset_to_sector(AssetId asset_id) {
    switch (asset_id) {
        case AssetId::CASH:
            return SectorId::FIAT;
        case AssetId::BTC:
        case AssetId::ETH:
            return SectorId::CRYPTO;
        case AssetId::GOLD:
            return SectorId::PRECIOUS_METALS;
        case AssetId::OIL:
            return SectorId::ENERGY;
        default:
            return SectorId::OTHER;
    }
}

PortfolioConstraintResult apply_portfolio_constraints(
    const AssetAllocations& proposed,
    const ConstraintRules& rules,
    const SectorDefinitions& sectors,
    const CorrelationProfiles& correlations,
    const RiskBudget& budget
) {
    PortfolioConstraintResult result;
    result.allocations = proposed;
    result.summary.initial_portfolio_risk = 0.0;
    result.summary.final_portfolio_risk = 0.0;
    result.summary.trace_count = 0;
    result.trace.step_count = 0;

    auto add_trace_step = [&](AssetId asset_id, ConstraintStage stage, ConstraintAction action, double before, double after, const char* msg) {
        if (result.trace.step_count < ConstraintTrace::MAX_STEPS) {
            ConstraintTraceStep& step = result.trace.steps[result.trace.step_count++];
            step.asset_id = asset_id;
            step.stage = static_cast<uint8_t>(stage);
            step.action_taken = static_cast<uint8_t>(action);
            step.before_value = before;
            step.after_value = after;
            std::strncpy(step.log, msg, sizeof(step.log) - 1);
            step.log[sizeof(step.log) - 1] = '\0';
        }
    };

    // Calculate initial portfolio risk
    double initial_risk = 0.0;
    for (size_t i = 0; i < result.allocations.count; ++i) {
        const auto& alloc = result.allocations.allocations[i];
        initial_risk += std::abs(alloc.allocation) * alloc.risk_score;
    }
    result.summary.initial_portfolio_risk = initial_risk;

    // Stage 1: Max-Exposure Clamping (Per Asset, Per Direction)
    for (size_t i = 0; i < result.allocations.count; ++i) {
        auto& alloc = result.allocations.allocations[i];
        double orig_val = alloc.allocation;

        // Find applicable rule
        const ConstraintRule* rule = nullptr;
        for (size_t r = 0; r < rules.count; ++r) {
            if (rules.rules[r].asset_id == alloc.asset_id && rules.rules[r].is_active) {
                rule = &rules.rules[r];
                break;
            }
        }

        if (rule) {
            if (alloc.allocation >= 0.0) {
                if (alloc.allocation > rule->max_long_exposure) {
                    alloc.allocation = rule->max_long_exposure;
                    add_trace_step(alloc.asset_id, ConstraintStage::EXPOSURE_CLAMP, ConstraintAction::CLAMPED, orig_val, alloc.allocation, "Max long exposure clamp");
                } else {
                    add_trace_step(alloc.asset_id, ConstraintStage::EXPOSURE_CLAMP, ConstraintAction::NO_CHANGE, orig_val, alloc.allocation, "Within exposure limit");
                }
            } else {
                // Shorting support
                double abs_alloc = std::abs(alloc.allocation);
                if (abs_alloc > rule->max_short_exposure) {
                    alloc.allocation = -rule->max_short_exposure;
                    add_trace_step(alloc.asset_id, ConstraintStage::EXPOSURE_CLAMP, ConstraintAction::CLAMPED, orig_val, alloc.allocation, "Max short exposure clamp");
                } else {
                    add_trace_step(alloc.asset_id, ConstraintStage::EXPOSURE_CLAMP, ConstraintAction::NO_CHANGE, orig_val, alloc.allocation, "Within short exposure limit");
                }
            }
        } else {
            add_trace_step(alloc.asset_id, ConstraintStage::EXPOSURE_CLAMP, ConstraintAction::NO_CHANGE, orig_val, alloc.allocation, "No exposure rule");
        }
    }

    // Stage 2: Sector Caps Enforcement with canonical sector classification
    for (size_t s = 0; s < sectors.count; ++s) {
        const auto& sec_def = sectors.sectors[s];
        if (!sec_def.is_active) continue;

        SectorId current_sector = static_cast<SectorId>(sec_def.sector_id);

        // Sum absolute exposure of assets belonging to this sector
        double sector_alloc_sum = 0.0;
        for (size_t i = 0; i < result.allocations.count; ++i) {
            const auto& alloc = result.allocations.allocations[i];
            if (map_asset_to_sector(alloc.asset_id) == current_sector) {
                sector_alloc_sum += std::abs(alloc.allocation);
            }
        }

        if (sector_alloc_sum > sec_def.max_sector_exposure && sector_alloc_sum > 0.0) {
            double scale = sec_def.max_sector_exposure / sector_alloc_sum;
            char msg_buf[40];
            std::snprintf(msg_buf, sizeof(msg_buf), "Sector %s cap breach", sec_def.sector_name);

            for (size_t i = 0; i < result.allocations.count; ++i) {
                auto& alloc = result.allocations.allocations[i];
                if (map_asset_to_sector(alloc.asset_id) == current_sector) {
                    double before = alloc.allocation;
                    alloc.allocation *= scale;
                    add_trace_step(alloc.asset_id, ConstraintStage::SECTOR_CAP_ENFORCE, ConstraintAction::CLAMPED, before, alloc.allocation, msg_buf);
                }
            }
        } else {
            for (size_t i = 0; i < result.allocations.count; ++i) {
                const auto& alloc = result.allocations.allocations[i];
                if (map_asset_to_sector(alloc.asset_id) == current_sector) {
                    add_trace_step(alloc.asset_id, ConstraintStage::SECTOR_CAP_ENFORCE, ConstraintAction::NO_CHANGE, alloc.allocation, alloc.allocation, "Sector cap pass");
                }
            }
        }
    }

    // Stage 3: Pairwise Correlation Dampening
    // Deterministic cluster logic:
    // CORR_CLUSTER_THRESHOLD = 0.70
    // CORR_CLUSTER_MAX_ALLOCATION = 0.30 of portfolio
    constexpr double CORR_CLUSTER_THRESHOLD = 0.70;
    constexpr double CORR_CLUSTER_MAX_ALLOCATION = 0.30;

    // Identify clusters where pairwise rho_ij > 0.70.
    // For simplicity and deterministic behavior: we find the connected components (clusters) based on correlations.
    // Given the max assets are 16, we can represent cluster membership using a simple labeling system.
    int cluster_labels[AssetAllocations::MAX_ASSETS];
    for (size_t i = 0; i < result.allocations.count; ++i) {
        cluster_labels[i] = static_cast<int>(i); // Each starts in its own cluster
    }

    // Union-find/connected components logic across active correlation profiles
    for (size_t p = 0; p < correlations.count; ++p) {
        const auto& profile = correlations.profiles[p];
        if (!profile.is_active || profile.correlation_score <= CORR_CLUSTER_THRESHOLD) {
            continue;
        }

        // Find index of asset_a and asset_b in result.allocations
        int idx_a = -1;
        int idx_b = -1;
        for (size_t i = 0; i < result.allocations.count; ++i) {
            if (result.allocations.allocations[i].asset_id == profile.asset_a) idx_a = static_cast<int>(i);
            if (result.allocations.allocations[i].asset_id == profile.asset_b) idx_b = static_cast<int>(i);
        }

        if (idx_a != -1 && idx_b != -1) {
            // Union of their clusters: map b's cluster label to a's cluster label
            int old_label = cluster_labels[idx_b];
            int new_label = cluster_labels[idx_a];
            if (old_label != new_label) {
                for (size_t i = 0; i < result.allocations.count; ++i) {
                    if (cluster_labels[i] == old_label) {
                        cluster_labels[i] = new_label;
                    }
                }
            }
        }
    }

    // Evaluate sum of allocations per cluster and apply dampening
    double cluster_sums[AssetAllocations::MAX_ASSETS] = {0.0};
    int cluster_sizes[AssetAllocations::MAX_ASSETS] = {0};

    for (size_t i = 0; i < result.allocations.count; ++i) {
        int label = cluster_labels[i];
        cluster_sizes[label]++;
        cluster_sums[label] += std::abs(result.allocations.allocations[i].allocation);
    }

    for (size_t i = 0; i < result.allocations.count; ++i) {
        int label = cluster_labels[i];
        if (cluster_sizes[label] > 1 && cluster_sums[label] > CORR_CLUSTER_MAX_ALLOCATION) {
            double scale = CORR_CLUSTER_MAX_ALLOCATION / cluster_sums[label];
            auto& alloc = result.allocations.allocations[i];
            double before = alloc.allocation;
            alloc.allocation *= scale;
            add_trace_step(alloc.asset_id, ConstraintStage::CORRELATION_DAMPEN, ConstraintAction::DAMPENED, before, alloc.allocation, "Correlation cluster dampened");
        } else {
            const auto& alloc = result.allocations.allocations[i];
            add_trace_step(alloc.asset_id, ConstraintStage::CORRELATION_DAMPEN, ConstraintAction::NO_CHANGE, alloc.allocation, alloc.allocation, "Correlation cluster pass");
        }
    }

    // Stage 4: Risk-Budget Enforcement
    double current_risk = 0.0;
    for (size_t i = 0; i < result.allocations.count; ++i) {
        const auto& alloc = result.allocations.allocations[i];
        current_risk += std::abs(alloc.allocation) * alloc.risk_score;
    }

    if (budget.is_active && current_risk > budget.max_portfolio_risk && current_risk > 0.0) {
        double scale = budget.max_portfolio_risk / current_risk;
        for (size_t i = 0; i < result.allocations.count; ++i) {
            auto& alloc = result.allocations.allocations[i];
            if (alloc.asset_id != AssetId::CASH) {
                double before = alloc.allocation;
                alloc.allocation *= scale;
                add_trace_step(alloc.asset_id, ConstraintStage::RISK_BUDGET_ENFORCE, ConstraintAction::SCALED, before, alloc.allocation, "Risk budget scaling applied");
            } else {
                add_trace_step(alloc.asset_id, ConstraintStage::RISK_BUDGET_ENFORCE, ConstraintAction::NO_CHANGE, alloc.allocation, alloc.allocation, "Cash excluded from scaling");
            }
        }
    } else {
        for (size_t i = 0; i < result.allocations.count; ++i) {
            const auto& alloc = result.allocations.allocations[i];
            add_trace_step(alloc.asset_id, ConstraintStage::RISK_BUDGET_ENFORCE, ConstraintAction::NO_CHANGE, alloc.allocation, alloc.allocation, "Risk budget pass");
        }
    }

    // Calculate final portfolio risk
    double final_risk = 0.0;
    for (size_t i = 0; i < result.allocations.count; ++i) {
        const auto& alloc = result.allocations.allocations[i];
        final_risk += std::abs(alloc.allocation) * alloc.risk_score;
    }
    result.summary.final_portfolio_risk = final_risk;
    result.summary.trace_count = static_cast<uint32_t>(result.trace.step_count);

    return result;
}

} // namespace AILLE
