#include "simulation/atmosphere.h"
#include "core/constants.h"
#include <cmath>
#include <array>

// NRLMSISE-00 est du code C pur — wrapper extern "C" obligatoire
extern "C" {
#include "nrlmsise-00.h"
}

namespace atmosphere {

// ── Table de correspondance altitude → densité (NRLMSISE-00) ──────────────────
// Précalculée au premier appel : 1001 points de 0 à 1000 km (pas 1 km).
// Conditions fixes : DOY=172 (solstice été), F10.7=150, Ap=4 (activité modérée).
// switch[0]=1 → sorties en kg/m³ (SI).
namespace {
    constexpr int LUT_N = 1001;    // 0..1000 km
    std::array<double, LUT_N> g_lut{};
    bool g_lutReady = false;

    void buildLUT()
    {
        for (int i = 0; i < LUT_N; ++i) {
            struct nrlmsise_input  inp  = {};
            struct nrlmsise_flags  flg  = {};
            struct nrlmsise_output out  = {};

            // Switch 0 = 1 → unités SI (kg/m³ pour d[5])
            flg.switches[0] = 1;
            for (int s = 1; s < 24; ++s)
                flg.switches[s] = 1;

            inp.year   = 2026;
            inp.doy    = 172;           // 21 juin (solstice)
            inp.sec    = 0.0;
            inp.alt    = (double)i;     // altitude en km
            inp.g_lat  = 0.0;
            inp.g_long = 0.0;
            inp.lst    = 12.0;          // midi solaire local
            inp.f107A  = 150.0;         // flux solaire moyen 81j
            inp.f107   = 150.0;         // flux solaire veille
            inp.ap     = 4.0;           // indice géomagnétique calme
            inp.ap_a   = nullptr;

            // gtd7d inclut l'oxygène anormal → meilleure précision pour la traînée
            gtd7d(&inp, &flg, &out);

            g_lut[i] = out.d[5];        // masse volumique totale [kg/m³]
        }
        g_lutReady = true;
    }
}  // anonymous namespace

// ── Interface publique ────────────────────────────────────────────────────────

// Densité atmosphérique (kg/m³) à l'altitude h_km.
// Utilise NRLMSISE-00 via une LUT interpolée linéairement.
double density(double h_km)
{
    if (!g_lutReady)
        buildLUT();

    if (h_km <= 0.0)   return g_lut[0];
    if (h_km >= 1000.0) return g_lut[1000];

    int    i    = static_cast<int>(h_km);
    double frac = h_km - i;
    return g_lut[i] * (1.0 - frac) + g_lut[i + 1] * frac;
}

double pressure   (double h) { (void)h; return 0.0; }
double temperature(double h) { (void)h; return 0.0; }
double speedOfSound(double h) { (void)h; return 0.0; }

double mach(double trueAirspeedMs, double h)
{
    double c = speedOfSound(h);
    return (c > 0.0) ? trueAirspeedMs / c : 0.0;
}

}  // namespace atmosphere
