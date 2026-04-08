// ============================================================
//  BFS / ISS Orbital Simulation  –  OpenGL 3.3 + GLFW + GLM
//  Preset  : a=6780 km, e=0.0005, i=51.6°, RAAN=0, ω=0, M0=0
//  Physics : Keplerian propagation + J2 secular perturbation
//  Scale   : 1 GL unit = 1 Earth radius (R_E = 6378.137 km)
// ============================================================

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>
#include <cmath>
#include <string>
#include <sstream>
#include <iomanip>

// ─── Constants ──────────────────────────────────────────────
static constexpr double PI     = 3.14159265358979323846;
static constexpr double TWO_PI = 2.0 * PI;

static constexpr double MU_KM  = 398600.4418;   // km³/s²
static constexpr double RE_KM  = 6378.137;       // km  (Earth equatorial radius)
static constexpr double J2     = 1.08263e-3;

// BFS / ISS preset
static constexpr double ORB_A    = 6780.0;           // km  (semi-major axis)
static constexpr double ORB_E    = 0.0005;            // eccentricity
static constexpr double ORB_I    = 51.6 * PI / 180.0; // rad inclination
static constexpr double ORB_RAAN = 0.0;               // rad
static constexpr double ORB_W    = 0.0;               // rad  argument of perigee
static constexpr double ORB_M0   = 0.0;               // rad  initial mean anomaly

// Simulation timeline: 0 → 24 h
static constexpr double SIM_MAX  = 86400.0;  // s

// GL scale
static constexpr double GL_SCALE   = 1.0 / RE_KM;  // 1 unit = 1 R_E
static constexpr double OMEGA_EARTH = 7.2921150e-5;  // rad/s  (Earth rotation rate)

// ─── Window / camera state ──────────────────────────────────
static const int SCR_W = 1200, SCR_H = 800;

static float camYaw    =  30.0f;
static float camPitch  =  20.0f;
static float camRadius =  4.5f;

static bool  mouseDown  = false;
static bool  sliderDrag = false;
static float lastX = 0.f, lastY = 0.f;

// ─── Layout & map view state ────────────────────────────────
static float mapSplit        = 0.60f;  // 3D | planisphere split (mutable)
static float mapLatCenter    = 0.0f;  // rad, centre lat affiché
static float mapLatHalfSpan  = (float)(PI * 0.5); // demi-étendue lat (π/2=plein)
static float mapLonCenter    = 0.0f;  // rad, centre lon affiché
static float mapLonHalfSpan  = (float)(PI);       // demi-étendue lon (π=360°)

// LMB drag dans la carte = pan lat+lon
static bool  mapLmbDrag      = false;
static float mapPanStartLat  = 0.f;
static float mapPanStartLon  = 0.f;
static float mapPanStartPx   = 0.f;
static float mapPanStartPy   = 0.f;

// LMB drag sur le séparateur = resize split
static bool  mapSepDrag      = false;
static float mapSepStartX    = 0.f;   // cursor X au début du drag

// Cached map bounds (updated each frame, used by callbacks)
static float g_mapX0=0,g_mapY0=0,g_mapX1=0,g_mapY1=0,g_mapH2=0,g_mapW2=0;
static int   g_W=0, g_H=0;

// Paris reference point
static constexpr float PARIS_LAT =  0.853013f;  // 48.8566° in rad
static constexpr float PARIS_LON =  0.041053f;  // 2.3522°  in rad

// ─── Simulation state ───────────────────────────────────────
static double simTime   = 0.0;
static double timeScale = 60.0;   // real-time multiplier
static bool   playing   = true;
static double wallPrev  = 0.0;

// ─── Ground track display duration ──────────────────────────
// Options: ~1 orbit (92 min), ~3 orbits, ~6 orbits, 24 h
static constexpr double GT_DURATIONS[]  = { 5520.0, 16560.0, 33120.0, 86400.0 };
static constexpr const char* GT_LABELS[] = { "1 orbit", "3 orbits", "6 orbits", "24 h" };
static int gtDurIdx = 1;  // default: 3 orbits

// ─── Orbital mechanics ──────────────────────────────────────

// Solve Kepler's equation  M = E – e·sin(E)  via Newton-Raphson
static double solveKepler(double M, double e)
{
    double E = M;
    for (int i = 0; i < 100; ++i) {
        double dE = (M - E + e * std::sin(E)) / (1.0 - e * std::cos(E));
        E += dE;
        if (std::abs(dE) < 1e-13) break;
    }
    return E;
}

struct OrbState {
    glm::dvec3 pos;   // km, ECI (x=vernal equinox, z=north pole)
    glm::dvec3 vel;   // km/s, ECI
    double     Omega; // current RAAN (rad)  – used for display
    double     omega; // current arg-perigee (rad)
    double     alt;   // altitude above RE  (km)
    double     speed; // |v| (km/s)
};

static OrbState propagate(double t)
{
    // Mean motion (unperturbed)
    double n = std::sqrt(MU_KM / (ORB_A * ORB_A * ORB_A));

    // Semi-latus rectum
    double p  = ORB_A * (1.0 - ORB_E * ORB_E);
    double ci = std::cos(ORB_I);
    double si = std::sin(ORB_I);

    // J2 secular drift rates  (Vallado, Curtis)
    double k  = 1.5 * J2 * (RE_KM / p) * (RE_KM / p);   // dimensionless factor
    double dOmega = -n * k * ci;
    double domega =  n * k * 0.5 * (5.0 * ci*ci - 1.0);
    // dM ≈ n  (J2 correction to mean motion is ~1e-6 × n, negligible here)

    double Omega = ORB_RAAN + dOmega * t;
    double omega = ORB_W    + domega * t;
    double M     = ORB_M0   + n * t;

    double E  = solveKepler(M, ORB_E);
    double nu = 2.0 * std::atan2(
        std::sqrt(1.0 + ORB_E) * std::sin(E * 0.5),
        std::sqrt(1.0 - ORB_E) * std::cos(E * 0.5));

    double r = ORB_A * (1.0 - ORB_E * std::cos(E));
    double u = omega + nu;   // argument of latitude

    double cO = std::cos(Omega), sO = std::sin(Omega);
    double cu = std::cos(u),     su = std::sin(u);

    // ECI position  (perifocal → ECI via standard rotation)
    glm::dvec3 pos = {
        r * (cO*cu - sO*su*ci),
        r * (sO*cu + cO*su*ci),
        r *  su * si
    };

    // ECI velocity
    double h  = std::sqrt(MU_KM * p);
    double vr = MU_KM / h *  ORB_E * std::sin(nu);
    double vt = MU_KM / h * (1.0   + ORB_E * std::cos(nu));

    glm::dvec3 vel = {
        (vr*cu - vt*su)*cO - (vr*su + vt*cu)*sO*ci,
        (vr*cu - vt*su)*sO + (vr*su + vt*cu)*cO*ci,
        (vr*su + vt*cu) * si
    };

    double alt   = r - RE_KM;
    double speed = glm::length(vel);

    return { pos, vel, Omega, omega, alt, speed };
}

