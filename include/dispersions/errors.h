#pragma once
#include <glm/glm.hpp>

struct BurnErrors {
    double biasKmps;          // systematic Δv error (km/s)
    double pointingErrorRad;  // 1-sigma pointing error (rad)
    double timingErrorSec;    // 1-sigma ignition time jitter (s)
};

struct AtmosphereErrors {
    double densityBias;         // fractional bias on ρ  (e.g. 0.05 = +5%)
    double densitySigma;        // 1-sigma fractional scatter on ρ
    double windMagnitudeSigma;  // 1-sigma horizontal wind speed (m/s)
};

struct DispersionModel {
    BurnErrors       burn;
    AtmosphereErrors atmo;
};

namespace errors {

// Representative LEO reentry dispersion model
DispersionModel defaultModel();

// Sample a burn pointing error vector (additive to nominal direction)
glm::dvec3 sampleBurnError(const BurnErrors& model, unsigned int seed = 0);

// Sample a density multiplier: returns (1 + fractional error)
double sampleDensityMultiplier(const AtmosphereErrors& model, unsigned int seed = 0);

} // namespace errors
