// =============================================================================
// Satellite.cpp — Calculs orbitaux, propagateur J2 RK4, géométrie 3D
//
// Voir Satellite.h pour la documentation des interfaces.
//
// PROPAGATEUR J2 :
//   On intègre les équations du mouvement dans le référentiel ECI Y-up
//   (pôle Nord = +Y, plan XZ = équatorial) en km et km/s.
//
//   Accélération totale :
//     a = a_grav + a_J2
//     a_grav = -μ/r³ · r_vec
//     a_J2   = -3/2 · J2 · μ · Re² / r⁵ · [ x(1-5y²/r²),
//                                              y(3-5y²/r²),  ← Y = pôle
//                                              z(1-5y²/r²) ]
//
//   Intégration : RK4, pas fixe dt_s.
//   Stockage    : positions ECI Y-up / R_Earth dans track[].
// =============================================================================

#include "simulation/Satellite.h"

#include <glm/gtc/constants.hpp>
#include <cmath>
#include <algorithm>
#include <array>

static constexpr float  DEG2RAD_F = glm::pi<float>()  / 180.0f;
static constexpr double DEG2RAD_D = glm::pi<double>() / 180.0;
static constexpr double RAD2DEG_D = 180.0 / glm::pi<double>();
static constexpr int    N_ORBIT   = 360;   // points sur l'ellipse statique


// =============================================================================
// OrbitalParams::recalculate
// =============================================================================
void OrbitalParams::recalculate()
{
    if (h_perigee > h_apogee) std::swap(h_perigee, h_apogee);

    const double rp = R_EARTH_KM + static_cast<double>(h_perigee);
    const double ra = R_EARTH_KM + static_cast<double>(h_apogee);

    semi_major_axis = static_cast<float>((rp + ra) / 2.0);
    eccentricity    = static_cast<float>((ra - rp) / (ra + rp));

    const double a_m = semi_major_axis * 1000.0;
    period_s = static_cast<float>(
        2.0 * glm::pi<double>() * std::sqrt(a_m * a_m * a_m / MU_EARTH_SI));
}


// =============================================================================
// Satellite::perifocalToECI  (float, pour la géométrie statique)
// =============================================================================
//
// Référentiel périfocal → ECI standard (Z-up) → notre ECI (Y-up).
//   Notre X = Std X
//   Notre Y = Std Z (pôle)
//   Notre Z = Std Y
// Normalisé par R_Earth [km].
glm::vec3 Satellite::perifocalToECI(float r_km, float theta_deg) const
{
    const float O  = orbital.raan        * DEG2RAD_F;
    const float in = orbital.inclination * DEG2RAD_F;
    const float w  = orbital.arg_perigee * DEG2RAD_F;
    const float th = theta_deg           * DEG2RAD_F;

    const float cosO  = std::cos(O),  sinO  = std::sin(O);
    const float cosi  = std::cos(in), sini  = std::sin(in);
    const float cosw  = std::cos(w),  sinw  = std::sin(w);
    const float costh = std::cos(th), sinth = std::sin(th);

    // Colonnes P et Q (périfocal → ECI standard Z-up)
    const float Px =  cosO * cosw - sinO * sinw * cosi;
    const float Py =  sinO * cosw + cosO * sinw * cosi;
    const float Pz =  sinw * sini;

    const float Qx = -cosO * sinw - sinO * cosw * cosi;
    const float Qy = -sinO * sinw + cosO * cosw * cosi;
    const float Qz =  cosw * sini;

    // Position ECI standard Z-up
    const float sx = r_km * (costh * Px + sinth * Qx);
    const float sy = r_km * (costh * Py + sinth * Qy);
    const float sz = r_km * (costh * Pz + sinth * Qz);

    // → notre ECI Y-up, normalisé
    const float R = static_cast<float>(R_EARTH_KM);
    return { sx / R, sz / R, sy / R };
}


