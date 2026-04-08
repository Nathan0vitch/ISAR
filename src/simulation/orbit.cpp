#include "simulation/orbit.h"
#include "core/constants.h"
#include <cmath>

double solveKepler(double M, double e)
{
    double E = M;
    for (int i = 0; i < 100; ++i) {
        double dE = (M - E + e * std::sin(E)) / (1.0 - e * std::cos(E));
        E += dE;
        if (std::abs(dE) < 1e-13) break;
    }
    return E;
}

OrbitalState propagate(const OrbitalElements& elems, double t)
{
    // TODO: port J2-perturbed propagation from main.cpp
    (void)elems; (void)t;
    return {};
}

CartesianState keplerianToCartesian(const OrbitalElements& elems)
{
    // TODO: perifocal → ECI rotation
    (void)elems;
    return {};
}

OrbitalElements cartesianToKeplerian(const CartesianState& state)
{
    // TODO: angular momentum, eccentricity vector, node vector
    (void)state;
    return {};
}

std::vector<float> makeOrbitPath(const OrbitalElements& elems, double t, int steps)
{
    // TODO: port from main.cpp
    (void)elems; (void)t; (void)steps;
    return {};
}
