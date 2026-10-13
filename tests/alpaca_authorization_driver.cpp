// Synthetic credentials only. Remote URLs may be inspected but never executed.
#define main volume_daemon_cli_main
#include "../examples/volume_auto_trader.cpp"
#undef main
#include "../ailee_plugins/PluginRegistry.hpp"

// Allows the same regression source to reproduce the pre-fix baseline.
template<class T>
auto authorize(T& cfg, bool consent, int) -> decltype(cfg.live_authorized = consent, void()) {
    cfg.live_authorized = consent;
}
template<class T> void authorize(T&, bool, long) {}

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "cli") {
        const char* url = std::getenv("ALPACA_BASE_URL");
        if (!url || (std::string(url).rfind("http://localhost:", 0) != 0 &&
                     std::string(url).rfind("https://localhost:", 0) != 0)) return 2;
        setenv("ALPACA_API_KEY_ID", "synthetic-pass-d-key", 1);
        setenv("ALPACA_SECRET_KEY", "synthetic-pass-d-secret", 1);
        return volume_daemon_cli_main(argc - 1, argv + 1);
    }
    if (argc < 6) return 2;
    const std::string path = argv[1], url = argv[2], action = argv[3];
    const bool live = std::string(argv[4]) == "live";
    const std::string state = argv[5];
    const bool consent = state == "consent" || state == "lockout";
    const bool local = url.rfind("http://localhost:", 0) == 0 ||
                       url.rfind("https://localhost:", 0) == 0;
    if (action != "inspect" && !local) return 2;
    setenv("ALPACA_BASE_URL", url.c_str(), 1);
    setenv("ALPACA_API_KEY_ID", state == "disabled" || state == "missing-key" ? "" : "synthetic-pass-d-key", 1);
    setenv("ALPACA_SECRET_KEY", state == "disabled" || state == "missing-secret" ? "" : "synthetic-pass-d-secret", 1);
    std::unique_ptr<IExecutionProvider> owner;
    AlpacaExecution* exec = nullptr;
    if (path == "daemon") {
        if (!local) return 2; // Daemon construction observes equity.
        DaemonConfig cfg;
        cfg.is_live = live;
        cfg.mock_mode = state == "mock";
        authorize(cfg, consent, 0);
        cfg.audit_log_file = "local-pass-d-audit.log";
        VolumeTraderDaemon daemon(cfg);
        VolumeState tick{};
        tick.current_volume = 20000;
        tick.avg_volume = 10000;
        tick.volume_anomaly_ratio = 2;
        tick.price_change = .008f;
        daemon.processTick(tick, 500);
        daemon.processTick(tick, 500);
        std::cout << "RESULT=" << daemon.executor()->isEnabled() << '\n';
        return 0;
    } else if (path == "registry") {
        owner = PluginRegistry::instance().createExecutionProvider("alpaca");
        exec = dynamic_cast<AlpacaExecution*>(owner.get());
    } else {
        AlpacaConfig cfg = path == "env" ? AlpacaExecution::loadConfigFromEnv() : AlpacaConfig();
        if (path != "env") cfg.base_url = url;
        if (path != "env") cfg.is_live = live;
        cfg.mock_mode = state == "mock";
        cfg.api_key_id = std::getenv("ALPACA_API_KEY_ID");
        cfg.secret_key = std::getenv("ALPACA_SECRET_KEY");
        authorize(cfg, consent, 0);
        owner = std::make_unique<AlpacaExecution>(cfg);
        exec = static_cast<AlpacaExecution*>(owner.get());
    }
    if (state == "lockout") exec->triggerLockout("local regression");
    std::string result;
    if (action == "inspect") result = std::to_string(exec->isEnabled()) + ":" + std::to_string(exec->config().is_live);
    else if (action == "submit") {
        OrderRequest req{};
        req.symbol = "SPY";
        req.side = OrderSide::BUY;
        req.quantity = 2;
        result = exec->submitOrder(req);
    } else if (action == "cancel") result = exec->cancelOrder("local-order") ? "1" : "0";
    else if (action == "flatten") result = exec->flattenPosition("SPY") ? "1" : "0";
    else if (action == "equity") result = std::to_string(exec->getAccountEquity());
    else return 2;
    std::cout << "RESULT=" << result << '\n';
}
