/*
 * AILLE Framework - Alpaca Plugin Integration Unit Tests
 *
 * Copyright (c) Don Michael Feeney Jr.
 * Licensed under the MIT License.
 */

#include "../ailee_plugins/plugins/execution/alpaca/AlpacaExecution.hpp"
#include <cassert>
#include <iostream>

void testAlpacaExecutionPluginMock() {
    using namespace AILLE::Plugins;
    using namespace AILLE::Plugins::Alpaca;

    AlpacaConfig cfg;
    cfg.mock_mode = true;
    cfg.max_position_usd = 10000.0f;
    cfg.max_daily_drawdown = 0.05f;

    AlpacaExecution exec(cfg);
    assert(exec.isEnabled());
    assert(!exec.isLockedOut());
    assert(exec.name() == "alpaca");

    OrderRequest req;
    req.symbol = "SPY";
    req.side = OrderSide::BUY;
    req.quantity = 10.0f;

    std::string order_id = exec.submitOrder(req);
    assert(!order_id.empty());
    assert(order_id.rfind("MOCK-ALPACA-", 0) == 0);

    bool cancelled = exec.cancelOrder(order_id);
    assert(cancelled);

    bool flattened = exec.flattenPosition("SPY");
    assert(flattened);

    float equity = exec.getAccountEquity();
    assert(equity == 100000.0f);

    exec.triggerLockout("Test drawdown breach");
    assert(exec.isLockedOut());

    std::string rejected_id = exec.submitOrder(req);
    assert(rejected_id.empty());
    std::cout << "[PASSED] testAlpacaExecutionPluginMock\n";
}

int main() {
    testAlpacaExecutionPluginMock();
    return 0;
}
