#include "simulation/physics.h"
#include "simulation/atmosphere.h"
#include "core/constants.h"
#include <cmath>

namespace physics {

glm::dvec3 gravity(const glm::dvec3& pos)
{
    double r2 = glm::dot(pos, pos);
    double r  = std::sqrt(r2);
    return -(constants::MU_EARTH / (r2 * r)) * pos;
}

double dragForce(const StateVec& s, const VehicleParams& vehicle)
{
    glm::dvec3 vel = { s[3], s[4], s[5] };
    double v       = glm::length(vel) * 1000.0; // km/s → m/s
    glm::dvec3 pos = { s[0], s[1], s[2] };
    double h       = glm::length(pos) - constants::R_EARTH; // km
    double rho     = atmosphere::density(h);
    return 0.5 * rho * vehicle.cd * vehicle.area * v * v;
}

StateVec derivatives(const StateVec& s, double t,
                     const VehicleParams& vehicle,
                     const glm::dvec3& thrustDir, double thrustMag)
{
    // TODO: add drag and thrust terms
    (void)t; (void)vehicle; (void)thrustDir; (void)thrustMag;

    glm::dvec3 pos = { s[0], s[1], s[2] };
    glm::dvec3 g   = gravity(pos);
    return { s[3], s[4], s[5],  g.x, g.y, g.z };
}

StateVec rk4Step(const StateVec& s, double t, double dt,
                 const VehicleParams& vehicle,
                 const glm::dvec3& thrustDir, double thrustMag)
{
    auto k1 = derivatives(s, t,           vehicle, thrustDir, thrustMag);
    StateVec s2; for (int i=0;i<6;++i) s2[i] = s[i] + 0.5*dt*k1[i];
    auto k2 = derivatives(s2, t+0.5*dt,  vehicle, thrustDir, thrustMag);
    StateVec s3; for (int i=0;i<6;++i) s3[i] = s[i] + 0.5*dt*k2[i];
    auto k3 = derivatives(s3, t+0.5*dt,  vehicle, thrustDir, thrustMag);
    StateVec s4; for (int i=0;i<6;++i) s4[i] = s[i] +    dt*k3[i];
    auto k4 = derivatives(s4, t+dt,      vehicle, thrustDir, thrustMag);

    StateVec out;
    for (int i = 0; i < 6; ++i)
        out[i] = s[i] + (dt / 6.0) * (k1[i] + 2*k2[i] + 2*k3[i] + k4[i]);
    return out;
}

} // namespace physics
