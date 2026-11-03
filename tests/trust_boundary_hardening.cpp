// Throwing checks intentionally remain active with -DNDEBUG in Release builds.
#include "aille.hpp"
#include "include/ailee_finance_governor.hpp"
#include "extensions/aille_unified_runtime.hpp"
#include "extensions/aille_anomaly.hpp"
#include "extensions/aille_stabilizer.hpp"
#include "extensions/aille_sync_adapter.hpp"
#include "extensions/aille_portfolio_constraints.hpp"
#include "extensions/aille_temporal_consistency.hpp"
#include "extensions/aille_delta_v.hpp"
#include "extensions/aille_math.hpp"
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <filesystem>
#include <fstream>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
constexpr float nan = std::numeric_limits<float>::quiet_NaN();
constexpr float inf = std::numeric_limits<float>::infinity();

void engine_config() {
    float AILLE::AILLEConfig::* fields[] = {
        &AILLE::AILLEConfig::min_confidence_threshold,
        &AILLE::AILLEConfig::grace_confidence_threshold,
        &AILLE::AILLEConfig::sign_agreement_threshold,
        &AILLE::AILLEConfig::fallback_position_scale,
        &AILLE::AILLEConfig::fallback_alpha, &AILLE::AILLEConfig::fallback_beta,
        &AILLE::AILLEConfig::contrarian_oversold_aggressiveness,
        &AILLE::AILLEConfig::max_position_abs,
    };
    for (auto field : fields) for (float value : {nan, inf, -inf}) {
        AILLE::AILLEConfig cfg;
        cfg.*field = value;
        AILLE::AILLEEngine engine(cfg);
        AILLE::ModelSignal signals[] = {{0.1f, 0.9f, 0}, {0.1f, 0.9f, 1}};
        auto decision = engine.makeDecision(signals, 2);
        require(decision.status == AILLE::ERROR_NO_MODELS, "invalid numeric config accepted");
        require(std::isfinite(decision.final_value) && decision.final_value == 0, "invalid config output");
    }
    for (int quorum : {0, 11}) {
        AILLE::AILLEConfig cfg;
        cfg.min_models_required = quorum;
        AILLE::AILLEEngine engine(cfg);
        AILLE::ModelSignal signals[] = {{0.1f, 0.9f, 0}, {0.1f, 0.9f, 1}};
        require(engine.makeDecision(signals, 2).status == AILLE::ERROR_NO_MODELS, "impossible quorum accepted");
    }
}

void engine_fallback() {
    for (bool dynamic : {false, true}) for (float sign : {-1.0f, 1.0f}) {
        AILLE::AILLEConfig cfg;
        cfg.max_position_abs = 0.02f;
        cfg.enable_dynamic_fallback = dynamic;
        AILLE::AILLEEngine engine(cfg);
        AILLE::ModelSignal accepted[] = {{sign * 0.1f, 0.9f, 0}, {sign * 0.1f, 0.9f, 1}};
        require(engine.makeDecision(accepted, 2).status == AILLE::DECISION_VALID, "valid setup rejected");
        // Every rejection route must use the same final hard envelope.
        AILLE::ModelSignal rejected[][2] = {
            {{nan, 0.9f, 0}, {0.1f, 0.9f, 1}},
            {{0.1f, -1.0f, 0}, {0.1f, 0.9f, 1}},
            {{0.1f, 0.0f, 0}, {0.1f, 0.0f, 1}},
            {{0.1f, 0.9f, 0}, {-0.1f, 0.9f, 1}},
            {{0.1f, 0.9f, 0}, {0.1f, 0.9f, 0}},
        };
        for (auto& signals : rejected) {
            auto decision = engine.makeDecision(signals, 2);
            require(decision.fallback_used, "expected fallback route");
            require(std::isfinite(decision.final_value) && std::abs(decision.final_value) <= cfg.max_position_abs,
                    "fallback bypasses hard position cap");
        }
        cfg.max_position_abs = 0.01f;
        engine.setConfig(cfg);
        require(std::abs(engine.makeDecision(rejected[0], 2).final_value) <= cfg.max_position_abs,
                "runtime tightened cap ignored by fallback");
    }
}

