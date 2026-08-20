#include "ailee_finance_governor.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>

#if __has_include(<Python.h>)
#include <Python.h>
#define HAS_PYTHON_H 1
#else
#define HAS_PYTHON_H 0
#endif

namespace ailee {

AileeFinanceGovernor::AileeFinanceGovernor() {
#if HAS_PYTHON_H
    if (!Py_IsInitialized()) {
        Py_Initialize();
        // Add current directory to sys.path
        PyRun_SimpleString("import sys; sys.path.append('.')");
    }
#endif
}

AileeFinanceGovernor::~AileeFinanceGovernor() {
    // Note: Py_Finalize() omitted to keep Python runtime available across calls if embedded
}

SellGovernanceDecisionCpp AileeFinanceGovernor::evaluateSell(const RawSellSignals& signals) {
#if HAS_PYTHON_H
    if (Py_IsInitialized()) {
        PyObject* pName = PyUnicode_DecodeFSDefault("ailee_finance.core_min");
        PyObject* pModule = PyImport_Import(pName);
        Py_XDECREF(pName);

        if (pModule != nullptr) {
            PyObject* pClass = PyObject_GetAttrString(pModule, "AileeFinanceTrustPipeline");
            if (pClass && PyCallable_Check(pClass)) {
                PyObject* pInstance = PyObject_CallObject(pClass, nullptr);
                Py_XDECREF(pClass);

                if (pInstance != nullptr) {
                    // Build signals dict
                    PyObject* pDict = PyDict_New();
                    PyDict_SetItemString(pDict, "position_size", PyFloat_FromDouble(signals.position_size));
                    PyDict_SetItemString(pDict, "volatility", PyFloat_FromDouble(signals.volatility));
                    PyDict_SetItemString(pDict, "trust_score", PyFloat_FromDouble(signals.trust_score));
                    PyDict_SetItemString(pDict, "intent_flag", signals.intent_flag ? Py_True : Py_False);
                    if (!signals.intent_reason.empty()) {
                        PyDict_SetItemString(pDict, "intent_reason", PyUnicode_FromString(signals.intent_reason.c_str()));
                    }

                    // Market dict
                    PyObject* pMarket = PyDict_New();
                    PyDict_SetItemString(pMarket, "spoofed_bids", signals.spoofed_bids ? Py_True : Py_False);
                    PyDict_SetItemString(pMarket, "bid_liquidity_drop", PyFloat_FromDouble(signals.bid_liquidity_drop));
                    PyDict_SetItemString(pMarket, "mev_detected", signals.mev_detected ? Py_True : Py_False);
                    PyDict_SetItemString(pMarket, "spread_widening", PyFloat_FromDouble(signals.spread_widening));
                    PyDict_SetItemString(pDict, "market", pMarket);
                    Py_DECREF(pMarket);

                    // Feeds list
                    PyObject* pFeeds = PyList_New(0);
                    for (const auto& feed : signals.feeds) {
                        PyObject* pFeedDict = PyDict_New();
                        PyDict_SetItemString(pFeedDict, "feed_id", PyUnicode_FromString(feed.feed_id.c_str()));
                        PyDict_SetItemString(pFeedDict, "price", PyFloat_FromDouble(feed.price));
                        PyDict_SetItemString(pFeedDict, "confidence", PyFloat_FromDouble(feed.confidence));
                        PyList_Append(pFeeds, pFeedDict);
                        Py_DECREF(pFeedDict);
                    }
                    PyDict_SetItemString(pDict, "feeds", pFeeds);
                    Py_DECREF(pFeeds);

                    // Call process_sell(dict)
                    PyObject* pDecision = PyObject_CallMethod(pInstance, "process_sell", "(O)", pDict);
                    Py_DECREF(pDict);
                    Py_DECREF(pInstance);
                    Py_DECREF(pModule);

                    if (pDecision != nullptr) {
                        SellGovernanceDecisionCpp cppDecision;
                        PyObject* pLevel = PyObject_GetAttrString(pDecision, "level");
                        PyObject* pAllowed = PyObject_GetAttrString(pDecision, "allowed_sell_amount");
                        PyObject* pTrust = PyObject_GetAttrString(pDecision, "trust_score");
                        PyObject* pManip = PyObject_GetAttrString(pDecision, "manipulation_score");
                        PyObject* pConsensus = PyObject_GetAttrString(pDecision, "consensus_score");
                        PyObject* pReason = PyObject_GetAttrString(pDecision, "reason");

                        if (pLevel) cppDecision.level = (int)PyLong_AsLong(pLevel);
                        if (pAllowed) cppDecision.allowed_sell_amount = PyFloat_AsDouble(pAllowed);
                        if (pTrust) cppDecision.trust_score = PyFloat_AsDouble(pTrust);
                        if (pManip) cppDecision.manipulation_score = PyFloat_AsDouble(pManip);
                        if (pConsensus) cppDecision.consensus_score = PyFloat_AsDouble(pConsensus);
                        if (pReason && PyUnicode_Check(pReason)) {
                            cppDecision.reason = PyUnicode_AsUTF8(pReason);
                        }

                        PyObject* pBull = PyObject_GetAttrString(pDecision, "bullish_mode_active");
                        PyObject* pPriceM = PyObject_GetAttrString(pDecision, "bullish_multiplier_price");
                        PyObject* pVolM = PyObject_GetAttrString(pDecision, "bullish_multiplier_volume");
                        PyObject* pExecS = PyObject_GetAttrString(pDecision, "bullish_execution_scale");
                        PyObject* pSellF = PyObject_GetAttrString(pDecision, "bullish_sell_ceiling_factor");

                        if (pBull) cppDecision.bullish_mode_active = PyObject_IsTrue(pBull) == 1;
                        if (pPriceM) cppDecision.bullish_multiplier_price = PyFloat_AsDouble(pPriceM);
                        if (pVolM) cppDecision.bullish_multiplier_volume = PyFloat_AsDouble(pVolM);
                        if (pExecS) cppDecision.bullish_execution_scale = PyFloat_AsDouble(pExecS);
                        if (pSellF) cppDecision.bullish_sell_ceiling_factor = PyFloat_AsDouble(pSellF);

                        Py_XDECREF(pLevel);
                        Py_XDECREF(pAllowed);
                        Py_XDECREF(pTrust);
                        Py_XDECREF(pManip);
                        Py_XDECREF(pConsensus);
                        Py_XDECREF(pReason);
                        Py_XDECREF(pBull);
                        Py_XDECREF(pPriceM);
                        Py_XDECREF(pVolM);
                        Py_XDECREF(pExecS);
                        Py_XDECREF(pSellF);
                        Py_DECREF(pDecision);

                        return cppDecision;
                    }
                } else {
                    Py_XDECREF(pClass);
                }
            }
            Py_DECREF(pModule);
        } else {
            PyErr_Clear();
        }
    }
#endif

    // Native C++ Evaluation Logic (Fallback / Standalone)
    SellGovernanceDecisionCpp decision;
    if (!signals.intent_flag || signals.position_size <= 0.0) {
        decision.level = 3;
        decision.allowed_sell_amount = std::max(0.0, signals.position_size * 0.1);
        decision.trust_score = signals.trust_score;
        decision.manipulation_score = 1.0;
        decision.consensus_score = 0.0;
        decision.reason = !signals.intent_flag ? (signals.intent_reason.empty() ? "Invalid sell intent" : signals.intent_reason)
                                                : "Non-positive position size";
        return decision;
    }

    // Trust Score
    decision.trust_score = std::clamp(signals.trust_score, 0.0, 1.0);

    // Manipulation Heuristics
    double manip = 0.0;
    if (signals.spoofed_bids) manip += 0.35;
    if (signals.bid_liquidity_drop > 0.0) manip += std::min(0.35, signals.bid_liquidity_drop * 0.5);
    if (signals.mev_detected) manip += 0.25;
    if (signals.spread_widening > 0.0) manip += std::min(0.20, signals.spread_widening * 0.4);
    decision.manipulation_score = std::clamp(manip, 0.0, 1.0);

    // Consensus Score
    if (signals.feeds.empty()) {
        decision.consensus_score = 0.0;
    } else {
        double sum_p = 0.0;
        double sum_conf = 0.0;
        for (const auto& feed : signals.feeds) {
            sum_p += feed.price;
            sum_conf += feed.confidence;
        }
        double avg_p = sum_p / signals.feeds.size();
        double avg_conf = sum_conf / signals.feeds.size();

        double var = 0.0;
        for (const auto& feed : signals.feeds) {
            var += (feed.price - avg_p) * (feed.price - avg_p);
        }
        double std_dev = std::sqrt(var / signals.feeds.size());
        double rel_std = (avg_p > 0.0) ? (std_dev / avg_p) : 0.0;
        double p_consensus = std::max(0.0, 1.0 - (rel_std * 5.0));
        decision.consensus_score = std::clamp(p_consensus * avg_conf, 0.0, 1.0);
    }

    // Governance Level Determination
    if (decision.trust_score >= 0.85 && decision.manipulation_score <= 0.20 && decision.consensus_score >= 0.80) {
        decision.level = 0;
    } else if (decision.trust_score >= 0.70 && decision.manipulation_score <= 0.40 && decision.consensus_score >= 0.60) {
        decision.level = 1;
    } else if (decision.trust_score >= 0.50 && decision.manipulation_score <= 0.60 && decision.consensus_score >= 0.40) {
        decision.level = 2;
    } else {
        decision.level = 3;
    }

    // Ceiling Cap
    double ceiling_ratio = 0.1;
    if (decision.level == 0) ceiling_ratio = 1.0;
    else if (decision.level == 1) ceiling_ratio = 0.6;
    else if (decision.level == 2) ceiling_ratio = 0.3;

    double raw_allowed = signals.position_size * ceiling_ratio;

    HFTBiasConfig cfg{};
    bool bullish_active = is_bullish_mode_allowed((float)decision.trust_score, (float)decision.manipulation_score, false, cfg);
    decision.bullish_mode_active = bullish_active;
    decision.bullish_multiplier_price = cfg.bullish_multiplier_price;
    decision.bullish_multiplier_volume = cfg.bullish_multiplier_volume;
    decision.bullish_execution_scale = cfg.bullish_execution_scale;
    decision.bullish_sell_ceiling_factor = cfg.bullish_sell_ceiling_factor;

    if (bullish_active && decision.level != 3) {
        raw_allowed *= cfg.bullish_sell_ceiling_factor;
    }

    // Grace Layer Volatility Adjustment
    double vol = std::max(0.0, signals.volatility);
    if (vol <= 0.20) {
        decision.allowed_sell_amount = raw_allowed;
    } else if (vol <= 0.50) {
        double factor = 1.0 - 0.5 * (vol - 0.20);
        decision.allowed_sell_amount = std::max(0.0, raw_allowed * factor);
    } else {
        double factor = std::max(0.20, 0.85 - 0.8 * (vol - 0.50));
        decision.allowed_sell_amount = std::max(0.0, raw_allowed * factor);
    }

    // Increased SELL sensitivity to downward manipulation
    if (decision.manipulation_score > 0.0) {
        decision.allowed_sell_amount *= std::max(0.0, 1.0 - 0.5 * decision.manipulation_score);
        if (decision.allowed_sell_amount > (signals.position_size * 0.3) && decision.consensus_score < 0.70) {
            decision.allowed_sell_amount *= std::max(0.1, decision.consensus_score);
        }
    }

    decision.allowed_sell_amount = std::max(0.0, decision.allowed_sell_amount);

    decision.reason = "SELL intent evaluated successfully via C++ governor";
    return decision;
}

} // namespace ailee