// Full orbit path at time t (360 vertices, closed strip)
static std::vector<float> makeOrbitPath(double t, int steps = 360)
{
    double n  = std::sqrt(MU_KM / (ORB_A * ORB_A * ORB_A));
    double p  = ORB_A * (1.0 - ORB_E * ORB_E);
    double ci = std::cos(ORB_I), si = std::sin(ORB_I);
    double k  = 1.5 * J2 * (RE_KM / p) * (RE_KM / p);

    double Omega = ORB_RAAN + (-n * k * ci)                        * t;
    double omega = ORB_W    + ( n * k * 0.5 * (5.0*ci*ci - 1.0))  * t;

    double cO = std::cos(Omega), sO = std::sin(Omega);

    std::vector<float> verts;
    verts.reserve((steps + 1) * 3);

    for (int k2 = 0; k2 <= steps; ++k2) {
        double nu = TWO_PI * k2 / steps;
        // nu → E (inverse)
        double E  = 2.0 * std::atan2(
            std::sqrt(1.0 - ORB_E) * std::sin(nu * 0.5),
            std::sqrt(1.0 + ORB_E) * std::cos(nu * 0.5));
        double r  = ORB_A * (1.0 - ORB_E * std::cos(E));
        double u  = omega + nu;
        double cu = std::cos(u), su = std::sin(u);

        float x = float((r * (cO*cu - sO*su*ci)) * GL_SCALE);
        float y = float((r * (sO*cu + cO*su*ci)) * GL_SCALE);
        float z = float((r *  su * si)            * GL_SCALE);
        verts.push_back(x);
        verts.push_back(y);
        verts.push_back(z);
    }
    return verts;
}

// ─── Ground track ─────────────────────────────────────────────

// ECI position (km) → geographic {lat, lon} in radians (ECEF)
static std::pair<float,float> eciToLatLon(double t, const glm::dvec3& posEci)
{
    double theta = OMEGA_EARTH * t;
    double c = std::cos(theta), s = std::sin(theta);
    double ex =  posEci.x * c + posEci.y * s;
    double ey = -posEci.x * s + posEci.y * c;
    double ez =  posEci.z;
    double r  = std::sqrt(ex*ex + ey*ey + ez*ez);
    return { (float)std::asin(ez / r), (float)std::atan2(ey, ex) };
}

// ECI position (km) → sub-satellite point in ECEF (GL units, on sphere surface)
// Applies GMST rotation: ECI→ECEF = Rz(−θ) where θ = OMEGA_EARTH * t
static glm::vec3 eciToGroundPoint(double t, const glm::dvec3& posEci)
{
    double theta = OMEGA_EARTH * t;
    double c = std::cos(theta), s = std::sin(theta);
    // Rz(-theta) applied to posEci
    double ex =  posEci.x * c + posEci.y * s;
    double ey = -posEci.x * s + posEci.y * c;
    double ez =  posEci.z;
    double r  = std::sqrt(ex*ex + ey*ey + ez*ez);
    // Project onto unit sphere, lifted slightly above surface to avoid z-fighting
    constexpr float LIFT = 1.005f;
    return glm::vec3(float(ex / r) * LIFT,
                     float(ey / r) * LIFT,
                     float(ez / r) * LIFT);
}

// Ground track from tStart to tEnd, sampled every dtStep seconds
// Returns ECEF surface points (GL units); render with Earth rotation MVP
static std::vector<float> makeGroundTrack(double tStart, double tEnd, double dtStep = 60.0)
{
    std::vector<float> verts;
    if (tEnd <= tStart) return verts;
    int n = (int)((tEnd - tStart) / dtStep) + 2;
    verts.reserve(n * 3);
    for (double t = tStart; t <= tEnd; t += dtStep) {
        OrbState  s  = propagate(t);
        glm::vec3 gp = eciToGroundPoint(t, s.pos);
        verts.push_back(gp.x);
        verts.push_back(gp.y);
        verts.push_back(gp.z);
    }
    // Exact last sample at tEnd if not already there
    if (std::fmod(tEnd - tStart, dtStep) > 1.0) {
        OrbState  s  = propagate(tEnd);
        glm::vec3 gp = eciToGroundPoint(tEnd, s.pos);
        verts.push_back(gp.x);
        verts.push_back(gp.y);
        verts.push_back(gp.z);
    }
    return verts;
}

// ─── Mesh helpers ────────────────────────────────────────────

struct LineMesh {
    std::vector<float>        verts;
    std::vector<unsigned int> idx;
};

static LineMesh makeSphere(float radius, int stacks, int slices)
{
    LineMesh m;
    for (int i = 0; i <= stacks; ++i) {
        float phi = float(PI) * i / stacks;
        for (int j = 0; j <= slices; ++j) {
            float th = float(TWO_PI) * j / slices;
            // Z = north pole, consistent with ECI frame
            m.verts.push_back(radius * std::sin(phi) * std::cos(th));
            m.verts.push_back(radius * std::sin(phi) * std::sin(th));
            m.verts.push_back(radius * std::cos(phi));
        }
    }
    for (int i = 0; i <= stacks; ++i)
        for (int j = 0; j < slices; ++j) {
            unsigned a = i*(slices+1)+j;
            m.idx.push_back(a); m.idx.push_back(a+1);
        }
    for (int i = 0; i < stacks; ++i)
        for (int j = 0; j <= slices; ++j) {
            unsigned a = i*(slices+1)+j, b = (i+1)*(slices+1)+j;
            m.idx.push_back(a); m.idx.push_back(b);
        }
    return m;
}

// ─── GL object wrappers ─────────────────────────────────────

struct GpuMesh { GLuint vao, vbo, ebo; GLsizei count; };

static GpuMesh uploadIndexed(const std::vector<float>& v,
                              const std::vector<unsigned>& idx)
{
    GpuMesh m{};
    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);
    glGenBuffers(1, &m.ebo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, v.size()*sizeof(float), v.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 idx.size()*sizeof(unsigned), idx.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    m.count = (GLsizei)idx.size();
    return m;
}

struct GpuDynamic { GLuint vao, vbo; GLsizei count; };

static GpuDynamic makeDynamic(int maxVerts3f)
{
    GpuDynamic d{};
    glGenVertexArrays(1, &d.vao);
    glGenBuffers(1, &d.vbo);
    glBindVertexArray(d.vao);
    glBindBuffer(GL_ARRAY_BUFFER, d.vbo);
    glBufferData(GL_ARRAY_BUFFER, maxVerts3f*3*sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    d.count = 0;
    return d;
}

static void updateDynamic(GpuDynamic& d, const std::vector<float>& v)
{
    glBindVertexArray(d.vao);
    glBindBuffer(GL_ARRAY_BUFFER, d.vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, v.size()*sizeof(float), v.data());
    d.count = (GLsizei)(v.size() / 3);
    glBindVertexArray(0);
}

// 2-D variant: vec2 vertices (for planisphere overlay)
static GpuDynamic makeDynamic2D(int maxVerts)
{
    GpuDynamic d{};
    glGenVertexArrays(1, &d.vao);
    glGenBuffers(1, &d.vbo);
    glBindVertexArray(d.vao);
    glBindBuffer(GL_ARRAY_BUFFER, d.vbo);
    glBufferData(GL_ARRAY_BUFFER, maxVerts*2*sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    d.count = 0;
    return d;
}

static void updateDynamic2D(GpuDynamic& d, const std::vector<float>& v)
{
    glBindVertexArray(d.vao);
    glBindBuffer(GL_ARRAY_BUFFER, d.vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, v.size()*sizeof(float), v.data());
    d.count = (GLsizei)(v.size() / 2);
    glBindVertexArray(0);
}

// ─── Shaders ─────────────────────────────────────────────────

static const char* VERT_3D = R"glsl(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
void main() { gl_Position = uMVP * vec4(aPos, 1.0); }
)glsl";

static const char* FRAG_3D = R"glsl(
#version 330 core
out vec4 FragColor;
uniform vec3 uColor;
void main() { FragColor = vec4(uColor, 1.0); }
)glsl";

// 2-D overlay: pixel coordinates → NDC
static const char* VERT_2D = R"glsl(
#version 330 core
layout(location = 0) in vec2 aPos;
uniform vec2 uScreen;
void main() {
    vec2 ndc = aPos / uScreen * 2.0 - 1.0;
    ndc.y = -ndc.y;
    gl_Position = vec4(ndc, 0.0, 1.0);
}
)glsl";

