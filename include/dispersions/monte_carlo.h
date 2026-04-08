#pragma once
#include "simulation/orbit.h"
#include "simulation/vehicle.h"
#include "dispersions/errors.h"
#include <vector>

struct ImpactPoint {
    double lat; // rad
    double lon; // rad
};

namespace monte_carlo {

struct RunConfig {
    int             nSamples;
    OrbitalElements orbit;
    VehicleParams   vehicle;
    DispersionModel dispersions;
    double          t_burn;  // mission time for deorbit burn (s)
    double          deltaV;  // deorbit burn magnitude (km/s)
    double          t_max;   // max integration time per run (s)
    double          dt;      // integrator time step (s)
};

// Run N Monte Carlo trajectories and return impact points
std::vector<ImpactPoint> run(const RunConfig& config);

} // namespace monte_carlo
