// =============================================================================
// Satellite.cpp — Calculs orbitaux et construction de la géométrie 3D
//
// Voir Satellite.h pour la documentation des interfaces.
// =============================================================================

#include "simulation/Satellite.h"

#include <glm/gtc/constants.hpp>
#include <cmath>

static constexpr float DEG2RAD = glm::pi<float>() / 180.0f;
static constexpr int   N_ORBIT = 360;   // points sur l'ellipse orbitale


// =============================================================================
// OrbitalParams::recalculate
// =============================================================================
//
// Formules :
//   r_p = R_Earth + h_perigee          (rayon périgée)
//   r_a = R_Earth + h_apogee           (rayon apogée)
//   a   = (r_p + r_a) / 2              (demi-grand axe)
//   e   = (r_a - r_p) / (r_a + r_p)   (excentricité)
//   T   = 2π √(a³ / μ)                 (période — a en mètres)
void OrbitalParams::recalculate()
{
    // Garantit périgée ≤ apogée (swap silencieux)
    if (h_perigee > h_apogee) std::swap(h_perigee, h_apogee);

    const double rp = R_EARTH_KM + static_cast<double>(h_perigee);
    const double ra = R_EARTH_KM + static_cast<double>(h_apogee);

    semi_major_axis = static_cast<float>((rp + ra) / 2.0);
    eccentricity    = static_cast<float>((ra - rp) / (ra + rp));

    const double a_m = semi_major_axis * 1000.0;   // km → m
    period_s = static_cast<float>(
        2.0 * glm::pi<double>() * std::sqrt(a_m * a_m * a_m / MU_EARTH_SI));
}


// =============================================================================
// Satellite::perifocalToECI
// =============================================================================
//
// Référentiel périfocal (P, Q, W) :
//   P : vecteur unitaire vers le périgée
//   Q : vecteur unitaire 90° en avance dans le plan orbital
//   W = P × Q : moment cinétique orbital
//
// Matrice de rotation périfocal → ECI standard (Z-up, Z = pôle Nord) :
//   r_eci = M · r_pqw   avec r_pqw = (r·cosθ, r·sinθ, 0)
//
// Colonnes de M :
//   P_eci = ( cos Ω cos ω − sin Ω sin ω cos i,
//             sin Ω cos ω + cos Ω sin ω cos i,
//             sin ω sin i )
//
//   Q_eci = (−cos Ω sin ω − sin Ω cos ω cos i,
//            −sin Ω sin ω + cos Ω cos ω cos i,
//             cos ω sin i )
//
// Conversion ECI standard (Z-up) → notre ECI (Y-up) :
//   Notre X = Std X   (vers lon 0°  équateur)
//   Notre Y = Std Z   (pôle Nord)
//   Notre Z = Std Y   (vers lon 90°E équateur)
//
// Normalisation : on divise par R_Earth [km] pour obtenir les unités scène
//   (la sphère terrestre a rayon 1 dans le moteur).
glm::vec3 Satellite::perifocalToECI(float r_km, float theta_deg) const
{
    const float O  = orbital.raan        * DEG2RAD;
    const float in = orbital.inclination * DEG2RAD;
    const float w  = orbital.arg_perigee * DEG2RAD;
    const float th = theta_deg           * DEG2RAD;

    const float cosO  = std::cos(O),  sinO  = std::sin(O);
    const float cosi  = std::cos(in), sini  = std::sin(in);
    const float cosw  = std::cos(w),  sinw  = std::sin(w);
    const float costh = std::cos(th), sinth = std::sin(th);

    // Colonnes P et Q de la matrice périfocal → ECI standard
    const float Px =  cosO * cosw - sinO * sinw * cosi;
    const float Py =  sinO * cosw + cosO * sinw * cosi;
    const float Pz =  sinw * sini;

    const float Qx = -cosO * sinw - sinO * cosw * cosi;
    const float Qy = -sinO * sinw + cosO * cosw * cosi;
    const float Qz =  cosw * sini;

    // Position ECI standard (Z-up) : r_std = r (cosθ P + sinθ Q)
    const float sx = r_km * (costh * Px + sinth * Qx);
    const float sy = r_km * (costh * Py + sinth * Qy);
    const float sz = r_km * (costh * Pz + sinth * Qz);

    // → Notre ECI (Y-up), normalisé par R_Earth
    const float R = static_cast<float>(R_EARTH_KM);
    return { sx / R,    // notre X = std X
             sz / R,    // notre Y = std Z (pôle)
             sy / R };  // notre Z = std Y
}


// =============================================================================
// Satellite::buildGeometry
// =============================================================================
void Satellite::buildGeometry()
{
    orbital.recalculate();

    const float a = orbital.semi_major_axis;
    const float e = orbital.eccentricity;
    const float p = a * (1.0f - e * e);   // semi-latus rectum [km]

    // ── Ellipse orbitale ──────────────────────────────────────────────────────
    orbitVerts.clear();
    orbitVerts.reserve(N_ORBIT * 3);
    for (int k = 0; k < N_ORBIT; ++k)
    {
        const float th  = static_cast<float>(k) * 360.0f / static_cast<float>(N_ORBIT);
        const float r   = p / (1.0f + e * std::cos(th * DEG2RAD));
        const glm::vec3 v = perifocalToECI(r, th);
        orbitVerts.push_back(v.x);
        orbitVerts.push_back(v.y);
        orbitVerts.push_back(v.z);
    }

    // ── Points caractéristiques ───────────────────────────────────────────────

    // Position courante (ν)
    {
        const float nu = orbital.true_anomaly;
        const float r  = p / (1.0f + e * std::cos(nu * DEG2RAD));
        posECI = perifocalToECI(r, nu);
    }

    // Apogée (θ = 180°)
    apECI = perifocalToECI(a * (1.0f + e), 180.0f);

    // Périgée (θ = 0°)
    peECI = perifocalToECI(a * (1.0f - e), 0.0f);

    // Nœud ascendant (argument de latitude = 0°, θ_AN = −ω)
    {
        const float th_an = -orbital.arg_perigee;
        const float r_an  = p / (1.0f + e * std::cos(th_an * DEG2RAD));
        anECI = perifocalToECI(r_an, th_an);
    }

    // Nœud descendant (argument de latitude = 180°, θ_DN = 180° − ω)
    {
        const float th_dn = 180.0f - orbital.arg_perigee;
        const float r_dn  = p / (1.0f + e * std::cos(th_dn * DEG2RAD));
        dnECI = perifocalToECI(r_dn, th_dn);
    }
}
