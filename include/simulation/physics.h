#pragma once
#include "simulation/vehicle.h"
#include <glm/glm.hpp>
#include <array>

// State vector: [x, y, z (km),  vx, vy, vz (km/s)]
using StateVec = std::array<double, 6>;

namespace physics {

// Gravitational acceleration (km/s²) at ECI position (two-body + J2)
glm::dvec3 gravity(const glm::dvec3& pos);

// Drag force magnitude (N) using NRLMSISE-00 density.
// t: simulation time (s) — required for Earth-rotation-based lat/lon lookup.
double dragForce(const StateVec& s, double t, const VehicleParams& vehicle);

// Equations of motion: gravity + aerodynamic drag + thrust
StateVec derivatives(const StateVec& s, double t,
                     const VehicleParams& vehicle,
                     const glm::dvec3& thrustDir = {},
                     double thrustMag = 0.0);

// RK4 integration step
StateVec rk4Step(const StateVec& s, double t, double dt,
                 const VehicleParams& vehicle,
                 const glm::dvec3& thrustDir = {},
                 double thrustMag = 0.0);

} // namespace physics