static const char* FRAG_2D = R"glsl(
#version 330 core
out vec4 FragColor;
uniform vec4 uColor;
void main() { FragColor = uColor; }
)glsl";

static GLuint compileShader(GLenum type, const char* src)
{
    GLuint id = glCreateShader(type);
    glShaderSource(id, 1, &src, nullptr);
    glCompileShader(id);
    GLint ok;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(id, 512, nullptr, log);
        std::cerr << "Shader error: " << log << "\n";
    }
    return id;
}

static GLuint createProgram(const char* vert, const char* frag)
{
    GLuint vs = compileShader(GL_VERTEX_SHADER,   vert);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, frag);
    GLuint p  = glCreateProgram();
    glAttachShader(p, vs); glAttachShader(p, fs);
    glLinkProgram(p);
    glDeleteShader(vs); glDeleteShader(fs);
    return p;
}

// ─── Stroke font ────────────────────────────────────────────
// Each glyph: x0,y0,x1,y1 line pairs in 0..1×0..1 (y=0 bottom).  -9 terminates.
static const float GF_0[] = { 0,0,1,0, 1,0,1,1, 1,1,0,1, 0,1,0,0, -9 };
static const float GF_1[] = { 0.5f,0,0.5f,1, 0.1f,0.7f,0.5f,1, 0,0,1,0, -9 };
static const float GF_2[] = { 0,1,1,1, 1,1,1,0.5f, 1,0.5f,0,0.5f, 0,0.5f,0,0, 0,0,1,0, -9 };
static const float GF_3[] = { 0,1,1,1, 1,1,1,0, 0,0,1,0, 0.1f,0.5f,1,0.5f, -9 };
static const float GF_6[] = { 1,1,0,1, 0,1,0,0, 0,0,1,0, 1,0,1,0.5f, 1,0.5f,0,0.5f, -9 };
static const float GF_8[] = { 0,0,1,0, 1,0,1,1, 1,1,0,1, 0,1,0,0, 0,0.5f,1,0.5f, -9 };
static const float GF_9[] = { 0,0.5f,0,1, 0,1,1,1, 1,1,1,0, 0,0.5f,1,0.5f, 0,0,1,0, -9 };
static const float GF_DEG[]= { 0.5f,0.62f,0.95f,0.62f, 0.95f,0.62f,0.95f,1, 0.95f,1,0.5f,1, 0.5f,1,0.5f,0.62f, -9 };
static const float GF_E[]  = { 0,0,0,1, 0,1,1,1, 0,0.5f,0.8f,0.5f, 0,0,1,0, -9 };
static const float GF_W[]  = { 0,1,0.25f,0, 0.25f,0,0.5f,0.55f, 0.5f,0.55f,0.75f,0, 0.75f,0,1,1, -9 };
static const float GF_N[]  = { 0,0,0,1, 0,1,1,0, 1,0,1,1, -9 };
static const float GF_S[]  = { 1,1,0,1, 0,1,0,0.5f, 0,0.5f,1,0.5f, 1,0.5f,1,0, 1,0,0,0, -9 };

static const float* glyphFor(char c) {
    switch (c) {
        case '0': return GF_0; case '1': return GF_1; case '2': return GF_2;
        case '3': return GF_3; case '6': return GF_6; case '8': return GF_8;
        case '9': return GF_9; case '@': return GF_DEG; // @ = degree sign
        case 'E': return GF_E; case 'W': return GF_W;
        case 'N': return GF_N; case 'S': return GF_S;
        default:  return nullptr;
    }
}

// Push a single glyph at (px,py) top-left corner, size (cw×ch) pixels.
static void pushGlyph(const float* g, float px, float py, float cw, float ch,
                      std::vector<float>& out) {
    if (!g) return;
    for (int i = 0; g[i] != -9.f; i += 4) {
        out.push_back(px + g[i+0]*cw);
        out.push_back(py + (1.f - g[i+1])*ch); // flip y (screen y down)
        out.push_back(px + g[i+2]*cw);
        out.push_back(py + (1.f - g[i+3])*ch);
    }
}

// Push a string of glyphs at (px,py) top-left. Returns total width used.
static float pushText(const char* str, float px, float py, float cw, float ch,
                      std::vector<float>& out) {
    float x = px;
    for (const char* p = str; *p; ++p) {
        pushGlyph(glyphFor(*p), x, py, cw, ch, out);
        x += cw + 1.f;
    }
    return x - px;
}

// String pixel width (for centering / right-align)
static float textWidth(const char* str, float cw) {
    int n = (int)std::strlen(str);
    return n > 0 ? n*cw + (n-1)*1.f : 0.f;
}

// ─── Map coordinate helpers ──────────────────────────────────

// Normalise lon dans [-π,π]
static float normLon(float lon) {
    lon = std::fmod(lon + (float)PI, (float)TWO_PI);
    if (lon < 0.f) lon += (float)TWO_PI;
    return lon - (float)PI;
}

static float lonToMapX(float lon, float x0, float mapW,
                       float lonCenter, float lonHalfSpan) {
    // frac : 0 = bord gauche (lonCenter-lonHalfSpan), 1 = bord droit
    float lo  = normLon(lon);
    // Décalage par rapport au centre, ramené dans [-π,π]
    float d   = normLon(lo - lonCenter);
    float frac = (d + lonHalfSpan) / (2.f * lonHalfSpan);
    return x0 + frac * mapW;
}

static float latToMapY(float lat, float y0, float mapH2,
                       float latCenter, float latHalfSpan) {
    return y0 + (latCenter + latHalfSpan - lat) / (2.f * latHalfSpan) * mapH2;
}

// ─── Planisphere geometry helpers ────────────────────────────

// Lat/lon grid as GL_LINES pixel data, zoom-aware (lat + lon)
static std::vector<float> makeMapGrid(float x0, float y0, float x1, float y1,
                                      float latCenter, float latHalfSpan,
                                      float lonCenter, float lonHalfSpan)
{
    float w = x1 - x0, h = y1 - y0;
    std::vector<float> v;
    // Adaptive interval
    float minSpan = std::min(latHalfSpan, lonHalfSpan);
    int step = 60;
    if (minSpan < (float)(PI/6))  step = 30;
    if (minSpan < (float)(PI/12)) step = 10;
    if (minSpan < (float)(PI/36)) step =  5;
    float latMinDeg = (latCenter - latHalfSpan) * (float)(180.0/PI);
    float latMaxDeg = (latCenter + latHalfSpan) * (float)(180.0/PI);
    float lonMinDeg = (lonCenter - lonHalfSpan) * (float)(180.0/PI);
    float lonMaxDeg = (lonCenter + lonHalfSpan) * (float)(180.0/PI);
    for (int dlat = -90; dlat <= 90; dlat += step) {
        if (dlat < latMinDeg - step || dlat > latMaxDeg + step) continue;
        float py = latToMapY((float)(dlat*(PI/180.0)), y0, h, latCenter, latHalfSpan);
        if (py < y0 - 1.f || py > y1 + 1.f) continue;
        v.push_back(x0); v.push_back(py);
        v.push_back(x1); v.push_back(py);
    }
    for (int dlon = -180; dlon <= 180; dlon += step) {
        if (dlon < lonMinDeg - step || dlon > lonMaxDeg + step) continue;
        float px = lonToMapX((float)(dlon*(PI/180.0)), x0, w, lonCenter, lonHalfSpan);
        if (px < x0 - 1.f || px > x1 + 1.f) continue;
        v.push_back(px); v.push_back(y0);
        v.push_back(px); v.push_back(y1);
    }
    return v;
}

