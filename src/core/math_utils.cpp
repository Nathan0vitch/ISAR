#include "core/math_utils.h"
#include "core/constants.h"
#include <cmath>

namespace math {

glm::dvec3 eciToEcef(const glm::dvec3& posEci, double gmst_rad)
{
    return rotZ(-gmst_rad) * posEci;
}

glm::dvec3 ecefToGeodetic(const glm::dvec3& ecef)
{
    // WGS-84 ellipsoid parameters (km)
    const double a  = constants::R_EARTH;          // equatorial radius
    const double b  = 6356.7523142;                // polar radius
    const double e2 = 1.0 - (b * b) / (a * a);    // first eccentricity squared

    double x = ecef.x, y = ecef.y, z = ecef.z;
    double lon = std::atan2(y, x);
    double p   = std::sqrt(x * x + y * y);

    // Bowring iterative method (converges in ≤4 iterations to <0.1 mm)
    double lat = std::atan2(z, p * (1.0 - e2));
    for (int i = 0; i < 5; ++i) {
        double sinLat = std::sin(lat);
        double N = a / std::sqrt(1.0 - e2 * sinLat * sinLat);
        lat = std::atan2(z + e2 * N * sinLat, p);
    }

    double sinLat = std::sin(lat), cosLat = std::cos(lat);
    double N = a / std::sqrt(1.0 - e2 * sinLat * sinLat);
    double alt = (std::abs(cosLat) > 1e-10) ? (p / cosLat - N)
                                             : (std::abs(z) / std::abs(sinLat) - N * (1.0 - e2));

    return { lat, lon, alt };
}

glm::dvec3 geodeticToEcef(double lat, double lon, double alt)
{
    const double a  = constants::R_EARTH;
    const double b  = 6356.7523142;
    const double e2 = 1.0 - (b * b) / (a * a);

    double sinLat = std::sin(lat), cosLat = std::cos(lat);
    double N = a / std::sqrt(1.0 - e2 * sinLat * sinLat);
    return {
        (N + alt) * cosLat * std::cos(lon),
        (N + alt) * cosLat * std::sin(lon),
        (N * (1.0 - e2) + alt) * sinLat
    };
}

double gmst(double t)
{
    // Simplified: θ_GMST = ω_E · t
    return constants::OMEGA_E * t;
}

glm::dmat3 rotX(double a)
{
    double c = std::cos(a), s = std::sin(a);
    return glm::dmat3(1, 0, 0,   0, c, s,   0, -s, c);
}

glm::dmat3 rotY(double a)
{
    double c = std::cos(a), s = std::sin(a);
    return glm::dmat3(c, 0, -s,   0, 1, 0,   s, 0, c);
}

glm::dmat3 rotZ(double a)
{
    double c = std::cos(a), s = std::sin(a);
    return glm::dmat3(c, s, 0,   -s, c, 0,   0, 0, 1);
}

} // namespace math
