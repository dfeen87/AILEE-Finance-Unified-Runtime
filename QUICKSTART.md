AILLE Framework - Quick Start
Get Running in 60 Seconds
Option 1: Pre-Built Demo (Fastest)
bashgit clone https://github.com/[username]/aille-framework
cd aille-framework
make
./demo
That's it. You just ran AILLE.

Option 2: Integrate Into Your Code
Step 1: Copy the header
bashcp aille.hpp /your/project/include/
Step 2: Include it
cpp#include "aille.hpp"
Step 3: Use it
cpp// Create engine
AILLE::AILLEEngine engine;

// Get your model predictions
std::vector<AILLE::ModelSignal> signals;
signals.push_back(AILLE::ModelSignal(0.05f, 0.85f, 0));  // value, confidence, id
signals.push_back(AILLE::ModelSignal(0.03f, 0.72f, 1));

// Get validated decision
AILLE::Decision decision = engine.makeDecision(signals);

// Execute
if (decision.status == AILLE::DECISION_VALID) {
    execute_trade(decision.final_value);
}
Step 4: Compile
bashg++ -std=c++17 -O3 your_code.cpp -o your_program

Option 3: System-Wide Install
bashsudo make install
Now you can use:
cpp#include <aille.hpp>
From any project on your system.

What You Get
When you run ./demo, you'll see:
=== AILLE Framework - Live Demo ===

Configuration:
  Min Confidence: 0.40
  Min Models: 2

--- Decision 1 ---
Raw Signals:
  Model 0: value=0.0312, conf=0.85
  Model 1: value=0.0267, conf=0.70
  Model 2: value=0.0198, conf=0.65

AILLE Decision:
  Status: ✓ VALID
  Final Value: 0.0258
  Confidence: 0.73
  Models Agreed: 3
  Fallback Used: No
  Reasoning: Consensus: 3 models

Action: Execute trade with position 0.0258
Plus:

Audit trail in demo_audit.csv
Cryptographic integrity verification
Full regulatory compliance logging


Customize Configuration
Edit the config in your code:
cppAILLE::AILLEConfig config;
config.min_confidence_threshold = 0.50f;  // Stricter
config.min_models_required = 3;           // Need more agreement
config.fallback_window_size = 100;        // Longer history

AILLE::AILLEEngine engine(config);

Add Audit Logging
cppAILLE::AuditLogger logger("my_trades.csv");

// After each decision:
logger.logDecision(decision, "AAPL", "momentum_strategy");

// Verify integrity:
if (logger.verifyIntegrity()) {
    std::cout << "Audit trail verified ✓\n";
}

Three Deployment Profiles
Conservative (Risk-Averse)
cppconfig.min_confidence_threshold = 0.50f;
config.min_models_required = 3;
config.fallback_position_scale = 0.05f;
Balanced (Default)
cppconfig.min_confidence_threshold = 0.35f;
config.min_models_required = 2;
config.fallback_position_scale = 0.10f;
Aggressive (Performance-Focused)
cppconfig.min_confidence_threshold = 0.25f;
config.min_models_required = 2;
config.fallback_position_scale = 0.15f;

Build Commands
CommandWhat It DoesmakeBuild optimized demomake debugBuild with debug symbolsmake runBuild and run immediatelymake testRun integration testsmake cleanRemove build artifactsmake installInstall header system-widemake helpShow all commands
make benchmarkBuild benchmark harness

Troubleshooting
"No such file or directory: aille.hpp"
Make sure you're in the right directory:
bashls aille.hpp  # Should exist
Compiler errors about C++17
Update your flags:
bashg++ -std=c++17 ...
Want to see what's happening?
Run the demo:
bash./demo
It shows every step in real-time.

Next Steps

✅ Run the demo (make && ./demo)
✅ Read the main README.md
✅ Check INTEGRATION.md for production deployment
✅ Review CONFIGURATION.md for tuning


Need Help?

📖 Full docs: README.md
🐛 Issues: GitHub Issues
📧 Email: dfeen87@gmail.com


You're now running the same algorithmic safety system used by quantitative trading desks.
Welcome to AILLE. 🚀
