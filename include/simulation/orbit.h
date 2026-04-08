#pragma once
#include <glm/glm.hpp>
#include <vector>

struct OrbitalElements {
    double a;     // semi-major axis (km)
    double e;     // eccentricity
    double i;     // inclination (rad)
    double raan;  // right ascension of ascending node (rad)
    double omega; // argument of perigee (rad)
    double nu;    // true anomaly (rad)
};

struct CartesianState {
    glm::dvec3 pos; // km,   ECI frame
    glm::dvec3 vel; // km/s, ECI frame
};

struct OrbitalState : CartesianState {
    double raan;   // current RAAN with J2 secular drift (rad)
    double omega;  // current arg-perigee with J2 drift (rad)
    double alt;    // altitude above R_E (km)
    double speed;  // |v| (km/s)
    double time;   // mission elapsed time (s)
};

// Solve Kepler's equation  M = E − e·sin(E)  via Newton-Raphson
double solveKepler(double M, double e);

// Propagate Keplerian orbit with J2 secular perturbation
OrbitalState propagate(const OrbitalElements& elems, double t);

// Convert between state representations
CartesianState  keplerianToCartesian(const OrbitalElements& elems);
OrbitalElements cartesianToKeplerian(const CartesianState& state);

// Build the full orbit polyline for rendering (GL units, 3 floats per vertex)
std::vector<float> makeOrbitPath(const OrbitalElements& elems, double t,
                                 int steps = 360);
