#include "mission/ground_track.h"
#include "core/math_utils.h"
#include "core/constants.h"
#include <cmath>

namespace ground_track {

GeoPoint eciToGeodetic(const glm::dvec3& posEci, double t)
{
    double     theta = math::gmst(t);
    glm::dvec3 ecef  = math::eciToEcef(posEci, theta);
    double     r     = glm::length(ecef);
    return {
        std::asin(ecef.z / r),       // geocentric latitude
        std::atan2(ecef.y, ecef.x)   // longitude
    };
}

std::vector<GeoPoint> compute(const OrbitalElements& elems,
                               double t0, double duration, double dtStep)
{
    std::vector<GeoPoint> track;
    for (double t = t0; t <= t0 + duration; t += dtStep) {
        OrbitalState state = propagate(elems, t);
        track.push_back(eciToGeodetic(state.pos, t));
    }
    return track;
}

glm::dvec3 toEcef(const GeoPoint& gp, double altKm)
{
    double r = constants::R_EARTH + altKm;
    return {
        r * std::cos(gp.lat) * std::cos(gp.lon),
        r * std::cos(gp.lat) * std::sin(gp.lon),
        r * std::sin(gp.lat)
    };
}

} // namespace ground_track
