#pragma once
#include "dispersions/monte_carlo.h"
#include <vector>

struct SigmaEllipse {
    double centerLat;  // rad
    double centerLon;  // rad
    double semiMajor;  // rad (angular distance)
    double semiMinor;  // rad
    double rotAngle;   // rad, from north clockwise
};

namespace landing_zone {

// Compute n-sigma ellipse (n = 1, 2, or 3) from Monte Carlo impact cloud
SigmaEllipse compute(const std::vector<ImpactPoint>& impacts, double nSigma);

// Build a closed polyline (GL units, on the sphere surface) for rendering
std::vector<float> ellipseToGL(const SigmaEllipse& ellipse, int steps = 64);

} // namespace landing_zone
