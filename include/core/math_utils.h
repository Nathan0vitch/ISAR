#pragma once
#include <glm/glm.hpp>

namespace math {

// ── Frame transforms ──────────────────────────────────────────────────────────
// ECI position (km) → ECEF (km), θ = Greenwich Mean Sidereal Time (rad)
glm::dvec3 eciToEcef(const glm::dvec3& posEci, double gmst);

// ECEF (km) → geodetic {lat (rad), lon (rad), alt (km)}
glm::dvec3 ecefToGeodetic(const glm::dvec3& ecef);

// Geodetic {lat (rad), lon (rad), alt (km)} → ECEF (km)
glm::dvec3 geodeticToEcef(double lat, double lon, double alt);

// Greenwich Mean Sidereal Time (rad) from epoch offset (s)
double gmst(double t);

// ── Rotation matrices (right-handed, passive) ─────────────────────────────────
glm::dmat3 rotX(double angle);
glm::dmat3 rotY(double angle);
glm::dmat3 rotZ(double angle);

} // namespace math
