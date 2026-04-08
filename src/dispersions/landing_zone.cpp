#include "dispersions/landing_zone.h"
#include "core/constants.h"
#include <cmath>

namespace landing_zone {

SigmaEllipse compute(const std::vector<ImpactPoint>& impacts, double nSigma)
{
    // TODO:
    //   1. compute centroid (mean lat/lon)
    //   2. build 2×2 covariance matrix in local tangent plane
    //   3. eigen-decompose → semi-axes and rotation
    //   4. scale by nSigma
    (void)impacts; (void)nSigma;
    return {};
}

std::vector<float> ellipseToGL(const SigmaEllipse& ellipse, int steps)
{
    // TODO: parametric ellipse in local NED, rotate by rotAngle,
    //       project onto sphere surface, convert to GL units
    (void)ellipse; (void)steps;
    return {};
}

} // namespace landing_zone
