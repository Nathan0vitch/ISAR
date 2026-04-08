#pragma once

namespace atmosphere {

// Density   (kg/m³) at geometric altitude h (km) — exponential model
double density(double h);

// Pressure  (Pa) at altitude h (km)
double pressure(double h);

// Temperature (K) at altitude h (km)
double temperature(double h);

// Speed of sound (m/s) at altitude h (km)
double speedOfSound(double h);

// Mach number given true airspeed (m/s) and altitude (km)
double mach(double trueAirspeedMs, double h);

} // namespace atmosphere
