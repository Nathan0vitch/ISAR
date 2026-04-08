#include "simulation/vehicle.h"

VehicleParams defaultCapsule()
{
    return {
        9500.0,  // mass  kg   (Crew Dragon ballpark)
        1.28,    // cd
        12.0,    // area  m²
        330.0,   // isp   s
    };
}