void engine_reconfigure() {
    AILLE::AILLEConfig cfg;
    cfg.fallback_window_size = 8;
    cfg.max_signal_age_ns = 0;
    AILLE::AILLEEngine engine(cfg);
    for (int i = 0; i < 7; ++i) {
        const float value = i == 6 ? -0.1f : 0.1f;
        AILLE::ModelSignal signals[] = {{value, 0.9f, 0}, {value, 0.9f, 1}};
        require(engine.makeDecision(signals, 2).status == AILLE::DECISION_VALID, "valid history rejected");
    }
    auto invalid = cfg;
    invalid.fallback_position_scale = inf;
    bool threw = false;
    try { engine.setConfig(invalid); } catch (const std::invalid_argument&) { threw = true; }
    require(threw && engine.getConfig().fallback_position_scale == cfg.fallback_position_scale,
            "invalid config update was not atomic");
    cfg.fallback_window_size = 1;
    engine.setConfig(cfg);
    AILLE::ModelSignal rejected[] = {{nan, 0.9f, 0}};
    require(engine.makeDecision(rejected, 1).final_value < 0, "window shrink lost newest accepted history");
    require(engine.makeDecision(nullptr, 2).status == AILLE::ERROR_NO_MODELS, "null model input not rejected");
}

void sell_governor() {
    ailee::AileeFinanceGovernor governor;
    ailee::RawSellSignals signals;
    signals.position_size = 1000.0;
    signals.trust_score = 0.9;
    signals.feeds = {{"a", 100, 0.95}, {"b", 100, 0.95}};
    for (double value : {double(nan), double(inf), double(-inf), -1.0}) {
        auto bad = signals;
        bad.position_size = value;
        auto decision = governor.evaluateSell(bad);
        require(decision.level == 3 && decision.allowed_sell_amount == 0 && !decision.bullish_mode_active,
                "invalid position became SELL permission");
        bad = signals;
        bad.volatility = value;
        decision = governor.evaluateSell(bad);
        require(decision.level == 3 && std::isfinite(decision.allowed_sell_amount) &&
                decision.allowed_sell_amount <= 100 && !decision.bullish_mode_active,
                "invalid volatility became trusted SELL context");
    }
    auto decision = governor.evaluateSell(signals);
    require(decision.level == 0 && decision.allowed_sell_amount > 799 && decision.allowed_sell_amount < 801,
            "valid SELL changed");
}

void unified_evidence() {
    for (float value : {nan, inf, -inf, -1.0f, 1.1f}) for (int field = 0; field < 4; ++field) {
        AILLE::UnifiedRuntimeState state;
        AILLE::UnifiedRuntimeMetrics metrics;
        AILLE::UnifiedRuntimeConfig cfg;
        AILLE::AnomalyAdvisory anomaly;
        anomaly.advisory_active = 1;
        AILLE::MarketStabilizerAdvisory stabilizer;
        stabilizer.risk_elevated = true;
        if (field == 0) cfg.max_allowed_risk = value;
        if (field == 1) stabilizer.dynamic_clamp_limit = value;
        if (field == 2) stabilizer.stabilization_factor = value;
        // Anomaly's documented C++ range is [0,100].
        if (field == 3) anomaly.anomaly_severity = value == 1.1f ? 101.0f : value;
        auto result = AILLE::evaluate_unified_runtime(state, metrics, nullptr, &anomaly, &stabilizer,
                                                     nullptr, nullptr, cfg);
        require(!result.execution_permitted && result.hft_freeze_active && result.recommended_execution_scale == 0,
                "malformed evidence did not lock unified execution");
        require(state.system_status == AILLE::UNIFIED_STATUS_META_LOCKED && state.aggregate_risk_score == 1 &&
                state.systemic_stability_index == 0 && metrics.meta_lock_active,
                "invalid evidence not reflected in finite state/metrics");
    }
}

