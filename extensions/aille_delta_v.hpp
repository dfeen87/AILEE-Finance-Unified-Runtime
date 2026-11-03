#ifndef AILLE_DELTA_V_HPP
#define AILLE_DELTA_V_HPP
// Deterministic mathematical evidence. This API has no authorization output.
#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

namespace AILLE::MathEvidence {
inline constexpr const char* FORMULA_VERSION = "ailee-delta-v/v1";
inline constexpr const char* INTEGRATION_METHOD = "composite-trapezoidal";
inline constexpr std::size_t MAX_SAMPLES = 100000;
struct MathValidationError : std::invalid_argument { using std::invalid_argument::invalid_argument; };
struct MathNumericalError : std::runtime_error { using std::runtime_error::runtime_error; };
struct DeltaVParameters { double specific_impulse, efficiency, alpha, initial_velocity; };
struct DeltaVSample { double time, input_power, workload, velocity, mass; };
struct DeltaVResult {
    double delta_v, integral_value, prefactor;
    std::size_t sample_count;
    double start_time, end_time;
    const char* integration_method = INTEGRATION_METHOD;
    const char* formula_version = FORMULA_VERSION;
};
inline void finite(double value) {
    if (!std::isfinite(value)) throw MathValidationError("mathematical input must be finite");
}
inline double exponential(double exponent) {
    if (!std::isfinite(exponent)) throw MathNumericalError("non-finite exponent");
    const double value=std::exp(exponent);
    if (!std::isfinite(value)) throw MathNumericalError("exponential overflow");
    return value;
}
inline double trapezoidal_integral(const std::vector<std::pair<double,double>>& points) {
    if (points.size()<2 || points.size()>MAX_SAMPLES) throw MathValidationError("invalid sample count");
    double total=0;
    for (std::size_t i=0; i<points.size(); ++i) {
        finite(points[i].first); finite(points[i].second);
        if (i==0) continue;
        if (points[i].first<=points[i-1].first) throw MathValidationError("times must strictly increase");
        const double duration=points[i].first-points[i-1].first;
        const double sum=points[i-1].second+points[i].second;
        const double midpoint=std::isfinite(sum)?0.5*sum:0.5*points[i-1].second+0.5*points[i].second;
        const double area=midpoint*duration;
        if (!std::isfinite(duration) || !std::isfinite(area) || !std::isfinite(total+area))
            throw MathNumericalError("non-finite integral");
        total+=area;
    }
    return total;
}
inline DeltaVResult compute_delta_v(const DeltaVParameters& parameters, const std::vector<DeltaVSample>& samples) {
    finite(parameters.specific_impulse); finite(parameters.efficiency);
    finite(parameters.alpha); finite(parameters.initial_velocity);
    if (parameters.specific_impulse<=0 || parameters.efficiency<0 || parameters.efficiency>1 || parameters.alpha<0)
        throw MathValidationError("invalid mathematical parameters");
    if (samples.size()<2 || samples.size()>MAX_SAMPLES) throw MathValidationError("invalid sample count");
    for (std::size_t i=0; i<samples.size(); ++i) {
        const auto& sample=samples[i];
        finite(sample.time); finite(sample.input_power); finite(sample.workload); finite(sample.velocity); finite(sample.mass);
        if (sample.input_power<0 || sample.mass<=0 || (i>0 && sample.time<=samples[i-1].time))
            throw MathValidationError("invalid sampled mathematical evidence");
    }
    const double v0=parameters.initial_velocity, alpha=parameters.alpha;
    const double initial_damping=exponential(-alpha*v0*v0);
    // Canonical numerator: exp(2*alpha*v0) * v(t), never exp(2*alpha*v0*v(t)).
    const double velocity_factor=exponential(2*alpha*v0);
    const double prefactor=parameters.specific_impulse*parameters.efficiency*initial_damping;
    if (!std::isfinite(prefactor)) throw MathNumericalError("non-finite prefactor");
    std::vector<std::pair<double,double>> points;
    points.reserve(samples.size());
    for (const auto& sample:samples) {
        const double damping=exponential(-alpha*sample.workload*sample.workload);
        const double value=sample.input_power*damping*velocity_factor*sample.velocity/sample.mass;
        if (!std::isfinite(value)) throw MathNumericalError("non-finite integrand");
        points.emplace_back(sample.time,value);
    }
    const double integral=trapezoidal_integral(points);
    const double result=prefactor*integral;
    if (!std::isfinite(result)) throw MathNumericalError("non-finite delta-v");
    return {result,integral,prefactor,samples.size(),samples.front().time,samples.back().time};
}
}
#endif
