// Copyright (c) Don Michael Feeney Jr.
// Licensed under the MIT License.

#ifndef AILLE_FIBONACCI_HPP
#define AILLE_FIBONACCI_HPP

namespace ailee {
namespace fib {

struct RetracementLevels {
    double level_236;
    double level_382;
    double level_618;
    double level_786;
};

struct ExtensionLevels {
    double level_1272;
    double level_1618;
    double level_2618;
};

constexpr RetracementLevels compute_retracements(double high, double low) noexcept {
    double range = high - low;
    return RetracementLevels{
        high - range * 0.236,
        high - range * 0.382,
        high - range * 0.618,
        high - range * 0.786
    };
}

constexpr ExtensionLevels compute_extensions(double high, double low) noexcept {
    double range = high - low;
    return ExtensionLevels{
        high + range * 1.272,
        high + range * 1.618,
        high + range * 2.618
    };
}

constexpr double golden_ratio() noexcept {
    return 1.61803398875;
}

constexpr double project_trend(double base, double delta) noexcept {
    return base + delta * golden_ratio();
}

} // namespace fib
} // namespace ailee

#endif // AILLE_FIBONACCI_HPP