void bound_governance() {
    AILLE::AILLEEngine engine;
    AILLE::UnifiedRuntimeState state;
    AILLE::UnifiedRuntimeMetrics metrics;
    AILLE::UnifiedRuntimeAdvisory advisory;
    AILLE::UnifiedRuntimeConfig cfg;
    cfg.max_allowed_risk = nan;
    engine.set_unified_runtime_state(&state);
    engine.set_unified_runtime_metrics(&metrics);
    engine.set_unified_runtime_advisory(&advisory);
    engine.set_unified_runtime_config(&cfg);
    AILLE::ModelSignal signals[] = {{0.1f, 0.9f, 0}, {0.1f, 0.9f, 1}};
    auto decision = engine.makeDecision(signals, 2);
    require(!advisory.execution_permitted && decision.fallback_used && decision.final_value == 0,
            "decision engine bypassed bound unified denial");
    // A denial cannot seed the positive fallback history.
    engine.set_unified_runtime_advisory(nullptr);
    signals[0].value = nan;
    require(engine.makeDecision(signals, 2).final_value == 0, "denied decision mutated accepted history");
    signals[0].value = 0.1f;
    AILLE::MarketStabilizerAdvisory stabilizer;
    stabilizer.stabilization_risk_score = 90.0f;
    engine.set_stabilizer_advisory(&stabilizer);
    for (float value : {nan, inf, -inf, -2.0f, 2.0f}) {
        stabilizer.dynamic_clamp_limit = value;
        decision = engine.makeDecision(signals, 2);
        require(decision.fallback_used && decision.final_value == 0, "malformed stabilizer bypassed hard envelope");
    }
}

void native_governance() {
    AILLE::AssetAllocations allocations{};
    allocations.count=1;
    allocations.allocations[0].allocation=0.5;
    allocations.allocations[0].risk_score=50;
    AILLE::RiskBudget budget{};
    budget.max_portfolio_risk=nan;
    bool rejected=false;
    try { (void)AILLE::apply_portfolio_constraints(allocations, {}, {}, {}, budget); }
    catch (const std::invalid_argument&) { rejected=true; }
    require(rejected, "nonfinite budget bypassed native constraints");
    allocations.count=AILLE::AssetAllocations::MAX_ASSETS+1;
    budget.max_portfolio_risk=25;
    rejected=false;
    try { (void)AILLE::apply_portfolio_constraints(allocations, {}, {}, {}, budget); }
    catch (const std::invalid_argument&) { rejected=true; }
    require(rejected, "oversized native container not rejected");
    AILLE::TemporalStates current{};
    current.count=1;
    current.states[0].prev_allocation=0.5;
    AILLE::TemporalPortfolioState portfolio{};
    AILLE::TemporalResiduals residual{};
    AILLE::TemporalTraceSteps trace{};
    rejected=false;
    try { AILLE::enforce_temporal_consistency({}, current, {}, portfolio, residual, trace, nan); }
    catch (const std::invalid_argument&) { rejected=true; }
    require(rejected && current.states[0].prev_allocation==0.5 && trace.count==0, "temporal failure mutated state");
}

void unified_replay() {
    AILLE::UnifiedRuntimeState state;
    AILLE::UnifiedRuntimeMetrics metrics;
    AILLE::SyncTick tick;
    tick.tick_index=10; tick.timestamp_ns=10;
    auto result=AILLE::evaluate_unified_runtime(state, metrics, nullptr,nullptr,nullptr,nullptr,nullptr,{},nullptr,&tick);
    require(result.execution_permitted, "fresh synchronized evidence rejected");
    result=AILLE::evaluate_unified_runtime(state, metrics, nullptr,nullptr,nullptr,nullptr,nullptr,{},nullptr,&tick);
    require(!result.execution_permitted && result.hft_freeze_active, "native synchronized replay authorized");
    // Native fault state is latched. Explicit state/metrics reset is required.
    tick.tick_index=11; tick.timestamp_ns=11;
    result=AILLE::evaluate_unified_runtime(state, metrics, nullptr,nullptr,nullptr,nullptr,nullptr,{},nullptr,&tick);
    require(!result.execution_permitted, "native lock reset implicitly");
    state={}; metrics={};
    result=AILLE::evaluate_unified_runtime(state, metrics, nullptr,nullptr,nullptr,nullptr,nullptr,{},nullptr,&tick);
    require(result.execution_permitted, "explicit native reset failed");
}

