#include "simulation/atmosphere.h"
#include "core/constants.h"
#include <cmath>

namespace atmosphere {

double density(double h)
{
    return constants::RHO0 * std::exp(-h / constants::H_SCALE);
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