// =============================================================================
// Satellite::buildGeometry  (ellipse statique à t=0)
// =============================================================================
void Satellite::buildGeometry()
{
    orbital.recalculate();

    const float a = orbital.semi_major_axis;
    const float e = orbital.eccentricity;
    const float p = a * (1.0f - e * e);

    orbitVerts.clear();
    orbitVerts.reserve(N_ORBIT * 3);
    for (int k = 0; k < N_ORBIT; ++k)
    {
        const float th = static_cast<float>(k) * 360.0f / static_cast<float>(N_ORBIT);
        const float r  = p / (1.0f + e * std::cos(th * DEG2RAD_F));
        const glm::vec3 v = perifocalToECI(r, th);
        orbitVerts.push_back(v.x);
        orbitVerts.push_back(v.y);
        orbitVerts.push_back(v.z);
    }

    // Position courante (ν)
    {
        const float nu = orbital.true_anomaly;
        const float r  = p / (1.0f + e * std::cos(nu * DEG2RAD_F));
        posECI = perifocalToECI(r, nu);
    }

    apECI = perifocalToECI(a * (1.0f + e), 180.0f);
    peECI = perifocalToECI(a * (1.0f - e),   0.0f);

    {
        const float th_an = -orbital.arg_perigee;
        const float r_an  = p / (1.0f + e * std::cos(th_an * DEG2RAD_F));
        anECI = perifocalToECI(r_an, th_an);
    }
    {
        const float th_dn = 180.0f - orbital.arg_perigee;
        const float r_dn  = p / (1.0f + e * std::cos(th_dn * DEG2RAD_F));
        dnECI = perifocalToECI(r_dn, th_dn);
    }
}


