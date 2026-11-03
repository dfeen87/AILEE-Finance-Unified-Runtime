// Loopback-only integration driver. Never reads credentials from the environment.
#include "../ailee_plugins/plugins/execution/alpaca/AlpacaExecution.hpp"
#include <iostream>
#include <filesystem>
#include <string>

int main(int argc, char** argv) {
    const auto journal_path=std::filesystem::temp_directory_path() / ("ailee-loopback-intent-"+
        std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
    struct Cleanup { std::filesystem::path path; ~Cleanup(){std::error_code error;std::filesystem::remove(path,error);} } cleanup{journal_path};
    setenv("ALPACA_INTENT_JOURNAL",journal_path.c_str(),1);
    if (argc != 4) return 2;
    using namespace AILLE::Plugins;
    using namespace AILLE::Plugins::Alpaca;
    AlpacaConfig cfg;
    cfg.base_url = argv[1];
    // Keep this test executable incapable of targeting a remote broker.
    if (cfg.base_url.rfind("https://localhost:", 0) != 0 &&
        cfg.base_url.rfind("http://localhost:", 0) != 0) return 2;
    const std::string mode = argv[3];
    if (mode != "disabled") {
        cfg.api_key_id = "synthetic-pass-c-key";
        cfg.secret_key = "synthetic-pass-c-secret";
    }
    cfg.mock_mode = mode == "mock";
    cfg.intent_journal_path=journal_path.string();
    AlpacaExecution exec(cfg);
    if (mode == "lockout") exec.triggerLockout("local regression");
    const std::string action = argv[2];
    std::string result;
    if (action == "submit") {
        OrderRequest req{};
        req.symbol = "SPY";
        req.side = OrderSide::BUY;
        req.quantity = 2.0f;
        req.limit_price = 100.0f;
        req.client_ref = "synthetic-transport-intent";
        result = exec.submitOrder(req);
    } else if (action == "cancel") {
        result = exec.cancelOrder("local-order") ? "1" : "0";
    } else if (action == "flatten") {
        result = exec.flattenPosition("SPY") ? "1" : "0";
    } else if (action == "equity") {
        result = std::to_string(exec.getAccountEquity());
    } else {
        return 2;
    }
    std::cout << "RESULT=" << result << '\n';
}