// Ground track on planisphere as GL_LINES pixel data, zoom lat+lon aware
static std::vector<float> makeMapTrack(
    double tStart, double tEnd, double dtStep,
    float x0, float y0, float x1, float y1,
    float latCenter, float latHalfSpan,
    float lonCenter, float lonHalfSpan)
{
    float w = x1 - x0, h = y1 - y0;
    std::vector<float> v;
    if (tEnd <= tStart) return v;
    float prevLat = 0.f, prevLon = 0.f;
    bool first = true;
    for (double t = tStart; t <= tEnd + dtStep * 0.5; t += dtStep) {
        double tc = std::min(t, tEnd);
        OrbState s = propagate(tc);
        auto [lat, lon] = eciToLatLon(tc, s.pos);
        if (!first) {
            float py0 = latToMapY(prevLat, y0, h, latCenter, latHalfSpan);
            float py1 = latToMapY(lat,     y0, h, latCenter, latHalfSpan);
            float px0 = lonToMapX(prevLon, x0, w, lonCenter, lonHalfSpan);
            float px1 = lonToMapX(lon,     x0, w, lonCenter, lonHalfSpan);
            // Skip if the segment jumps across the window edge (antimeridian wrap).
            // A real orbital step can never span more than ~half the visible width.
            if (std::abs(px1 - px0) < w * 0.5f) {
                v.push_back(px0); v.push_back(py0);
                v.push_back(px1); v.push_back(py1);
            }
        }
        prevLat = lat; prevLon = lon; first = false;
        if (tc == tEnd) break;
    }
    return v;
}

// Lat/lon labels as GL_LINES (stroke font), zoom lat+lon aware
static std::vector<float> makeMapLabels(float x0, float y0, float x1, float y1,
                                        float latCenter, float latHalfSpan,
                                        float lonCenter, float lonHalfSpan)
{
    float w = x1 - x0, h = y1 - y0;
    float cw = std::max(7.f, std::min(10.f, h * 0.025f));
    float ch = cw * 1.6f;
    std::vector<float> v;

    float minSpan = std::min(latHalfSpan, lonHalfSpan);
    int step = 60;
    if (minSpan < (float)(PI/6))  step = 30;
    if (minSpan < (float)(PI/12)) step = 10;
    if (minSpan < (float)(PI/36)) step =  5;

    float latMinDeg = (latCenter - latHalfSpan) * (float)(180.0/PI);
    float latMaxDeg = (latCenter + latHalfSpan) * (float)(180.0/PI);
    float lonMinDeg = (lonCenter - lonHalfSpan) * (float)(180.0/PI);
    float lonMaxDeg = (lonCenter + lonHalfSpan) * (float)(180.0/PI);

    // ── Latitude labels (inside right edge) ──
    for (int dlat = -90; dlat <= 90; dlat += step) {
        if (dlat < latMinDeg - step || dlat > latMaxDeg + step) continue;
        float py = latToMapY((float)(dlat*(PI/180.0)), y0, h, latCenter, latHalfSpan);
        if (py < y0 + 2.f || py > y1 - ch) continue;
        char buf[10];
        if      (dlat > 0) std::snprintf(buf, sizeof(buf), "%d@N", dlat);
        else if (dlat < 0) std::snprintf(buf, sizeof(buf), "%d@S", -dlat);
        else               std::snprintf(buf, sizeof(buf), "0@");
        float tw = textWidth(buf, cw);
        pushText(buf, x1 - tw - 4.f, py - ch * 0.5f, cw, ch, v);
    }

    // ── Longitude labels (below map) ──
    for (int dlon = -180; dlon <= 180; dlon += step) {
        if (dlon == -180) continue;
        if (dlon < lonMinDeg - step || dlon > lonMaxDeg + step) continue;
        float lon_rad = (float)(dlon * (PI/180.0));
        float px = lonToMapX(lon_rad, x0, w, lonCenter, lonHalfSpan);
        if (px < x0 || px > x1) continue;
        char buf[10];
        if      (dlon > 0 && dlon < 180) std::snprintf(buf, sizeof(buf), "%d@E", dlon);
        else if (dlon < 0)               std::snprintf(buf, sizeof(buf), "%d@W", -dlon);
        else                             std::snprintf(buf, sizeof(buf), "%d@", std::abs(dlon));
        float tw = textWidth(buf, cw);
        pushText(buf, px - tw * 0.5f, y1 + 4.f, cw, ch, v);
    }
    return v;
}

// Paris hexagon on planisphere (GL_LINES)
static std::vector<float> makeParisHexMap(float x0, float y0, float x1, float y1,
                                          float latCenter, float latHalfSpan,
                                          float lonCenter, float lonHalfSpan)
{
    float w = x1-x0, h = y1-y0;
    float cx = lonToMapX(PARIS_LON, x0, w, lonCenter, lonHalfSpan);
    float cy = latToMapY(PARIS_LAT, y0, h, latCenter, latHalfSpan);
    if (cy < y0 - 20.f || cy > y1 + 20.f) return {};
    float r = 7.f;
    std::vector<float> v;
    for (int i = 0; i < 6; ++i) {
        float a0 = (i    ) * (float)(PI/3.0);
        float a1 = (i + 1) * (float)(PI/3.0);
        v.push_back(cx + r*std::cos(a0)); v.push_back(cy + r*std::sin(a0));
        v.push_back(cx + r*std::cos(a1)); v.push_back(cy + r*std::sin(a1));
    }
    return v;
}

// Paris hexagon on globe (3D, ECEF frame, GL_LINES)
static std::vector<float> makeParisHexGlobe(float r = 0.035f)
{
    float clat = std::cos(PARIS_LAT), slat = std::sin(PARIS_LAT);
    float clon = std::cos(PARIS_LON), slon = std::sin(PARIS_LON);
    float lift = 1.008f;
    float cx = clat*clon*lift, cy = clat*slon*lift, cz = slat*lift;
    float east[3]  = { -slon,       clon,      0.f   };
    float north[3] = { -slat*clon, -slat*slon, clat  };
    float vx[6], vy[6], vz[6];
    for (int i = 0; i < 6; ++i) {
        float a = i * (float)(PI/3.0);
        float ca = std::cos(a), sa = std::sin(a);
        vx[i] = cx + r*(ca*east[0] + sa*north[0]);
        vy[i] = cy + r*(ca*east[1] + sa*north[1]);
        vz[i] = cz + r*(ca*east[2] + sa*north[2]);
    }
    std::vector<float> v;
    for (int i = 0; i < 6; ++i) {
        int j = (i+1)%6;
        v.push_back(vx[i]); v.push_back(vy[i]); v.push_back(vz[i]);
        v.push_back(vx[j]); v.push_back(vy[j]); v.push_back(vz[j]);
    }
    return v;
}

