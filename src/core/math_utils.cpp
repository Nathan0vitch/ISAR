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
    // TODO: Bowring iterative method
    (void)ecef;
    return {};
}

glm::dvec3 geodeticToEcef(double lat, double lon, double alt)
{
    // TODO
    (void)lat; (void)lon; (void)alt;
    return {};
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
