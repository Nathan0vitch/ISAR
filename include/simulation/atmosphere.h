#pragma once
#include <glm/glm.hpp>

namespace atmosphere {

// Density (kg/m³) at geometric altitude h (km) — exponential fallback model
double density(double h);

// Density (kg/m³) using NRLMSISE-00 empirical model.
// posEci: ECI position (km), t: simulation time (s) from epoch.
double densityNRLMSISE(const glm::dvec3& posEci, double t);

// Pressure  (Pa) at altitude h (km)
double pressure(double h);

// Temperature (K) at altitude h (km)
double temperature(double h);

// Speed of sound (m/s) at altitude h (km)
double speedOfSound(double h);

// Mach number given true airspeed (m/s) and altitude (km)
double mach(double trueAirspeedMs, double h);

} // namespace atmosphere
