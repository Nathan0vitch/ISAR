#include "simulation/orbit.h"
#include "core/constants.h"
#include <cmath>
#include <algorithm>

// ── Résolution de l'équation de Kepler M = E - e·sin(E) ──────────────────────
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

// ── Éléments képlériens → position/vitesse ECI ───────────────────────────────
// Repère périfocal → ECI via la matrice de rotation R3(-Ω)·R1(-i)·R3(-ω).
CartesianState keplerianToCartesian(const OrbitalElements& el)
{
    double p = el.a * (1.0 - el.e * el.e);            // semi-latus rectum
    double r = p / (1.0 + el.e * std::cos(el.nu));    // distance radiale

    // Position et vitesse dans le repère périfocal
    double xp  = r * std::cos(el.nu);
    double yp  = r * std::sin(el.nu);

    double h   = std::sqrt(constants::MU_EARTH * p);  // moment cinétique spécifique
    double vxp = -(constants::MU_EARTH / h) * std::sin(el.nu);
    double vyp =  (constants::MU_EARTH / h) * (el.e + std::cos(el.nu));

    // Colonnes de la matrice périfocal → ECI
    double cosO = std::cos(el.raan),  sinO = std::sin(el.raan);
    double cosI = std::cos(el.i),     sinI = std::sin(el.i);
    double cosW = std::cos(el.omega), sinW = std::sin(el.omega);

    // P-hat (direction du périastre en ECI)
    glm::dvec3 Phat = {
         cosO*cosW - sinO*sinW*cosI,
         sinO*cosW + cosO*sinW*cosI,
         sinW*sinI
    };
    // Q-hat (perpendiculaire dans le plan orbital, en ECI)
    glm::dvec3 Qhat = {
        -cosO*sinW - sinO*cosW*cosI,
        -sinO*sinW + cosO*cosW*cosI,
         cosW*sinI
    };

    CartesianState st;
    st.pos = xp * Phat + yp * Qhat;
    st.vel = vxp * Phat + vyp * Qhat;
    return st;
}

// ── Position/vitesse ECI → éléments képlériens ───────────────────────────────
OrbitalElements cartesianToKeplerian(const CartesianState& state)
{
    using namespace constants;

    glm::dvec3 r_vec = state.pos;
    glm::dvec3 v_vec = state.vel;

    double r = glm::length(r_vec);
    double v = glm::length(v_vec);

    // Moment cinétique spécifique
    glm::dvec3 h_vec = glm::cross(r_vec, v_vec);
    double h = glm::length(h_vec);

    // Vecteur nœud ascendant
    glm::dvec3 N_vec = glm::cross(glm::dvec3(0, 0, 1), h_vec);
    double N = glm::length(N_vec);

    // Vecteur excentricité
    glm::dvec3 e_vec = ((v*v - MU_EARTH/r) * r_vec - glm::dot(r_vec, v_vec) * v_vec) / MU_EARTH;
    double e = glm::length(e_vec);

    // Demi-grand axe
    double a = 1.0 / (2.0/r - v*v/MU_EARTH);

    // Inclinaison
    double i = std::acos(std::min(1.0, std::max(-1.0, h_vec.z / h)));

    // RAAN
    double raan = 0.0;
    if (N > 1e-10) {
        raan = std::acos(std::min(1.0, std::max(-1.0, N_vec.x / N)));
        if (N_vec.y < 0.0) raan = TWO_PI - raan;
    }

    // Argument du périastre
    double omega = 0.0;
    if (N > 1e-10 && e > 1e-10) {
        omega = std::acos(std::min(1.0, std::max(-1.0, glm::dot(N_vec, e_vec) / (N * e))));
        if (e_vec.z < 0.0) omega = TWO_PI - omega;
    }

    // Anomalie vraie
    double nu = 0.0;
    if (e > 1e-10) {
        nu = std::acos(std::min(1.0, std::max(-1.0, glm::dot(e_vec, r_vec) / (e * r))));
        if (glm::dot(r_vec, v_vec) < 0.0) nu = TWO_PI - nu;
    }

    return { a, e, i, raan, omega, nu };
}

// ── Propagation képlérienne avec dérive séculaire J2 ─────────────────────────
OrbitalState propagate(const OrbitalElements& elems, double t)
{
    using namespace constants;

    double n = std::sqrt(MU_EARTH / (elems.a * elems.a * elems.a)); // mouvement moyen
    double p = elems.a * (1.0 - elems.e * elems.e);
    double cosI = std::cos(elems.i);

    // Vitesses de dérive séculaire dues à J2
    double k        = -1.5 * n * J2 * (R_EARTH / p) * (R_EARTH / p);
    double raanDot  = k * cosI;
    double omegaDot = -k * (0.5 - 2.5 * cosI * cosI);

    // Éléments propagés
    OrbitalElements el = elems;
    el.raan  = elems.raan  + raanDot  * t;
    el.omega = elems.omega + omegaDot * t;

    // Anomalie vraie via anomalie moyenne
    // Conversion ν₀ → M₀
    double tanHalf0 = std::sqrt((1.0 - el.e) / (1.0 + el.e + 1e-15)) * std::tan(elems.nu / 2.0);
    double E0       = 2.0 * std::atan(tanHalf0);
    double M0       = E0 - el.e * std::sin(E0);

    double M  = M0 + n * t;
    double E  = solveKepler(M, el.e);
    double tanHalf = std::sqrt((1.0 + el.e) / (1.0 - el.e + 1e-15)) * std::tan(E / 2.0);
    el.nu = 2.0 * std::atan(tanHalf);

    CartesianState cs = keplerianToCartesian(el);

    OrbitalState st;
    st.pos   = cs.pos;
    st.vel   = cs.vel;
    st.raan  = el.raan;
    st.omega = el.omega;
    st.alt   = glm::length(cs.pos) - R_EARTH;
    st.speed = glm::length(cs.vel);
    st.time  = t;
    return st;
}

// ── Tracé de l'ellipse orbitale (pour le rendu) ───────────────────────────────
// Génère `steps` points sur l'orbite képlérienne courante (en unités GL).
std::vector<float> makeOrbitPath(const OrbitalElements& elems, double t, int steps)
{
    (void)t;

    std::vector<float> pts;
    pts.reserve((steps + 1) * 3);

    float scale = (float)constants::GL_SCALE;

    for (int k = 0; k <= steps; ++k) {
        OrbitalElements el = elems;
        el.nu = constants::TWO_PI * k / steps;
        CartesianState cs = keplerianToCartesian(el);
        pts.push_back((float)cs.pos.x * scale);
        pts.push_back((float)cs.pos.y * scale);
        pts.push_back((float)cs.pos.z * scale);
    }

    return pts;
}
