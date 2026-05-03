#pragma once
// =============================================================================
// Satellite.h — Données orbitales, physiques et géométrie 3D, OrbitalSim
//
// Trois structures :
//   OrbitalParams  — éléments képlériens (saisis + calculés)
//   PhysicalParams — propriétés physiques du satellite
//   TrackPoint     — point de la trajectoire propagée (ECI Y-up, normalisé)
//   Satellite      — combine les deux + géométrie GPU (orbite, trajectoire J2)
// =============================================================================

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <cmath>

// ── Constantes physiques ──────────────────────────────────────────────────────
static constexpr double MU_EARTH_SI  = 3.986004418e14;  // m³/s²  (param. grav. Terre)
static constexpr double MU_EARTH_KM3 = 3.986004418e5;   // km³/s²
static constexpr double R_EARTH_KM   = 6371.0;           // km     (rayon moyen)
static constexpr double OMEGA_EARTH  = 7.2921150e-5;     // rad/s  (rotation terrestre)
static constexpr double J2_EARTH     = 1.08263e-3;       // coefficient J2 (aplatissement)
static constexpr double SIM_DURATION = 3.0 * 86400.0;   // 3 jours [s]
static constexpr double R_HILL_KM    = 924600.0;         // km     (sphère de Hill de la Terre)


// =============================================================================
// OrbitalParams — Éléments képlériens osculateurs
// =============================================================================
struct OrbitalParams
{
    // ── Paramètres saisis ─────────────────────────────────────────────────────
    float r_perigee    = static_cast<float>(R_EARTH_KM + 400.0);  // Rayon périgée r_p  [km]
    float r_apogee     = static_cast<float>(R_EARTH_KM + 420.0);  // Rayon apogée  r_a  [km]
    float inclination  =  51.6f;   // Inclinaison (i)           [°]
    float raan         =   0.0f;   // RAAN (Ω)                  [°]
    float arg_perigee  =   0.0f;   // Argument du périgée (ω)   [°]
    float true_anomaly =   0.0f;   // Anomalie vraie initiale (ν)[°]

    // ── Paramètres calculés (lecture seule) ───────────────────────────────────
    float semi_major_axis = 0.0f;   // Demi-grand axe (a)  [km]
    float eccentricity    = 0.0f;   // Excentricité   (e)
    float period_s        = 0.0f;   // Période orbitale (T)[s]

    // Met à jour a, e, T depuis r_perigee et r_apogee.
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
// TrackPoint — un point de la trajectoire propagée
// =============================================================================
//
// Stocké dans notre référentiel ECI Y-up (pôle Nord = +Y),
// normalisé par R_Earth (rayon sphère = 1 dans le moteur 3D).
//
// t     : temps écoulé depuis l'epoch [s]
// x,y,z : position ECI Y-up / R_Earth
struct TrackPoint
{
    double t;
    double x, y, z;
};


// =============================================================================
// Satellite — Données complètes + géométrie 3D
// =============================================================================
struct Satellite
{
    std::string    name  = "SAT-1";
    glm::vec4      color = { 1.0f, 0.85f, 0.20f, 1.0f };

    OrbitalParams  orbital;
    PhysicalParams physical;

    // ── Géométrie de l'ellipse (statique, au t=0) ────────────────────────────
    std::vector<float> orbitVerts;   // GL_LINE_LOOP — triplets xyz ECI Y-up
    glm::vec3 posECI = {};           // Position à ν (t=0)
    glm::vec3 apECI  = {};           // Apogée
    glm::vec3 peECI  = {};           // Périgée
    glm::vec3 anECI  = {};           // Nœud ascendant
    glm::vec3 dnECI  = {};           // Nœud descendant

    // ── Trajectoire propagée sur SIM_DURATION avec J2 ────────────────────────
    // Propagée par propagate(), rechargeable à tout moment.
    std::vector<TrackPoint> track;      // positions ECI Y-up / R_Earth + temps
    std::vector<float>      trackVerts; // xyz triplets pour GPU (GL_LINE_STRIP)

    // ── API ───────────────────────────────────────────────────────────────────

    // Construit l'ellipse statique (orbitVerts, apECI, peECI, anECI, dnECI, posECI).
    void buildGeometry();

    // Propage l'orbite sur `duration_s` secondes depuis t=0 avec un pas RK4 de
    // `dt_s` secondes, en tenant compte de la perturbation J2.
    // Remplit track[] et trackVerts[].
    void propagate(double duration_s = SIM_DURATION, double dt_s = 30.0);

    // Retourne la position ECI Y-up normalisée au temps t_s [s] par
    // interpolation linéaire dans track[].
    // Retourne posECI si track est vide.
    glm::vec3 posAtTime(double t_s) const;

    // Retourne la latitude géocentrique [°] et la longitude ECEF [°] du
    // satellite au temps t_s, en corrigeant la rotation terrestre.
    void latLonAtTime(double t_s, float& lat_deg, float& lon_deg) const;

private:
    // Convertit un point périfocal (r [km], θ [°]) → ECI Y-up normalisé.
    glm::vec3 perifocalToECI(float r_km, float theta_deg) const;
};
