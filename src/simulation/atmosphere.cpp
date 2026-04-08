#include "simulation/atmosphere.h"
#include "core/constants.h"
#include "core/math_utils.h"
#include <cmath>

// NRLMSISE-00 is a C library — wrap the include to avoid C++ name mangling.
extern "C" {
#include "nrlmsise-00.h"
}

namespace atmosphere {

double density(double h)
{
    return constants::RHO0 * std::exp(-h / constants::H_SCALE);
}

double densityNRLMSISE(const glm::dvec3& posEci, double t)
{
    // ── ECI → ECEF → geodetic ──────────────────────────────────────────────
    double gmst_rad         = math::gmst(t);
    glm::dvec3 ecef         = math::eciToEcef(posEci, gmst_rad);
    glm::dvec3 geo          = math::ecefToGeodetic(ecef); // {lat rad, lon rad, alt km}

    double lat_deg  = geo.x * constants::RAD2DEG;
    double lon_deg  = geo.y * constants::RAD2DEG;
    double alt_km   = geo.z;
    if (alt_km < 0.0) alt_km = 0.0;

    // ── Simulation time → day-of-year + seconds-in-day ────────────────────
    // Epoch: 2026-01-01 00:00 UT  (day 1 of year 2026)
    double t_days     = t / 86400.0;
    int    doy        = static_cast<int>(t_days) % 365 + 1;
    double sec_in_day = std::fmod(t, 86400.0);

    // ── NRLMSISE-00 flags: all effects on, SI output ───────────────────────
    nrlmsise_flags flags{};
    flags.switches[0] = 1;                          // SI units (kg/m³)
    for (int i = 1; i < 24; ++i) flags.switches[i] = 1;

    // ── NRLMSISE-00 input ──────────────────────────────────────────────────
    // Solar/geomagnetic activity: moderate quiet-Sun defaults.
    // (f107 < 80 km has negligible effect; ap < 80 km also negligible)
    nrlmsise_input input{};
    input.year    = 2026;
    input.doy     = doy;
    input.sec     = sec_in_day;
    input.alt     = alt_km;
    input.g_lat   = lat_deg;
    input.g_long  = lon_deg;
    input.lst     = sec_in_day / 3600.0 + lon_deg / 15.0;
    input.f107A   = 150.0;   // 81-day average solar flux (SFU)
    input.f107    = 150.0;   // daily solar flux for previous day
    input.ap      = 4.0;     // daily geomagnetic Ap index (quiet)
    input.ap_a    = nullptr;

    // ── Call GTD7D: total effective mass density for drag ──────────────────
    // d[5] = effective total mass density including anomalous O (>500 km)
    nrlmsise_output output{};
    gtd7d(&input, &flags, &output);

    return output.d[5]; // kg/m³
}

double pressure(double h)
{
    // TODO: piecewise standard atmosphere (ISA layers)
    (void)h;
    return 0.0;
}

double temperature(double h)
{
    // TODO: piecewise standard atmosphere (ISA layers)
    (void)h;
    return 0.0;
}

double speedOfSound(double h)
{
    // TODO: c = sqrt(γ · R_air · T(h))
    (void)h;
    return 0.0;
}

double mach(double trueAirspeedMs, double h)
{
    double c = speedOfSound(h);
    return (c > 0.0) ? trueAirspeedMs / c : 0.0;
}

} // namespace atmosphere
