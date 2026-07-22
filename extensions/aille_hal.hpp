#ifndef AILLE_HAL_HPP
#define AILLE_HAL_HPP

#include "../aille.hpp"
#include <string>
#include <vector>
#include <memory>

namespace AILLE {
namespace HAL {

// ============================================================================
// FPGA KERNEL CONTRACTS
// ============================================================================

struct FPGAConfidenceContract {
    static constexpr int CYCLE_BUDGET = 5;
    bool advisory_only = true;
};

struct FPGAConsensusContract {
    static constexpr int CYCLE_BUDGET = 15;
    bool advisory_only = true;
};

struct FPGASafetyContract {
    static constexpr int CYCLE_BUDGET = 5;
    bool advisory_only = true;
    bool execution_capability = false; // MUST BE FALSE
};

// ============================================================================
// CONSENSUS ENGINE INTERFACE
// ============================================================================

class IConsensusEngine {
public:
    virtual ~IConsensusEngine() = default;

    // Evaluate signals and produce a final decision
    virtual Decision evaluate(const std::vector<ModelSignal>& signals, const AILLEConfig& config) = 0;

    // Returns the name of the backend
    virtual std::string backendName() const = 0;
};

// ============================================================================
// BACKEND IMPLEMENTATIONS
// ============================================================================

class CPUConsensusEngine : public IConsensusEngine {
private:
    AILLEEngine engine_;
public:
    CPUConsensusEngine() = default;

    Decision evaluate(const std::vector<ModelSignal>& signals, const AILLEConfig& config) override {
        engine_.setConfig(config);
        return engine_.makeDecision(signals.data(), signals.size());
    }

    std::string backendName() const override {
        return "CPUConsensusEngine";
    }
};

class FPGAConsensusEngine : public IConsensusEngine {
private:
    // Simulation of FPGA delegation
    AILLEEngine fallback_engine_;
public:
    FPGAConsensusEngine() = default;

    Decision evaluate(const std::vector<ModelSignal>& signals, const AILLEConfig& config) override {
        // In a real implementation, this would map to PCIe/AXI memory and invoke the FPGA kernel.
        // We simulate the output using the fallback engine for now.
        fallback_engine_.setConfig(config);
        Decision d = fallback_engine_.makeDecision(signals.data(), signals.size());
        char buf[256]; snprintf(buf, sizeof(buf), "%s [FPGA Accelerated]", d.getReasoningString()); d.setReasoning(buf);
        return d;
    }

    std::string backendName() const override {
        return "FPGAConsensusEngine";
    }
};

class SimConsensusEngine : public IConsensusEngine {
private:
    AILLEEngine engine_;
public:
    SimConsensusEngine() = default;

    Decision evaluate(const std::vector<ModelSignal>& signals, const AILLEConfig& config) override {
        engine_.setConfig(config);
        Decision d = engine_.makeDecision(signals.data(), signals.size());
        char buf[256]; snprintf(buf, sizeof(buf), "%s [Simulated]", d.getReasoningString()); d.setReasoning(buf);
        return d;
    }

    std::string backendName() const override {
        return "SimConsensusEngine";
    }
};


// ============================================================================
// NIC HOST RUNTIME (ALVEO-STYLE)
// ============================================================================

class NICHostRuntime {
private:
    std::unique_ptr<IConsensusEngine> primary_engine_;
    std::unique_ptr<IConsensusEngine> fallback_engine_;
    AILLEConfig config_;

    bool hardware_fault_ = false;
    bool kill_switch_ = false;
    bool advisory_only_ = true;
    bool human_confirmation_required_ = true;

public:
    NICHostRuntime(const AILLEConfig& config) : config_(config) {
        primary_engine_ = std::make_unique<FPGAConsensusEngine>();
        fallback_engine_ = std::make_unique<CPUConsensusEngine>();
    }

    void declareHardwareFault() noexcept {
        hardware_fault_ = true;
    }

    void engageKillSwitch() noexcept {
        kill_switch_ = true;
    }

    bool isAdvisoryOnly() const noexcept {
        return advisory_only_;
    }

    bool requiresHumanConfirmation() const noexcept {
        return human_confirmation_required_;
    }

    Decision processSignals(const std::vector<ModelSignal>& signals) {
        Decision d;
        d.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count();

        // 1. Kill Switch Check (Reduces/neutralizes advisory, never amplify risk)
        if (kill_switch_) {
            d.status = FALLBACK_ACTIVATED;
            d.final_value = 0.0f;
            d.confidence = 0.0f;
            d.fallback_used = true;
            d.setReasoning("Kill switch engaged - fallback to zero");
            return d;
        }

        // 2. Hardware Fault Check (Fail-closed behavior)
        if (hardware_fault_) {
            // Fallback to CPU to produce a safe zero position
            d = fallback_engine_->evaluate(signals, config_);

            // To ensure safe fallback/zero-position advisory, we enforce the fail-closed behavior
            // on the CPU fallback's result
            d.status = FALLBACK_ACTIVATED;
            d.final_value = 0.0f; // Force safe fallback / zero-position
            d.confidence = 0.0f;
            d.fallback_used = true;
            d.setReasoning("Hardware fault detected - fallback to CPU with zero-position advisory");
            return d;
        }

        // 3. Safety Layer Final Veto (Pre-check)
        // If signals don't pass the minimum confidence, the safety layer vetoes them before FPGA
        bool valid_signals_exist = false;
        for(const auto& sig : signals) {
             if (sig.confidence >= config_.grace_confidence_threshold) { // Need at least grace threshold
                 valid_signals_exist = true;
                 break;
             }
        }

        if (!valid_signals_exist && !signals.empty()) {
             d.status = REJECTED_LOW_CONFIDENCE;
             d.final_value = 0.0f; // Safe fallback
             d.confidence = 0.1f;
             d.fallback_used = true;
             d.setReasoning("Safety layer veto: All models failed confidence");
             return d;
        }

        // Delegate to primary engine (FPGA)
        d = primary_engine_->evaluate(signals, config_);

        // Final sanity check on execution capabilities
        if (!advisory_only_ || !human_confirmation_required_) {
             d.status = FALLBACK_ACTIVATED;
             d.final_value = 0.0f;
             d.setReasoning("Veto: Execution capability detected in advisory path");
        }

        return d;
    }
};


} // namespace HAL
} // namespace AILLE

#endif // AILLE_HAL_HPP