void durable_audit() {
    {
        AILLE::AuditLogger audit;
        AILLE::Decision bad;
        bad.status=AILLE::DECISION_VALID;
        bad.setReasoning("A\x1fsymbol=B");
        bool rejected=false;
        try { audit.logDecision(bad,"C","test"); } catch (const std::invalid_argument&) { rejected=true; }
        require(rejected && audit.getAuditTrailSize()==0,"ambiguous canonical audit text accepted");
        bad.setReasoning("valid");
        bad.num_contributing_models=11;
        rejected=false;
        try { audit.logDecision(bad,"C","test"); } catch (const std::invalid_argument&) { rejected=true; }
        require(rejected && audit.getAuditTrailSize()==0,"unrepresentable contributor evidence truncated");
    }
    const auto path = (std::filesystem::temp_directory_path() /
        ("ailee-durable-"+std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count())+".csv")).string();
    AILLE::Decision decision;
    decision.status=AILLE::DECISION_VALID;
    decision.confidence=0.9f;
    decision.final_value=0.1f;
    {
        AILLE::AuditLogger audit(path);
        audit.logDecision(decision,"SPY","test");
        bool rejected=false;
        try { AILLE::AuditLogger concurrent(path); } catch (const std::runtime_error&) { rejected=true; }
        require(rejected,"concurrent durable writer allowed");
    }
    {
        AILLE::AuditLogger recovered(path);
        require(recovered.getAuditTrailSize()==1 && recovered.verifyIntegrity(),"audit restart lost chain");
        recovered.logDecision(decision,"SPY","test");
        decision.final_value=nan;
        bool rejected=false;
        try { recovered.logDecision(decision,"SPY","test"); } catch (const std::invalid_argument&) { rejected=true; }
        require(rejected && recovered.getAuditTrailSize()==2,"malformed audit mutated history");
    }
    {
        std::ofstream broken(path+".journal",std::ios::binary|std::ios::app);
        broken.put('x');
    }
    bool rejected=false;
    try { AILLE::AuditLogger corrupted(path); } catch (const std::runtime_error&) { rejected=true; }
    require(rejected,"torn durable record accepted on restart");
    std::filesystem::remove(path); std::filesystem::remove(path+".journal");
}

void canonical_math() {
    using namespace AILLE::MathEvidence;
    const auto result=compute_delta_v({1,1,0.1,0.5}, {{0,1,0,3,1},{1,1,0,3,1}});
    require(std::abs(result.delta_v-3*std::exp(0.075))<1e-12,"canonical numerator changed");
    require(trapezoidal_integral({{0,1e308},{1,1e308}})==1e308,"finite trapezoid mean overflowed");
    bool rejected=false;
    try { (void)compute_delta_v({1,1,0,0},{{0,1,0,1,0},{1,1,0,1,1}}); }
    catch (const MathValidationError&) { rejected=true; }
    require(rejected,"nonpositive canonical mass accepted");
    AILLE::Math::HFTSampleTick tick;
    tick.p_input=1; tick.v=1; tick.M=1; tick.dt=0.0001f;
    bool valid=false;
    const auto short_tick=AILLE::Math::calculate_hft_delta_v(1,1,0,0,&tick,1,1e-6f,&valid);
    require(valid && std::abs(short_tick-0.0001f)<1e-9,"legacy dt parity failed");
    tick.M=0;
    const auto malformed=AILLE::Math::calculate_hft_delta_v(1,1,0,0,&tick,1,1e-6f,&valid);
    require(!valid && malformed==0,"legacy invalid numeric evidence promoted");
}
}

int main(int argc, char** argv) {
    const std::pair<const char*, std::function<void()>> tests[] = {
        {"engine_config", engine_config}, {"engine_fallback", engine_fallback},
        {"engine_reconfigure", engine_reconfigure}, {"sell_governor", sell_governor},
        {"unified_evidence", unified_evidence},
        {"bound_governance", bound_governance},
        {"native_governance", native_governance}, {"unified_replay", unified_replay},
        {"durable_audit", durable_audit},
        {"canonical_math", canonical_math},
    };
    int failed = 0;
    for (const auto& [name, run] : tests) if (argc == 1 || std::string(argv[1]) == name) {
        try { run(); std::cout << name << " passed\n"; }
        catch (const std::exception& error) { ++failed; std::cerr << name << ": " << error.what() << '\n'; }
    }
    return failed ? 1 : 0;
}