// =============================================================================
// Satellite::propagate — Propagateur RK4 + J2 (double précision)
// =============================================================================
//
// Travaille dans notre ECI Y-up [km, km/s].
// Convertit l'état képlérien initial → cartésien, intègre N pas RK4, stocke
// les positions normalisées dans track[] et trackVerts[].
void Satellite::propagate(double duration_s, double dt_s)
{
    track.clear();
    trackVerts.clear();

    orbital.recalculate();

    // ── Constantes ────────────────────────────────────────────────────────────
    const double mu  = MU_EARTH_KM3;   // km³/s²
    const double Re  = R_EARTH_KM;     // km
    const double J2  = J2_EARTH;

    // ── Éléments orbitaux (double) ────────────────────────────────────────────
    const double a   = static_cast<double>(orbital.semi_major_axis);  // km
    const double e   = static_cast<double>(orbital.eccentricity);
    const double nu  = static_cast<double>(orbital.true_anomaly)  * DEG2RAD_D;
    const double p   = a * (1.0 - e * e);                             // semi-latus [km]

    const double O   = static_cast<double>(orbital.raan)         * DEG2RAD_D;
    const double inc = static_cast<double>(orbital.inclination)  * DEG2RAD_D;
    const double w   = static_cast<double>(orbital.arg_perigee)  * DEG2RAD_D;

    // ── Matrice périfocale → ECI standard Z-up ────────────────────────────────
    const double cosO = std::cos(O), sinO = std::sin(O);
    const double cosi = std::cos(inc), sini = std::sin(inc);
    const double cosw = std::cos(w),  sinw = std::sin(w);

    // Colonne P
    const double Px =  cosO * cosw - sinO * sinw * cosi;
    const double Py =  sinO * cosw + cosO * sinw * cosi;
    const double Pz_std =  sinw * sini;   // composante Z standard (= notre Y)

    // Colonne Q
    const double Qx = -cosO * sinw - sinO * cosw * cosi;
    const double Qy = -sinO * sinw + cosO * cosw * cosi;
    const double Qz_std =  cosw * sini;

    // ── État initial (position + vitesse en ECI standard Z-up) ───────────────
    const double cosnu = std::cos(nu), sinnu = std::sin(nu);
    const double r0     = p / (1.0 + e * cosnu);

    const double xs = r0 * (cosnu * Px + sinnu * Qx);
    const double ys = r0 * (cosnu * Py + sinnu * Qy);
    const double zs = r0 * (cosnu * Pz_std + sinnu * Qz_std);  // pôle (Z std)

    const double sqmup = std::sqrt(mu / p);
    const double vxs = sqmup * (-sinnu * Px      + (e + cosnu) * Qx);
    const double vys = sqmup * (-sinnu * Py      + (e + cosnu) * Qy);
    const double vzs = sqmup * (-sinnu * Pz_std  + (e + cosnu) * Qz_std);

    // ── Conversion ECI standard Z-up → notre ECI Y-up ────────────────────────
    //   (xo, yo, zo) = (xs, zs, ys)
    double rx = xs, ry = zs, rz = ys;   // position [km]
    double vx = vxs, vy = vzs, vz = vys; // vitesse  [km/s]

    // ── Accélération J2 dans notre ECI Y-up (Y = pôle) ───────────────────────
    //   a_x = -μ/r³ · x · [1 + 1.5·J2·(Re/r)²·(1 − 5·y²/r²)]
    //   a_y = -μ/r³ · y · [1 + 1.5·J2·(Re/r)²·(3 − 5·y²/r²)]  ← Y = pôle
    //   a_z = -μ/r³ · z · [1 + 1.5·J2·(Re/r)²·(1 − 5·y²/r²)]
    auto accel = [&](double x, double y, double z,
                     double& ax, double& ay, double& az)
    {
        const double r2    = x*x + y*y + z*z;
        const double r     = std::sqrt(r2);
        const double r3    = r2 * r;
        const double coeff = -mu / r3;
        // 1.5 · J2 · (Re/r)² — facteur J2 adimensionnel
        const double j2fac = 1.5 * J2 * (Re * Re) / r2;
        const double y2r2  = (y * y) / r2;   // (y/r)²

        ax = coeff * x * (1.0 + j2fac * (1.0 - 5.0 * y2r2));
        ay = coeff * y * (1.0 + j2fac * (3.0 - 5.0 * y2r2));
        az = coeff * z * (1.0 + j2fac * (1.0 - 5.0 * y2r2));
    };

    // ── Réserve mémoire ───────────────────────────────────────────────────────
    const int N = static_cast<int>(duration_s / dt_s);
    track.reserve(N + 1);
    trackVerts.reserve((N + 1) * 3);

    // ── Enregistre t = 0 ──────────────────────────────────────────────────────
    auto push = [&](double t) {
        track.push_back({ t, rx / Re, ry / Re, rz / Re });
        trackVerts.push_back(static_cast<float>(rx / Re));
        trackVerts.push_back(static_cast<float>(ry / Re));
        trackVerts.push_back(static_cast<float>(rz / Re));
    };
    push(0.0);

    // ── Boucle RK4 ────────────────────────────────────────────────────────────
    for (int i = 1; i <= N; ++i)
    {
        // k1
        double ax1, ay1, az1;
        accel(rx, ry, rz, ax1, ay1, az1);

        // k2 (état au milieu avec k1)
        const double h2 = dt_s * 0.5;
        double rx2 = rx + h2*vx, ry2 = ry + h2*vy, rz2 = rz + h2*vz;
        double vx2 = vx + h2*ax1, vy2 = vy + h2*ay1, vz2 = vz + h2*az1;
        double ax2, ay2, az2;
        accel(rx2, ry2, rz2, ax2, ay2, az2);

        // k3 (état au milieu avec k2)
        double rx3 = rx + h2*vx2, ry3 = ry + h2*vy2, rz3 = rz + h2*vz2;
        double vx3 = vx + h2*ax2, vy3 = vy + h2*ay2, vz3 = vz + h2*az2;
        double ax3, ay3, az3;
        accel(rx3, ry3, rz3, ax3, ay3, az3);

        // k4 (état à la fin avec k3)
        double rx4 = rx + dt_s*vx3, ry4 = ry + dt_s*vy3, rz4 = rz + dt_s*vz3;
        double vx4 = vx + dt_s*ax3, vy4 = vy + dt_s*ay3, vz4 = vz + dt_s*az3;
        double ax4, ay4, az4;
        accel(rx4, ry4, rz4, ax4, ay4, az4);

        // Mise à jour (combinaison pondérée)
        const double inv6 = dt_s / 6.0;
        rx += inv6 * (vx  + 2.0*vx2 + 2.0*vx3 + vx4);
        ry += inv6 * (vy  + 2.0*vy2 + 2.0*vy3 + vy4);
        rz += inv6 * (vz  + 2.0*vz2 + 2.0*vz3 + vz4);
        vx += inv6 * (ax1 + 2.0*ax2 + 2.0*ax3 + ax4);
        vy += inv6 * (ay1 + 2.0*ay2 + 2.0*ay3 + ay4);
        vz += inv6 * (az1 + 2.0*az2 + 2.0*az3 + az4);

        push(i * dt_s);
    }
}


