#pragma once
#include "simulation/orbit.h"
#include <vector>

struct GeoPoint {
    double lat; // rad  (geocentric latitude)
    double lon; // rad  (longitude east)
};

namespace ground_track {

// ECI position (km) → geodetic at mission time t (s)
GeoPoint eciToGeodetic(const glm::dvec3& posEci, double t);

// Ground track for a given orbit, sampled over [t0, t0+duration] with step dtStep
std::vector<GeoPoint> compute(const OrbitalElements& elems,
                               double t0, double duration, double dtStep);

// GeoPoint → ECEF (km), optionally lifted by altKm above the surface
glm::dvec3 toEcef(const GeoPoint& gp, double altKm = 0.0);

} // namespace ground_track