// ─── Scale bar ───────────────────────────────────────────────
// Returns GL_LINES + GL_POINTS geometry + text geometry separately.
// distPerPx : km per pixel at the centre latitude.
static void makeScaleBar(float bx, float by, float mapW,
                         float lonHalfSpan, float latCenter,
                         std::vector<float>& lines, std::vector<float>& textV)
{
    // km per pixel at the visible lat centre (equirectangular)
    float kmPerPx = (float)(2.0 * lonHalfSpan * 6371.0 * std::cos((double)latCenter))
                    / mapW;
    if (kmPerPx <= 0.f) return;
    // Max bar = 120 px → choose a nice round distance
    static const float niceKm[] = { 50,100,200,500,1000,2000,5000,10000,20000 };
    float targetKm = kmPerPx * 120.f;
    float barKm = niceKm[0];
    for (float v : niceKm) if (v < targetKm) barKm = v;
    float barPx = barKm / kmPerPx;
    float bx1 = bx + barPx;
    float tickH = 5.f;
    // Bar line
    lines.push_back(bx);  lines.push_back(by);
    lines.push_back(bx1); lines.push_back(by);
    // Left tick
    lines.push_back(bx); lines.push_back(by - tickH);
    lines.push_back(bx); lines.push_back(by + tickH);
    // Right tick
    lines.push_back(bx1); lines.push_back(by - tickH);
    lines.push_back(bx1); lines.push_back(by + tickH);
    // Label "XXXXX km"
    char buf[16];
    if (barKm >= 1000.f) std::snprintf(buf, sizeof(buf), "%d000 km", (int)(barKm/1000));
    else                 std::snprintf(buf, sizeof(buf), "%d km",    (int)barKm);
    float cw = 5.f, ch = 7.f;
    float tw = textWidth(buf, cw);
    pushText(buf, bx + (barPx - tw)*0.5f, by - ch - tickH - 2.f, cw, ch, textV);
}

// ─── 2-D slider ──────────────────────────────────────────────
static GLuint sliderVAO = 0, sliderVBO = 0;
static GLint  loc2dColor = -1, loc2dScreen = -1;

static void initSlider2D()
{
    glGenVertexArrays(1, &sliderVAO);
    glGenBuffers(1, &sliderVBO);
    glBindVertexArray(sliderVAO);
    glBindBuffer(GL_ARRAY_BUFFER, sliderVBO);
    // pre-allocate for a few 2D points
    glBufferData(GL_ARRAY_BUFFER, 16 * 2 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

static void drawSlider2D(int W, int H)
{
    constexpr float PAD = 40.f;
    float barY  = (float)H - 28.f;
    float barX0 = PAD;
    float barX1 = (float)W - PAD;
    float frac  = (float)(simTime / SIM_MAX);
    float hx    = barX0 + frac * (barX1 - barX0);

    glBindVertexArray(sliderVAO);
    glBindBuffer(GL_ARRAY_BUFFER, sliderVBO);

    // background track
    float track[4] = { barX0, barY, barX1, barY };
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(track), track);
    glUniform4f(loc2dColor, 0.35f, 0.35f, 0.40f, 1.f);
    glLineWidth(3.f);
    glDrawArrays(GL_LINES, 0, 2);

    // elapsed portion (cyan)
    float elapsed[4] = { barX0, barY, hx, barY };
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(elapsed), elapsed);
    glUniform4f(loc2dColor, 0.25f, 0.75f, 1.f, 1.f);
    glDrawArrays(GL_LINES, 0, 2);

    // handle
    float pt[2] = { hx, barY };
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(pt), pt);
    glUniform4f(loc2dColor, 1.f, 1.f, 1.f, 1.f);
    glPointSize(11.f);
    glDrawArrays(GL_POINTS, 0, 1);
    glPointSize(1.f);

    glBindVertexArray(0);
}

// ─── Callbacks ───────────────────────────────────────────────

static void framebuffer_size_callback(GLFWwindow*, int w, int h)
{
    glViewport(0, 0, w, h);
}

static void mouse_button_callback(GLFWwindow* win, int button, int action, int)
{
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;

    double cx, cy;
    glfwGetCursorPos(win, &cx, &cy);
    int W, H;
    glfwGetWindowSize(win, &W, &H);

    float sepX = (float)(W * mapSplit);
    bool  onSep = (std::abs((float)cx - sepX) <= 6.f);
    bool  inMap = ((float)cx > sepX + 6.f);

    if (action == GLFW_PRESS) {
        if (onSep) {
            // Drag separator
            mapSepDrag  = true;
            mapSepStartX = (float)cx;
        } else if (inMap && g_mapH2 > 0.f) {
            // Pan map with LMB
            mapLmbDrag     = true;
            mapPanStartLat = mapLatCenter;
            mapPanStartLon = mapLonCenter;
            mapPanStartPx  = (float)cx;
            mapPanStartPy  = (float)cy;
        } else if (!inMap && cy > H - 50) {
            // Slider
            sliderDrag = true;
            double frac = (cx - 40.0) / (W * mapSplit - 80.0);
            simTime = std::max(0.0, std::min(SIM_MAX, frac * SIM_MAX));
        } else if (!inMap) {
            mouseDown = true;
        }
    } else { // RELEASE
        mapSepDrag = false;
        mapLmbDrag = false;
        mouseDown  = false;
        sliderDrag = false;
    }
}

static void cursor_pos_callback(GLFWwindow* win, double xpos, double ypos)
{
    int W, H;
    glfwGetWindowSize(win, &W, &H);

    if (mapSepDrag) {
        mapSplit = std::max(0.25f, std::min(0.80f, (float)xpos / (float)W));
        lastX = (float)xpos; lastY = (float)ypos;
        return;
    }

    if (mapLmbDrag && g_mapH2 > 0.f && g_mapW2 > 0.f) {
        float dx = (float)xpos - mapPanStartPx;
        float dy = (float)ypos - mapPanStartPy;
        // Pixels → radians
        float lonDelta = -dx / g_mapW2 * 2.f * mapLonHalfSpan;
        float latDelta =  dy / g_mapH2 * 2.f * mapLatHalfSpan;
        mapLonCenter = normLon(mapPanStartLon + lonDelta);
        mapLatCenter = mapPanStartLat + latDelta;
        mapLatCenter = std::max(-(float)(PI*0.5) + mapLatHalfSpan,
                       std::min( (float)(PI*0.5) - mapLatHalfSpan, mapLatCenter));
        lastX = (float)xpos; lastY = (float)ypos;
        return;
    }

    if (sliderDrag) {
        double frac = (xpos - 40.0) / (W * mapSplit - 80.0);
        simTime = std::max(0.0, std::min(SIM_MAX, frac * SIM_MAX));
        lastX = (float)xpos; lastY = (float)ypos;
        return;
    }

    static bool first = true;
    if (first) { lastX = (float)xpos; lastY = (float)ypos; first = false; }

    if (mouseDown) {
        camYaw   += ((float)xpos - lastX) * 0.4f;
        camPitch  = glm::clamp(camPitch + ((float)ypos - lastY) * 0.4f, -85.f, 85.f);
    }
    lastX = (float)xpos; lastY = (float)ypos;
}

static void scroll_callback(GLFWwindow* win, double, double dy)
{
    double cx, cy;
    glfwGetCursorPos(win, &cx, &cy);
    if (g_W > 0 && (float)cx > g_W * mapSplit && g_mapH2 > 0.f && g_mapW2 > 0.f) {
        // Zoom isotrope planisphère — point sous le curseur reste fixe
        float normX = std::max(0.f, std::min(1.f, ((float)cx - g_mapX0) / g_mapW2));
        float normY = std::max(0.f, std::min(1.f, ((float)cy - g_mapY0) / g_mapH2));
        float curLon = normLon(mapLonCenter - mapLonHalfSpan + normX * 2.f * mapLonHalfSpan);
        float curLat = mapLatCenter + mapLatHalfSpan * (1.f - 2.f * normY);
        float factor = (dy > 0) ? 0.75f : 1.333f; // molette haut = zoom in
        mapLatHalfSpan = std::max(0.03f, std::min((float)(PI*0.5f), mapLatHalfSpan * factor));
        mapLonHalfSpan = std::max(0.03f, std::min((float)PI,        mapLonHalfSpan * factor));
        // Réajuste centres pour que le point sous le curseur reste fixe
        mapLatCenter = curLat - mapLatHalfSpan * (1.f - 2.f * normY);
        mapLatCenter = std::max(-(float)(PI*0.5f) + mapLatHalfSpan,
                       std::min( (float)(PI*0.5f) - mapLatHalfSpan, mapLatCenter));
        mapLonCenter = normLon(curLon - mapLonHalfSpan + (1.f - normX) * 2.f * mapLonHalfSpan
                               - mapLonHalfSpan);
        // Simplification: keep lon delta symmetric around cursor
        mapLonCenter = normLon(curLon - mapLonHalfSpan * (2.f * normX - 1.f));
    } else {
        camRadius = glm::clamp(camRadius - (float)dy * 0.3f, 1.5f, 20.f);
    }
}

