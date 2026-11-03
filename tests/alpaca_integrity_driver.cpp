#define main volume_integrity_cli_main
#include "../examples/volume_auto_trader.cpp"
#undef main
#include <iostream>
#include <limits>
int main(int argc,char** argv) {
    using namespace AILLE::Plugins;
    using namespace AILLE::Plugins::Alpaca;
    if(argc!=4 || std::string(argv[1]).rfind("http://localhost:",0)!=0) return 2;
    AlpacaConfig cfg;
    cfg.base_url=argv[1]; cfg.api_key_id="synthetic-integrity-key"; cfg.secret_key="synthetic-integrity-secret";
    cfg.intent_journal_path=argv[2]; const std::string mode=argv[3];
    if(mode=="missing-journal") cfg.intent_journal_path.clear();
    if(mode=="bad-config") cfg.max_position_usd=std::numeric_limits<float>::infinity();
    if(mode=="daemon" || mode=="daemon-kill") {
        setenv("ALPACA_API_KEY_ID",cfg.api_key_id.c_str(),1); setenv("ALPACA_SECRET_KEY",cfg.secret_key.c_str(),1);
        setenv("ALPACA_BASE_URL",cfg.base_url.c_str(),1); setenv("ALPACA_INTENT_JOURNAL",cfg.intent_journal_path.c_str(),1);
        DaemonConfig daemon_cfg; daemon_cfg.mock_mode=false; daemon_cfg.enable_auto_execute=true; daemon_cfg.hysteresis_bars=1;
        daemon_cfg.audit_log_file=cfg.intent_journal_path+".audit";
        VolumeTraderDaemon daemon(daemon_cfg); AILLE::VolumeState tick;
        tick.current_volume=20000; tick.avg_volume=10000; tick.volume_anomaly_ratio=2; tick.price_change=.008f;
        daemon.processTick(tick,100); AILLE::SafetyState safety; safety.kill_switch=mode=="daemon-kill"; daemon.processTick(tick,100,&safety);
        std::cout<<"RESULT="<<daemon.brokerPending()<<":"<<static_cast<int>(daemon.confirmedPositionSide())<<'\n'; return 0;
    }
    if(mode.rfind("route-",0)==0) cfg.mock_mode=true;
    AlpacaExecution executor(cfg);
    if(mode=="locked-store") { AlpacaExecution competitor(cfg); std::cout<<"RESULT="<<competitor.isEnabled()<<'\n'; return 0; }
    if(mode.rfind("route-",0)==0) {
        AILLE::Decision decision; decision.status=AILLE::DECISION_VALID; decision.final_value=.1f; decision.confidence=.9f;
        if(mode=="route-fallback") decision.status=AILLE::FALLBACK_ACTIVATED;
        if(mode=="route-zero") decision.final_value=0;
        if(mode=="route-nan") decision.confidence=std::numeric_limits<float>::quiet_NaN();
        if(mode=="route-negative") decision.confidence=-.9f;
        std::cout<<"RESULT="<<executor.routeDecision(decision,"SPY",10)<<'\n'; return 0;
    }
    OrderRequest order{};
    order.symbol="SPY"; order.side=OrderSide::BUY; order.quantity=2; order.limit_price=100; order.client_ref="integrity-intent";
    if(mode=="sell") order.side=OrderSide::SELL;
    if(mode=="new-ref") order.client_ref="new-integrity-intent";
    if(mode=="market") order.limit_price=0;
    if(mode=="nan-quantity") order.quantity=std::numeric_limits<float>::quiet_NaN();
    if(mode=="infinite-limit") order.limit_price=std::numeric_limits<float>::infinity();
    if(mode=="too-large") order.quantity=1000;
    if(mode=="fractional") order.quantity=2.5;
    if(mode=="bad-symbol") order.symbol="SPY\"} hacked";
    if(mode=="bad-ref") order.client_ref="../unexpected";
    if(mode=="equity") { std::cout<<"RESULT="<<executor.getAccountEquity()<<'\n'; return 0; }
    if(mode=="cancel-path") { std::cout<<"RESULT="<<executor.cancelOrder("../account")<<'\n'; return 0; }
    if(mode=="flatten-path") { std::cout<<"RESULT="<<executor.flattenPosition("../account")<<'\n'; return 0; }
    std::string result=executor.submitOrder(order);
    if(mode=="repeat") result += ":"+executor.submitOrder(order);
    if(mode=="collision") { order.quantity=3; result += ":"+executor.submitOrder(order); }
    std::cout<<"RESULT="<<result<<'\n';
}
