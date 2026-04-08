#include "simulation/physics.h"
#include "simulation/atmosphere.h"
#include "core/constants.h"
#include <cmath>

namespace physics {

// ── Gravité deux-corps (km/s²) ────────────────────────────────────────────────
glm::dvec3 gravity(const glm::dvec3& pos)
{
    double r2 = glm::dot(pos, pos);
    double r  = std::sqrt(r2);
    return -(constants::MU_EARTH / (r2 * r)) * pos;
}

// ── Force de traînée scalaire (N) ─────────────────────────────────────────────
// Utilise la densité NRLMSISE-00 à l'altitude courante.
double dragForce(const StateVec& s, const VehicleParams& vehicle)
{
    glm::dvec3 vel = { s[3], s[4], s[5] };
    double v_ms    = glm::length(vel) * 1000.0;           // km/s → m/s
    glm::dvec3 pos = { s[0], s[1], s[2] };
    double h       = glm::length(pos) - constants::R_EARTH; // km
    double rho     = atmosphere::density(h);               // kg/m³ (NRLMSISE-00)
    return 0.5 * rho * vehicle.cd * vehicle.area * v_ms * v_ms;
}

// ── Équations du mouvement ────────────────────────────────────────────────────
// Accélération = gravité + traînée atmosphérique + poussée
// Traînée modélisée à partir de la position réelle du satellite via NRLMSISE-00.
StateVec derivatives(const StateVec& s, double t,
                     const VehicleParams& vehicle,
                     const glm::dvec3& thrustDir, double thrustMag)
{
    (void)t;

    glm::dvec3 pos = { s[0], s[1], s[2] };
    glm::dvec3 vel = { s[3], s[4], s[5] };

    // Gravité (km/s²)
    glm::dvec3 accel = gravity(pos);

    // Traînée atmosphérique — dépend de la position (altitude) du satellite
    double h = glm::length(pos) - constants::R_EARTH;  // altitude (km)
    double v = glm::length(vel);                        // vitesse (km/s)

    if (h > 0.0 && h < 1000.0 && v > 0.0) {
        double v_ms   = v * 1000.0;                                    // m/s
        double rho    = atmosphere::density(h);                        // kg/m³ (NRLMSISE-00)
        double F_N    = 0.5 * rho * vehicle.cd * vehicle.area * v_ms * v_ms; // N
        double a_kms2 = (F_N / vehicle.mass) / 1000.0;                // km/s²
        accel -= a_kms2 * glm::normalize(vel);                        // opposée à v
    }

    // Poussée
    if (thrustMag > 0.0 && vehicle.mass > 0.0) {
        double a_kms2 = (thrustMag / vehicle.mass) / 1000.0;
        accel += a_kms2 * glm::normalize(thrustDir);
    }

    return { vel.x, vel.y, vel.z, accel.x, accel.y, accel.z };
}

// ── Intégration RK4 ───────────────────────────────────────────────────────────
StateVec rk4Step(const StateVec& s, double t, double dt,
                 const VehicleParams& vehicle,
                 const glm::dvec3& thrustDir, double thrustMag)
{
    auto k1 = derivatives(s, t,          vehicle, thrustDir, thrustMag);
    StateVec s2; for (int i = 0; i < 6; ++i) s2[i] = s[i] + 0.5*dt*k1[i];
    auto k2 = derivatives(s2, t+0.5*dt, vehicle, thrustDir, thrustMag);
    StateVec s3; for (int i = 0; i < 6; ++i) s3[i] = s[i] + 0.5*dt*k2[i];
    auto k3 = derivatives(s3, t+0.5*dt, vehicle, thrustDir, thrustMag);
    StateVec s4; for (int i = 0; i < 6; ++i) s4[i] = s[i] +    dt*k3[i];
    auto k4 = derivatives(s4, t+dt,     vehicle, thrustDir, thrustMag);

    StateVec out;
    for (int i = 0; i < 6; ++i)
        out[i] = s[i] + (dt / 6.0) * (k1[i] + 2*k2[i] + 2*k3[i] + k4[i]);
    return out;
}

}  // namespace physics
