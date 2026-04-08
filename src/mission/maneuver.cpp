#include "mission/maneuver.h"
#include "core/constants.h"
#include <cmath>

namespace maneuver {

Burn retrogradeBurn(const OrbitalState& state, double deltaV_kmps)
{
    glm::dvec3 dir = -glm::normalize(state.vel);
    return { state.time, 0.0, deltaV_kmps, dir };
}

std::pair<Burn, Burn> hohmannTransfer(double r1_km, double r2_km, double t0)
{
    // TODO: Δv₁ = √(μ/r1)·(√(2r2/(r1+r2)) − 1)
    //        Δv₂ = √(μ/r2)·(1 − √(2r1/(r1+r2)))
    (void)r1_km; (void)r2_km; (void)t0;
    return {};
}

glm::dvec3 lambert(const CartesianState& s1, const CartesianState& s2, double dt)
{
    // TODO: universal variable method (Bate, Mueller & White)
    (void)s1; (void)s2; (void)dt;
    return {};
}

double totalDeltaV(const std::vector<Burn>& burns)
{
    double total = 0.0;
    for (const auto& b : burns) total += b.deltaV;
    return total;
}

} // namespace maneuver