static void key_callback(GLFWwindow* win, int key, int, int action, int)
{
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

    // one orbital period
    double T = TWO_PI * std::sqrt(ORB_A * ORB_A * ORB_A / MU_KM);

    switch (key) {
    case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(win, true); break;
    case GLFW_KEY_SPACE:  playing = !playing;                  break;
    case GLFW_KEY_R:      simTime = 0.0; playing = true; timeScale = 60.0; break;
    case GLFW_KEY_RIGHT:  simTime = std::min(SIM_MAX, simTime + T * 0.1);  break;
    case GLFW_KEY_LEFT:   simTime = std::max(0.0,     simTime - T * 0.1);  break;
    case GLFW_KEY_UP:     simTime = std::min(SIM_MAX, simTime + 3600.0);   break;
    case GLFW_KEY_DOWN:   simTime = std::max(0.0,     simTime - 3600.0);   break;
    case GLFW_KEY_EQUAL:
    case GLFW_KEY_KP_ADD:
        timeScale = std::min(3600.0, timeScale * 2.0); break;
    case GLFW_KEY_MINUS:
    case GLFW_KEY_KP_SUBTRACT:
        timeScale = std::max(1.0, timeScale * 0.5); break;
    case GLFW_KEY_T:
        gtDurIdx = (gtDurIdx + 1) % 4; break;
    case GLFW_KEY_O:   // O = shrink 3D / expand map
        mapSplit = std::max(0.30f, mapSplit - 0.05f); break;
    case GLFW_KEY_P:   // P = expand 3D / shrink map
        mapSplit = std::min(0.80f, mapSplit + 0.05f); break;
    case GLFW_KEY_M:              // M = reset map view (full world)
        mapLatCenter   = 0.f;
        mapLatHalfSpan = (float)(PI * 0.5);
        mapLonCenter   = 0.f;
        mapLonHalfSpan = (float)PI;
        break;
    default: break;
    }
}

// ─── Main ─────────────────────────────────────────────────────

