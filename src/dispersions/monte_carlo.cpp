#include "dispersions/monte_carlo.h"

namespace monte_carlo {

std::vector<ImpactPoint> run(const RunConfig& config)
{
    // TODO: for each sample i:
    //   1. draw dispersion from errors::sampleBurnError / sampleDensityMultiplier
    //   2. apply perturbed burn at t_burn
    //   3. integrate with physics::rk4Step until altitude ≤ 0 or t > t_max
    //   4. record geodetic impact point
    (void)config;
    return {};
}

} // namespace monte_carlo