// =============================================================================
// Satellite::posAtTime — Interpolation dans track[]
// =============================================================================
glm::vec3 Satellite::posAtTime(double t_s) const
{
    if (track.empty())
        return posECI;
    if (track.size() == 1)
        return { (float)track[0].x, (float)track[0].y, (float)track[0].z };

    // Pas de temps uniforme — on déduit l'index directement.
    const double dt = track[1].t - track[0].t;
    const double idx_f = t_s / dt;
    const int    i0    = static_cast<int>(idx_f);
    const int    i1    = i0 + 1;

    if (i0 <= 0)
        return { (float)track[0].x, (float)track[0].y, (float)track[0].z };
    if (i1 >= static_cast<int>(track.size())) {
        const auto& p = track.back();
        return { (float)p.x, (float)p.y, (float)p.z };
    }

    const double frac = idx_f - i0;
    const auto&  p0   = track[i0];
    const auto&  p1   = track[i1];
    return {
        (float)(p0.x + frac * (p1.x - p0.x)),
        (float)(p0.y + frac * (p1.y - p0.y)),
        (float)(p0.z + frac * (p1.z - p0.z))
    };
}


// =============================================================================
// Satellite::latLonAtTime — Latitude géocentrique + longitude ECEF
// =============================================================================
//
// Position ECI Y-up normalisée → (lat, lon_eci) → lon_ecef = lon_eci − ω_E·t
//
// Dans notre ECI Y-up :
//   lat  = arcsin(y / r)                     (y = pôle)
//   lon_eci = atan2(z, x)                    (x = 0°lon, z = 90°E)
//   lon_ecef = lon_eci − ω_E·t  [rad]       (Terre tourne vers l'est)
void Satellite::latLonAtTime(double t_s, float& lat_deg, float& lon_deg) const
{
    const glm::vec3 pos = posAtTime(t_s);

    const double x = pos.x, y = pos.y, z = pos.z;
    const double r = std::sqrt(x*x + y*y + z*z);

    lat_deg = static_cast<float>(std::asin(y / r) * RAD2DEG_D);

    // Longitude inertielle (ECI)
    const double lon_eci_rad  = std::atan2(z, x);
    // Rotation terrestre depuis t=0 (angle en radians)
    const double earth_rot    = t_s * OMEGA_EARTH;
    // Longitude ECEF
    double lon_ecef_rad = lon_eci_rad - earth_rot;

    // Normalisation dans [−π, +π]
    lon_ecef_rad = std::fmod(lon_ecef_rad + 3.0 * glm::pi<double>(),
                             2.0 * glm::pi<double>()) - glm::pi<double>();

    lon_deg = static_cast<float>(lon_ecef_rad * RAD2DEG_D);
}
