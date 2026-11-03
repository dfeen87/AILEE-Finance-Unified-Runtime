#ifndef AILLE_VALIDATION_HPP
#define AILLE_VALIDATION_HPP
#include <cmath>
#include <initializer_list>
#include <iterator>
#include <stdexcept>

namespace AILLE::validation {
inline void finite(std::initializer_list<double> values) {
    for (double value : values) if (!std::isfinite(value))
        throw std::invalid_argument("non-finite governance evidence");
}
inline void nonnegative(double value) {
    finite({value});
    if (value < 0) throw std::invalid_argument("negative governance limit");
}
inline void unit(double value) {
    nonnegative(value);
    if (value > 1) throw std::invalid_argument("governance ratio outside [0,1]");
}
template <class T, std::size_t N> void count(std::size_t value, const T (&)[N]) {
    if (value > N) throw std::invalid_argument("governance container count exceeds capacity");
}
}
#endif