int main()
{
    // ── GLFW init ──
    if (!glfwInit()) { std::cerr << "glfwInit failed\n"; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWwindow* win = glfwCreateWindow(
        SCR_W, SCR_H, "BFS / ISS Orbital Simulation", nullptr, nullptr);
    if (!win) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(win);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "GLAD failed\n"; return -1;
    }

    glfwSetFramebufferSizeCallback(win, framebuffer_size_callback);
    glfwSetMouseButtonCallback   (win, mouse_button_callback);
    glfwSetCursorPosCallback     (win, cursor_pos_callback);
    glfwSetScrollCallback        (win, scroll_callback);
    glfwSetKeyCallback           (win, key_callback);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);

    // ── Programs ──
    GLuint prog3d = createProgram(VERT_3D, FRAG_3D);
    GLuint prog2d = createProgram(VERT_2D, FRAG_2D);

    GLint loc3dMVP   = glGetUniformLocation(prog3d, "uMVP");
    GLint loc3dColor = glGetUniformLocation(prog3d, "uColor");
    loc2dColor  = glGetUniformLocation(prog2d, "uColor");
    loc2dScreen = glGetUniformLocation(prog2d, "uScreen");

    // ── Earth wireframe ──
    auto earthMesh = makeSphere(1.0f, 24, 36);
    auto earthGpu  = uploadIndexed(earthMesh.verts, earthMesh.idx);

    // ── Equator ring (separate, thicker/brighter) ──
    {
        // already part of Earth mesh at stack i=12 (phi=PI/2), use as-is
    }

    // ── Axis lines  (length 1.6 R_E) ──
    static const float AXES[] = {
        0,0,0,  1.6f,0,0,   // X  red
        0,0,0,  0,1.6f,0,   // Y  green
        0,0,0,  0,0,1.6f,   // Z  blue  (north pole)
    };
    GLuint axVAO, axVBO;
    glGenVertexArrays(1, &axVAO); glGenBuffers(1, &axVBO);
    glBindVertexArray(axVAO);
    glBindBuffer(GL_ARRAY_BUFFER, axVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(AXES), AXES, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // ── Orbit path (dynamic, 361 vertices) ──
    GpuDynamic orbitPath = makeDynamic(362);

    // ── Satellite point (single vertex, dynamic) ──
    GpuDynamic satPoint = makeDynamic(1);

    // ── Ground track (ECEF, dynamic: 1 sample/60s → 1440 pts max for 24h) ──
    GpuDynamic groundTrack = makeDynamic(1500);

    // ── North pole cross marker  (small + at Z=1.01 in ECI/ECEF) ──
    static const float NP_VERTS[] = {
        -0.045f, 0.f, 1.01f,   0.045f, 0.f, 1.01f,   // horizontal
         0.f, -0.045f, 1.01f,  0.f, 0.045f, 1.01f,   // vertical
    };
    GLuint npVAO, npVBO;
    glGenVertexArrays(1, &npVAO); glGenBuffers(1, &npVBO);
    glBindVertexArray(npVAO);
    glBindBuffer(GL_ARRAY_BUFFER, npVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(NP_VERTS), NP_VERTS, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // ── Paris hexagon on globe (ECEF, static) ──
    auto parisGlobeVerts = makeParisHexGlobe();
    GpuDynamic parisGlobe = makeDynamic(12);
    updateDynamic(parisGlobe, parisGlobeVerts);

    // ── Planisphere 2-D buffer (grid + labels + track + markers) ──
    GpuDynamic planMap = makeDynamic2D(12000);

    // ── 2-D slider ──
    initSlider2D();

    // ── Console header ──
    std::cout
        << "╔══════════════════════════════════════════════════════════╗\n"
        << "║  BFS / ISS Orbital Simulation  –  J2 Perturbation        ║\n"
        << "║  a=" << (int)ORB_A << " km  e=" << ORB_E
        << "  i=51.6°  RAAN=0  ω=0                 ║\n"
        << "║  Controls:  SPACE=play/pause  ←/→=±10% orbit             ║\n"
        << "║             ↑/↓=±1 h          +/-=speed  R=reset         ║\n"
        << "║             Drag slider (bottom)  |  Mouse: rotate/zoom  ║\n"
        << "╚══════════════════════════════════════════════════════════╝\n"
        << std::left
        << std::setw(12) << "t(s)"
        << std::setw(10) << "Alt(km)"
        << std::setw(10) << "|v|(km/s)"
        << std::setw(16) << "x(km)"
        << std::setw(16) << "y(km)"
        << std::setw(16) << "z(km)"
        << std::setw(12) << "vx(km/s)"
        << std::setw(12) << "vy(km/s)"
        << std::setw(12) << "vz(km/s)"
        << "\n"
        << std::string(116, '-') << "\n";

    wallPrev = glfwGetTime();
    double lastConsolePrint = -999.0;

    // ─────────────────────────────────────────────────────────
    while (!glfwWindowShouldClose(win))
    {
        glfwPollEvents();

        // ── Advance simulation time ──
        double wallNow = glfwGetTime();
        double dt      = wallNow - wallPrev;
        wallPrev = wallNow;
        if (playing) {
            simTime += dt * timeScale;
            if (simTime >= SIM_MAX) { simTime = SIM_MAX; playing = false; }
        }

        // ── Propagate orbit ──
        OrbState s = propagate(simTime);
        glm::vec3 satGL = glm::vec3(s.pos * GL_SCALE);

        // ── Update GPU buffers ──
        auto orbitVerts = makeOrbitPath(simTime);
        updateDynamic(orbitPath, orbitVerts);

        double gtDur   = GT_DURATIONS[gtDurIdx];
        double gtStart = std::max(0.0, simTime - gtDur);
        auto gtVerts = makeGroundTrack(gtStart, simTime);
        updateDynamic(groundTrack, gtVerts);

        float sp[3] = { satGL.x, satGL.y, satGL.z };
        glBindBuffer(GL_ARRAY_BUFFER, satPoint.vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(sp), sp);
        satPoint.count = 1;

        // ── Console print every simulated minute ──
        if (simTime - lastConsolePrint >= 60.0) {
            lastConsolePrint = simTime;
            int hh = (int)(simTime / 3600);
            int mm = (int)(std::fmod(simTime, 3600.0) / 60.0);
            int ss = (int)std::fmod(simTime, 60.0);
            std::cout << std::fixed << std::setprecision(1)
                      << std::setw(4) << hh << "h" << std::setw(2) << mm << "m"
                      << std::setw(2) << ss << "s   "
                      << std::setw(7)  << s.alt   << "   "
                      << std::setw(7)  << s.speed << "   "
                      << std::setprecision(1)
                      << std::setw(12) << s.pos.x << "   "
                      << std::setw(12) << s.pos.y << "   "
                      << std::setw(12) << s.pos.z << "   "
                      << std::setprecision(4)
                      << std::setw(10) << s.vel.x << "   "
                      << std::setw(10) << s.vel.y << "   "
                      << std::setw(10) << s.vel.z << "\n";
        }

        // ── Window title ──
        {
            int hh = (int)(simTime / 3600);
            int mm = (int)(std::fmod(simTime, 3600.0) / 60.0);
            std::ostringstream t;
            t << std::fixed << std::setprecision(1)
              << "BFS/ISS  |  " << hh << "h " << mm << "m"
              << "  |  Alt " << s.alt << " km"
              << "  |  v = " << s.speed << " km/s"
              << "  |  x" << (int)timeScale << " realtime"
              << "  |  Track: " << GT_LABELS[gtDurIdx] << "  [T]"
              << (playing ? "  ▶" : "  ⏸");
            glfwSetWindowTitle(win, t.str().c_str());
        }

        // ── 3D render (left viewport only) ──
        int W, H;
        glfwGetFramebufferSize(win, &W, &H);
        g_W = W; g_H = H;
        int view3dW = (int)(W * mapSplit);
        glViewport(0, 0, view3dW, H);
        glClearColor(0.04f, 0.04f, 0.09f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Camera orbits around Z axis — north pole stays at top (up = +Z)
        float aspect = (float)view3dW / (float)H;
        glm::mat4 proj = glm::perspective(glm::radians(45.f), aspect, 0.01f, 200.f);
        float yR = glm::radians(camYaw), pR = glm::radians(camPitch);
        glm::vec3 camPos = {
            camRadius * std::cos(pR) * std::sin(yR),
            camRadius * std::cos(pR) * std::cos(yR),
            camRadius * std::sin(pR)
        };
        glm::mat4 view = glm::lookAt(camPos, glm::vec3(0.f), glm::vec3(0.f, 0.f, 1.f));
        glm::mat4 mvp  = proj * view;

        // Earth rotation: Rz(GMST) rotates the ECEF frame with the planet
        double    thetaE   = OMEGA_EARTH * simTime;
        glm::mat4 earthRot = glm::rotate(glm::mat4(1.f), (float)thetaE,
                                          glm::vec3(0.f, 0.f, 1.f));
        glm::mat4 mvpEarth = mvp * earthRot;

        glUseProgram(prog3d);

        // Earth (ECEF frame — rotates with planet)
        glUniformMatrix4fv(loc3dMVP, 1, GL_FALSE, glm::value_ptr(mvpEarth));
        glUniform3f(loc3dColor, 0.05f, 0.20f, 0.55f);
        glBindVertexArray(earthGpu.vao);
        glDrawElements(GL_LINES, earthGpu.count, GL_UNSIGNED_INT, nullptr);

        // Axes (ECI frame — inertial, no Earth rotation)
        glUniformMatrix4fv(loc3dMVP, 1, GL_FALSE, glm::value_ptr(mvp));
        glLineWidth(1.5f);
        glBindVertexArray(axVAO);
        glUniform3f(loc3dColor, 1.0f, 0.25f, 0.25f); glDrawArrays(GL_LINES, 0, 2); // X
        glUniform3f(loc3dColor, 0.25f, 1.0f, 0.25f); glDrawArrays(GL_LINES, 2, 2); // Y
        glUniform3f(loc3dColor, 0.30f, 0.55f, 1.0f); glDrawArrays(GL_LINES, 4, 2); // Z (north)
        glLineWidth(1.f);

        // Orbit path (bright cyan, ECI frame)
        glUniform3f(loc3dColor, 0.25f, 0.90f, 0.90f);
        glBindVertexArray(orbitPath.vao);
        glDrawArrays(GL_LINE_STRIP, 0, orbitPath.count);

        // Satellite (bright yellow, ECI frame)
        glUniform3f(loc3dColor, 1.f, 0.95f, 0.25f);
        glBindVertexArray(satPoint.vao);
        glPointSize(10.f);
        glDrawArrays(GL_POINTS, 0, 1);
        glPointSize(1.f);

        // North pole marker (white cross, Z axis — unchanged by Rz rotation)
        glUniformMatrix4fv(loc3dMVP, 1, GL_FALSE, glm::value_ptr(mvp));
        glUniform3f(loc3dColor, 1.f, 1.f, 1.f);
        glLineWidth(2.f);
        glBindVertexArray(npVAO);
        glDrawArrays(GL_LINES, 0, 4);
        glLineWidth(1.f);

        // Ground track (ECEF frame — sticks to rotating Earth, bright green)
        glUniformMatrix4fv(loc3dMVP, 1, GL_FALSE, glm::value_ptr(mvpEarth));
        glUniform3f(loc3dColor, 0.15f, 1.0f, 0.40f);
        glLineWidth(1.5f);
        glBindVertexArray(groundTrack.vao);
        glDrawArrays(GL_LINE_STRIP, 0, groundTrack.count);
        glLineWidth(1.f);

        // Paris hexagon (ECEF frame — rotates with Earth, orange)
        glUniform3f(loc3dColor, 1.f, 0.55f, 0.10f);
        glLineWidth(2.f);
        glBindVertexArray(parisGlobe.vao);
        glDrawArrays(GL_LINES, 0, parisGlobe.count);
        glLineWidth(1.f);

        // ── 2-D overlay (full viewport) ──
        glViewport(0, 0, W, H);
        glDisable(GL_DEPTH_TEST);
        glUseProgram(prog2d);
        glUniform2f(loc2dScreen, (float)W, (float)H);

        // -- Slider (left portion) --
        drawSlider2D(view3dW, H);

        // -- Planisphere (right portion) --
        {
            constexpr float PAD_L = 14.f, PAD_R = 6.f;
            constexpr float PAD_T = 5.f,  PAD_B = 38.f; // room for labels + hint
            float mapX0 = (float)view3dW + PAD_L;
            float mapX1 = (float)W - PAD_R;
            float mapW2 = mapX1 - mapX0;
            // Hauteur indépendante de la largeur : remplit la fenêtre verticalement
            float mapH2 = std::max(40.f, (float)H - PAD_T - PAD_B);
            float mapY0 = PAD_T;
            float mapY1 = mapY0 + mapH2;
            // Cache pour les callbacks
            g_mapX0=mapX0; g_mapY0=mapY0; g_mapX1=mapX1; g_mapY1=mapY1;
            g_mapH2=mapH2; g_mapW2=mapW2;

            auto draw2d = [&](const std::vector<float>& v, GLenum mode) {
                if (v.empty()) return;
                updateDynamic2D(planMap, v);
                glBindVertexArray(planMap.vao);
                glDrawArrays(mode, 0, planMap.count);
            };

            // Fond du panneau droit
            glUniform4f(loc2dColor, 0.04f, 0.04f, 0.09f, 1.f);
            draw2d({ (float)view3dW,0, (float)W,0, (float)W,(float)H,
                     (float)view3dW,0, (float)W,(float)H, (float)view3dW,(float)H },
                   GL_TRIANGLES);

            // Ligne séparateur (survol = curseur resize)
            glUniform4f(loc2dColor, 0.30f, 0.30f, 0.40f, 1.f);
            draw2d({ (float)view3dW, 0.f, (float)view3dW, (float)H }, GL_LINES);

            // Fond océan de la carte
            glUniform4f(loc2dColor, 0.05f, 0.08f, 0.14f, 1.f);
            draw2d({ mapX0,mapY0, mapX1,mapY0, mapX1,mapY1,
                     mapX0,mapY0, mapX1,mapY1, mapX0,mapY1 }, GL_TRIANGLES);

            // Clipper tout le contenu de la carte au rectangle map
            // glScissor : origine en bas-gauche framebuffer (Y inversé)
            glEnable(GL_SCISSOR_TEST);
            glScissor((GLint)mapX0, (GLint)(H - mapY1),
                      (GLsizei)(mapX1 - mapX0), (GLsizei)(mapY1 - mapY0));

            // Grille lat/lon
            glUniform4f(loc2dColor, 0.18f, 0.18f, 0.28f, 1.f);
            draw2d(makeMapGrid(mapX0, mapY0, mapX1, mapY1,
                               mapLatCenter, mapLatHalfSpan,
                               mapLonCenter, mapLonHalfSpan), GL_LINES);

            // Équateur + méridien 0° (plus lumineux)
            float eqY  = latToMapY(0.f, mapY0, mapH2, mapLatCenter, mapLatHalfSpan);
            float pm0X = lonToMapX(0.f, mapX0, mapW2, mapLonCenter, mapLonHalfSpan);
            glUniform4f(loc2dColor, 0.28f, 0.28f, 0.45f, 1.f);
            draw2d({ mapX0, eqY, mapX1, eqY,
                     pm0X, mapY0, pm0X, mapY1 }, GL_LINES);

            // Trace au sol
            auto track2d = makeMapTrack(gtStart, simTime, 60.0,
                                        mapX0, mapY0, mapX1, mapY1,
                                        mapLatCenter, mapLatHalfSpan,
                                        mapLonCenter, mapLonHalfSpan);
            glUniform4f(loc2dColor, 0.15f, 1.0f, 0.40f, 1.f);
            glLineWidth(1.5f);
            draw2d(track2d, GL_LINES);
            glLineWidth(1.f);

            // Hexagone Paris sur la carte (orange)
            glUniform4f(loc2dColor, 1.f, 0.55f, 0.10f, 1.f);
            glLineWidth(1.5f);
            draw2d(makeParisHexMap(mapX0, mapY0, mapX1, mapY1,
                                   mapLatCenter, mapLatHalfSpan,
                                   mapLonCenter, mapLonHalfSpan), GL_LINES);
            glLineWidth(1.f);

            // Point satellite courant (jaune)
            auto [satLat, satLon] = eciToLatLon(simTime, s.pos);
            float sx = lonToMapX(satLon, mapX0, mapW2, mapLonCenter, mapLonHalfSpan);
            float sy = latToMapY(satLat, mapY0, mapH2, mapLatCenter, mapLatHalfSpan);
            glUniform4f(loc2dColor, 1.f, 0.95f, 0.25f, 1.f);
            glPointSize(7.f);
            draw2d({ sx, sy }, GL_POINTS);
            glPointSize(1.f);

            // Fin du clipping — les labels, bordure et barre d'échelle peuvent
            // légèrement déborder de la carte (c'est intentionnel).
            glDisable(GL_SCISSOR_TEST);

            // Bordure carte (dessinée par-dessus pour masquer les débordements)
            glUniform4f(loc2dColor, 0.45f, 0.45f, 0.60f, 1.f);
            draw2d({ mapX0,mapY0, mapX1,mapY0, mapX1,mapY0, mapX1,mapY1,
                     mapX1,mapY1, mapX0,mapY1, mapX0,mapY1, mapX0,mapY0 }, GL_LINES);

            // Labels lat/lon (stroke font)
            glUniform4f(loc2dColor, 0.80f, 0.80f, 0.90f, 1.f);
            draw2d(makeMapLabels(mapX0, mapY0, mapX1, mapY1,
                                 mapLatCenter, mapLatHalfSpan,
                                 mapLonCenter, mapLonHalfSpan), GL_LINES);

            // Barre d'échelle (bas-gauche)
            {
                std::vector<float> scaleLines, scaleText;
                makeScaleBar(mapX0 + 8.f, mapY1 - 12.f, mapW2,
                             mapLonHalfSpan, mapLatCenter,
                             scaleLines, scaleText);
                glUniform4f(loc2dColor, 0.85f, 0.85f, 0.95f, 1.f);
                glLineWidth(1.5f);
                draw2d(scaleLines, GL_LINES);
                glLineWidth(1.f);
                glUniform4f(loc2dColor, 0.75f, 0.75f, 0.85f, 1.f);
                draw2d(scaleText, GL_LINES);
            }

            // Hint commandes (sous la carte)
            {
                std::vector<float> hintV;
                pushText("drag=pan  scroll=zoom  O/P=resize  M=reset",
                         mapX0, mapY1 + 5.f, 5.f, 7.f, hintV);
                glUniform4f(loc2dColor, 0.30f, 0.30f, 0.40f, 1.f);
                draw2d(hintV, GL_LINES);
            }
        }

        glDisable(GL_SCISSOR_TEST); // sécurité si le bloc map sort prématurément
        glEnable(GL_DEPTH_TEST);

        glfwSwapBuffers(win);
    }

    // ── Cleanup ──
    glDeleteVertexArrays(1, &earthGpu.vao);
    glDeleteBuffers(1, &earthGpu.vbo);
    glDeleteBuffers(1, &earthGpu.ebo);
    glDeleteVertexArrays(1, &axVAO);
    glDeleteBuffers(1, &axVBO);
    glDeleteVertexArrays(1, &orbitPath.vao);
    glDeleteBuffers(1, &orbitPath.vbo);
    glDeleteVertexArrays(1, &satPoint.vao);
    glDeleteBuffers(1, &satPoint.vbo);
    glDeleteVertexArrays(1, &groundTrack.vao);
    glDeleteBuffers(1, &groundTrack.vbo);
    glDeleteVertexArrays(1, &npVAO);
    glDeleteBuffers(1, &npVBO);
    glDeleteVertexArrays(1, &parisGlobe.vao);
    glDeleteBuffers(1, &parisGlobe.vbo);
    glDeleteVertexArrays(1, &planMap.vao);
    glDeleteBuffers(1, &planMap.vbo);
    glDeleteVertexArrays(1, &sliderVAO);
    glDeleteBuffers(1, &sliderVBO);
    glDeleteProgram(prog3d);
    glDeleteProgram(prog2d);
    glfwTerminate();
    return 0;
}
