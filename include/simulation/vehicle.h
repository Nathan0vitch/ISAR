#pragma once
#include <glm/glm.hpp>

struct VehicleParams {
    double mass;   // kg   (dry mass + propellant)
    double cd;     // drag coefficient
    double area;   // m²   (reference cross-section)
    double isp;    // s    (specific impulse)
};

struct VehicleState {
    glm::dvec3 pos;        // km,   ECI
    glm::dvec3 vel;        // km/s, ECI
    double     propellant; // kg    remaining
    double     time;       // s     mission elapsed time
};

// Default Crew Dragon-like capsule preset
VehicleParams defaultCapsule();
