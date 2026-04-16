#pragma once
// =============================================================================
// Satellite.h — Données orbitales, physiques et géométrie 3D, OrbitalSim
//
// Trois structures :
//   OrbitalParams  — éléments képlériens (saisis + calculés)
//   PhysicalParams — propriétés physiques du satellite
//   Satellite      — combine les deux + géométrie GPU (orbite, marqueurs)
// =============================================================================

#include <glm/glm.hpp>
#include <vector>
#include <string>

// Constantes physiques
static constexpr double MU_EARTH_SI = 3.986004418e14;  // m³/s²  (param. gravitationnel Terre)
static constexpr double R_EARTH_KM  = 6371.0;           // km     (rayon moyen Terre)


// =============================================================================
// OrbitalParams — Éléments képlériens osculateurs
// =============================================================================
struct OrbitalParams
{
    // ── Paramètres saisis ─────────────────────────────────────────────────────
    float h_perigee    = 400.0f;   // Altitude du périgée       [km]
    float h_apogee     = 420.0f;   // Altitude de l'apogée      [km]
    float inclination  =  51.6f;   // Inclinaison (i)           [°]
    float raan         =   0.0f;   // RAAN (Ω)                  [°]
    float arg_perigee  =   0.0f;   // Argument du périgée (ω)   [°]
    float true_anomaly =   0.0f;   // Anomalie vraie initiale (ν)[°]

    // ── Paramètres calculés (lecture seule) ───────────────────────────────────
    float semi_major_axis = 0.0f;   // Demi-grand axe (a)  [km]
    float eccentricity    = 0.0f;   // Excentricité   (e)
    float period_s        = 0.0f;   // Période orbitale (T)[s]

    // Met à jour a, e, T depuis h_perigee et h_apogee.
    // À appeler dès que l'un des deux altitudes change.
    void recalculate();
};


// =============================================================================
// PhysicalParams — Propriétés physiques du satellite
// =============================================================================
struct PhysicalParams
{
    float mass              =  50.0f;   // Masse (m)                [kg]
    float area              =  0.04f;   // Surface de référence (A) [m²]
    float cd                =   2.2f;   // Coeff. de traînée (Cd)
    float ballistic_coeff   = 570.0f;   // Coeff. balistique (β)    [kg/m²]
    float parachute_finesse =   6.0f;   // Finesse parachute (L/D)
};


// =============================================================================
// Satellite — Données complètes + géométrie 3D
// =============================================================================
struct Satellite
{
    std::string    name  = "SAT-1";
    glm::vec4      color = { 1.0f, 0.85f, 0.20f, 1.0f };   // or

    OrbitalParams  orbital;
    PhysicalParams physical;

    // ── Géométrie 3D (unités scène : rayon sphère = 1 = R_Earth) ─────────────
    // Mise à jour par buildGeometry().
    std::vector<float> orbitVerts;   // GL_LINE_LOOP — triplets xyz, N_ORBIT points
    glm::vec3 posECI = {};   // Position à l'anomalie vraie ν
    glm::vec3 apECI  = {};   // Point apogée (θ = 180°)
    glm::vec3 peECI  = {};   // Point périgée (θ = 0°)
    glm::vec3 anECI  = {};   // Nœud ascendant (argument lat. = 0°)
    glm::vec3 dnECI  = {};   // Nœud descendant (argument lat. = 180°)

    // Reconstruit toute la géométrie 3D depuis orbital.
    // Appelle orbital.recalculate() en interne.
    void buildGeometry();

private:
    // Convertit un point du plan orbital (r [km], θ [°]) en coordonnées
    // ECI (Y-up, normalisé par R_Earth) utilisées par le moteur 3D.
    //
    // Transformation appliquée :
    //   1. Périfocal → ECI standard (Z-up) via matrice de rotation Ω, i, ω
    //   2. ECI standard → notre ECI (Y-up) : (x, y_std, z_std) → (x, z_std, y_std)
    //   3. Normalisation par R_Earth [km]
    glm::vec3 perifocalToECI(float r_km, float theta_deg) const;
};
