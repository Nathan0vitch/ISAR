#include "dispersions/errors.h"
#include "core/constants.h"
#include <random>

namespace errors {

DispersionModel defaultModel()
{
    return {
        { 0.001, 0.5 * constants::DEG2RAD, 1.0 },  // burn
        { 0.0,   0.10,                     20.0 },  // atmosphere
    };
}

glm::dvec3 sampleBurnError(const BurnErrors& model, unsigned int seed)
{
    // TODO: random rotation of magnitude ~ pointingErrorRad applied to burn dir
    (void)model; (void)seed;
    return {};
}

double sampleDensityMultiplier(const AtmosphereErrors& model, unsigned int seed)
{
    std::mt19937 rng(seed);
    std::normal_distribution<double> nd(1.0 + model.densityBias, model.densitySigma);
    return nd(rng);
}

} // namespace errors
