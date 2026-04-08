#pragma once
#include "simulation/orbit.h"
#include <vector>
#include <utility>

struct Burn {
    double     t_ignition; // s    mission elapsed time
    double     duration;   // s
    double     deltaV;     // km/s magnitude
    glm::dvec3 direction;  // unit vector, ECI
};

namespace maneuver {

// Retrograde deorbit burn at current state
Burn retrogradeBurn(const OrbitalState& state, double deltaV_kmps);

// Hohmann transfer between two circular orbits (radii in km)
// Returns {first burn, second burn}
std::pair<Burn, Burn> hohmannTransfer(double r1_km, double r2_km, double t0);

// Lambert solver — required Δv vector to go from s1 to s2 in dt seconds
glm::dvec3 lambert(const CartesianState& s1, const CartesianState& s2, double dt);

// Total Δv for a burn sequence (km/s)
double totalDeltaV(const std::vector<Burn>& burns);

} // namespace maneuver
