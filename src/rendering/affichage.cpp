// =============================================================================
// affichage.cpp — Implémentation des classes de rendu, OrbitalSim
//
// Voir affichage.h pour la documentation des interfaces.
// =============================================================================

#include "rendering/affichage.h"

#include <glm/gtc/type_ptr.hpp>         // glm::value_ptr (pour passer les vecteurs à OpenGL)
#include <glm/gtc/matrix_transform.hpp> // glm::radians
#include <glm/gtc/constants.hpp>        // glm::pi<float>()

#include <cmath>
#include <algorithm>    // std::max, std::min, std::floor, std::ceil


// =============================================================================
// WayPoint
// =============================================================================

// ── hexSphere ─────────────────────────────────────────────────────────────────
std::vector<float> WayPoint::hexSphere() const
{
    // ── Conversion degrés → radians ──────────────────────────────────────────
    const float la = glm::radians(lat);
    const float lo = glm::radians(lon);

    // ── Centre sur la sphère unité ────────────────────────────────────────────
    // Convention : axe Y = pôle Nord, plan XZ = équateur.
    //   x = cos(lat) · cos(lon)   (vers 0° lon sur l'équateur)
    //   y = sin(lat)              (vers le pôle Nord)
    //   z = cos(lat) · sin(lon)   (vers 90°E sur l'équateur)
    glm::vec3 P = {
        std::cos(la) * std::cos(lo),
        std::sin(la),
        std::cos(la) * std::sin(lo)
    };

    // ── Vecteur "Est" local ───────────────────────────────────────────────────
    // C'est la dérivée de P par rapport à la longitude (normalisée) :
    //   dP/dlon = (-cos(la)·sin(lo),  0,  cos(la)·cos(lo))
    // Le facteur cos(la) sort (on normalise), donc :
    //   east = (-sin(lo), 0, cos(lo))
    glm::vec3 east = glm::normalize(glm::vec3(
        -std::sin(lo),
         0.0f,
         std::cos(lo)
    ));

    // ── Vecteur "Nord" local ──────────────────────────────────────────────────
    // Dérivée de P par rapport à la latitude (normalisée) :
    //   dP/dlat = (-sin(la)·cos(lo),  cos(la),  -sin(la)·sin(lo))
    glm::vec3 north = glm::normalize(glm::vec3(
        -std::sin(la) * std::cos(lo),
         std::cos(la),
        -std::sin(la) * std::sin(lo)
    ));

    // ── Sommets de l'hexagone ─────────────────────────────────────────────────
    // Chaque sommet est dans le plan tangent :
    //   V_i = P·1.005 + r·(cos(a_i)·east + sin(a_i)·north)
    // Le facteur 1.005 décolle légèrement l'hexagone de la surface pour
    // éviter le z-fighting (artefact visuel quand deux surfaces sont coplanaires).
    std::vector<float> pts;
    for (int i = 0; i <= 6; ++i) {   // 7 points : 6 côtés + retour au départ
        float a = i * glm::pi<float>() / 3.0f;   // 0°, 60°, 120°, …, 360°
        glm::vec3 v = P * 1.005f + radius3D * (std::cos(a) * east + std::sin(a) * north);
        pts.push_back(v.x);
        pts.push_back(v.y);
        pts.push_back(v.z);
    }
    return pts;
}

// ── crossSphere ───────────────────────────────────────────────────────────────
std::vector<float> WayPoint::crossSphere() const
{
    const float la = glm::radians(lat);
    const float lo = glm::radians(lon);

    // Même convention d'axes que hexSphere()
    glm::vec3 P = {
        std::cos(la) * std::cos(lo),
        std::sin(la),
        std::cos(la) * std::sin(lo)
    };

    // Vecteurs locaux (identiques à hexSphere)
    glm::vec3 east = glm::normalize(glm::vec3(
        -std::sin(lo),
         0.0f,
         std::cos(lo)
    ));
    glm::vec3 north = glm::normalize(glm::vec3(
        -std::sin(la) * std::cos(lo),
         std::cos(la),
        -std::sin(la) * std::sin(lo)
    ));

    // ── Quatre extrémités de la croix ─────────────────────────────────────────
    // La croix (+) est composée de deux segments dans le plan tangent :
    //   barre horizontale : P·1.005 ± r·east
    //   barre verticale   : P·1.005 ± r·north
    //
    // Format de retour pour GL_LINES : [p0, p1,  p2, p3]
    //   segment 1 : p0 → p1  (barre est-ouest)
    //   segment 2 : p2 → p3  (barre nord-sud)
    glm::vec3 center = P * 1.005f;
    const float r = radius3D;

    glm::vec3 p0 = center - r * east;
    glm::vec3 p1 = center + r * east;
    glm::vec3 p2 = center - r * north;
    glm::vec3 p3 = center + r * north;

    return {
        p0.x, p0.y, p0.z,
        p1.x, p1.y, p1.z,
        p2.x, p2.y, p2.z,
        p3.x, p3.y, p3.z
    };
}

// ── markerSphere / glMode ─────────────────────────────────────────────────────
std::vector<float> WayPoint::markerSphere() const
{
    return (shape == WPShape::Cross) ? crossSphere() : hexSphere();
}

GLenum WayPoint::glMode() const
{
    // Hexagone : contour fermé → LINE_STRIP
    // Croix    : 2 segments indépendants → LINES
    return (shape == WPShape::Cross) ? GL_LINES : GL_LINE_STRIP;
}


// =============================================================================
// DynBuf2D
// =============================================================================

DynBuf2D make_dyn2d()
{
    DynBuf2D d;

    // ── Création des objets OpenGL ────────────────────────────────────────────
    glGenVertexArrays(1, &d.vao);   // VAO : mémorise la configuration des attributs
    glGenBuffers(1, &d.vbo);        // VBO : stocke les données de sommets

    // ── Configuration du format des attributs ─────────────────────────────────
    glBindVertexArray(d.vao);
    glBindBuffer(GL_ARRAY_BUFFER, d.vbo);

    // Attribut 0 : position 2D (vec2 = 2 floats, non normalisés)
    // stride = 2 * sizeof(float) = 8 octets entre deux sommets consécutifs
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    return d;
}

void draw_2d(DynBuf2D& d, const std::vector<float>& pts,
             GLenum mode, GLint locColor, glm::vec4 col)
{
    if (pts.empty()) return;

    // Upload des données : GL_DYNAMIC_DRAW signale au driver que ce buffer
    // est souvent mis à jour (il peut l'allouer dans une zone mémoire rapide).
    glBindBuffer(GL_ARRAY_BUFFER, d.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(pts.size() * sizeof(float)),
                 pts.data(), GL_DYNAMIC_DRAW);

    // Envoi de la couleur uniforme au shader actif
    glUniform4fv(locColor, 1, glm::value_ptr(col));

    // Dessin
    glBindVertexArray(d.vao);
    glDrawArrays(mode, 0, static_cast<GLsizei>(pts.size() / 2));
    glBindVertexArray(0);
}


// =============================================================================
// DynBuf3D
// =============================================================================

DynBuf3D make_dyn3d()
{
    DynBuf3D d;
    glGenVertexArrays(1, &d.vao);
    glGenBuffers(1, &d.vbo);

    glBindVertexArray(d.vao);
    glBindBuffer(GL_ARRAY_BUFFER, d.vbo);

    // Attribut 0 : position 3D (vec3 = 3 floats)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    return d;
}

void draw_3d(DynBuf3D& d, const std::vector<float>& pts,
             GLenum mode, GLint locColor, glm::vec4 col)
{
    if (pts.empty()) return;
    glBindBuffer(GL_ARRAY_BUFFER, d.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(pts.size() * sizeof(float)),
                 pts.data(), GL_DYNAMIC_DRAW);
    glUniform4fv(locColor, 1, glm::value_ptr(col));
    glBindVertexArray(d.vao);
    glDrawArrays(mode, 0, static_cast<GLsizei>(pts.size() / 3));
    glBindVertexArray(0);
}


// =============================================================================
// SphereGPU
// =============================================================================

SphereGPU build_sphere(float r, int stacks, int slices)
{
    std::vector<float>        verts;
    std::vector<unsigned int> idx;

    // ── Génération des sommets ────────────────────────────────────────────────
    // Paramétrage par coordonnées sphériques :
    //   phi   ∈ [0, π]   : colatitude (0 = pôle Nord, π = pôle Sud)
    //   theta ∈ [0, 2π]  : longitude
    //
    // Position : (sin(phi)·cos(theta),  cos(phi),  sin(phi)·sin(theta))
    // Normale  : identique car la sphère est centrée à l'origine
    for (int i = 0; i <= stacks; ++i) {
        float phi = glm::pi<float>() * i / stacks;
        for (int j = 0; j <= slices; ++j) {
            float theta = glm::two_pi<float>() * j / slices;

            float x = std::sin(phi) * std::cos(theta);
            float y = std::cos(phi);    // pôle Nord = (0, 1, 0)
            float z = std::sin(phi) * std::sin(theta);

            // Position
            verts.push_back(r * x);
            verts.push_back(r * y);
            verts.push_back(r * z);
            // Normale (direction unitaire, même valeur que pos/r)
            verts.push_back(x);
            verts.push_back(y);
            verts.push_back(z);
        }
    }

    // ── Génération des indices ────────────────────────────────────────────────
    // Chaque quad (i,j) est découpé en 2 triangles.
    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            unsigned a = i * (slices + 1) + j;
            unsigned b = a + (slices + 1);

            idx.push_back(a);   idx.push_back(b);   idx.push_back(a + 1);
            idx.push_back(a + 1); idx.push_back(b); idx.push_back(b + 1);
        }
    }

    // ── Upload GPU ────────────────────────────────────────────────────────────
    SphereGPU g;
    g.count = static_cast<GLsizei>(idx.size());

    glGenVertexArrays(1, &g.vao);
    glGenBuffers(1, &g.vbo);
    glGenBuffers(1, &g.ebo);

    glBindVertexArray(g.vao);

    glBindBuffer(GL_ARRAY_BUFFER, g.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(verts.size() * sizeof(float)),
                 verts.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(idx.size() * sizeof(unsigned)),
                 idx.data(), GL_STATIC_DRAW);

    // Attrib 0 : position (3 floats, offset 0, stride 6 floats)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                          6 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);

    // Attrib 1 : normale (3 floats, offset 3 floats)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                          6 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    return g;
}


// =============================================================================
// Graticule3DGPU
// =============================================================================

// ── Helper interne : uploade un tableau de vec3 dans un VAO/VBO statique ──────
// GL_STATIC_DRAW : indique au driver que ces données ne changeront jamais.
static void upload_static3d(GLuint& vao, GLuint& vbo, GLsizei& cnt,
                             const std::vector<float>& pts)
{
    cnt = static_cast<GLsizei>(pts.size() / 3);
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(pts.size() * sizeof(float)),
                 pts.data(), GL_STATIC_DRAW);
    // Attribut 0 : position vec3
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

Graticule3DGPU build_graticule3D(float r, int seg)
{
    // seg = nombre de segments par arc (≥ 36 pour paraître lisse).
    // Chaque arc est stocké comme `seg` paires de sommets (GL_LINES).

    std::vector<float> fine, major, highlight;

    // ── Lambda : ajoute un méridien (arc de lon fixe, lat = −90→+90) ─────────
    // lon_deg : longitude fixe en degrés entiers
    auto addMeridian = [&](std::vector<float>& v, int lon_deg)
    {
        const float theta = glm::radians(static_cast<float>(lon_deg));
        for (int i = 0; i < seg; ++i)
        {
            // Deux extrémités du i-ème sous-segment
            float lat0 = -90.0f + 180.0f *  i      / seg;
            float lat1 = -90.0f + 180.0f * (i + 1) / seg;
            float phi0 = glm::radians(lat0);
            float phi1 = glm::radians(lat1);

            // p0
            v.push_back(r * std::cos(phi0) * std::cos(theta));
            v.push_back(r * std::sin(phi0));
            v.push_back(r * std::cos(phi0) * std::sin(theta));
            // p1
            v.push_back(r * std::cos(phi1) * std::cos(theta));
            v.push_back(r * std::sin(phi1));
            v.push_back(r * std::cos(phi1) * std::sin(theta));
        }
    };

    // ── Lambda : ajoute un parallèle (arc de lat fixe, lon = 0→360°) ─────────
    // lat_deg : latitude fixe en degrés entiers
    auto addParallel = [&](std::vector<float>& v, int lat_deg)
    {
        const float phi = glm::radians(static_cast<float>(lat_deg));
        const float ry  = r * std::sin(phi);   // hauteur y (fixe sur tout le cercle)
        const float rr  = r * std::cos(phi);   // rayon du cercle de latitude

        for (int i = 0; i < seg; ++i)
        {
            float th0 = glm::two_pi<float>() *  i      / seg;
            float th1 = glm::two_pi<float>() * (i + 1) / seg;

            // p0
            v.push_back(rr * std::cos(th0));
            v.push_back(ry);
            v.push_back(rr * std::sin(th0));
            // p1
            v.push_back(rr * std::cos(th1));
            v.push_back(ry);
            v.push_back(rr * std::sin(th1));
        }
    };

    // ─────────────────────────────────────────────────────────────────────────
    // NIVEAU 1 — Grille fine : méridiens tous les 10°, parallèles tous les 5°
    // Exclut les multiples de 30° / 15° (dessinés au niveau 2).
    // ─────────────────────────────────────────────────────────────────────────
    for (int lo = 0; lo < 360; lo += 10)
        if (lo % 30 != 0)
            addMeridian(fine, lo);

    for (int la = -85; la <= 85; la += 5)  // ±90° exclus (pôles = points dégénérés)
        if (la % 15 != 0)
            addParallel(fine, la);

    // ─────────────────────────────────────────────────────────────────────────
    // NIVEAU 2 — Grille principale : méridiens tous les 30°, parallèles tous les 15°
    // Exclut Greenwich (lon=0°) et l'équateur (lat=0°) → dessinés au niveau 3.
    // ─────────────────────────────────────────────────────────────────────────
    for (int lo = 0; lo < 360; lo += 30)
        if (lo != 0)
            addMeridian(major, lo);

    for (int la = -75; la <= 75; la += 15)
        if (la != 0)
            addParallel(major, la);

    // ─────────────────────────────────────────────────────────────────────────
    // NIVEAU 3 — Surbrillances : méridien de Greenwich (lon=0°) + équateur (lat=0°)
    // Trait plus épais, couleur plus vive — dessinés en dernier (par-dessus).
    // ─────────────────────────────────────────────────────────────────────────
    addMeridian(highlight, 0);  // méridien de Greenwich
    addParallel(highlight, 0);  // équateur

    // ── Upload sur GPU ────────────────────────────────────────────────────────
    Graticule3DGPU g;
    upload_static3d(g.vaoFine, g.vboFine, g.cntFine, fine);
    upload_static3d(g.vaoMaj,  g.vboMaj,  g.cntMaj,  major);
    upload_static3d(g.vaoHl,   g.vboHl,   g.cntHl,   highlight);
    return g;
}

void draw_graticule3D(const Graticule3DGPU& g, GLint locColor)
{
    // ── Niveau fin (couleur sombre, même palette que le planisphère) ──────────
    glUniform4f(locColor, 0.08f, 0.17f, 0.30f, 1.0f);
    glBindVertexArray(g.vaoFine);
    glDrawArrays(GL_LINES, 0, g.cntFine);

    // ── Niveau principal ──────────────────────────────────────────────────────
    glUniform4f(locColor, 0.18f, 0.36f, 0.58f, 1.0f);
    glBindVertexArray(g.vaoMaj);
    glDrawArrays(GL_LINES, 0, g.cntMaj);

    // ── Surbrillances : Greenwich + équateur ──────────────────────────────────
    // Trait légèrement plus épais pour mieux les distinguer.
    glLineWidth(2.0f);
    glUniform4f(locColor, 0.40f, 0.66f, 0.92f, 1.0f);
    glBindVertexArray(g.vaoHl);
    glDrawArrays(GL_LINES, 0, g.cntHl);
    glLineWidth(1.0f);   // reset systématique

    glBindVertexArray(0);
}


// =============================================================================
// Helpers géométrie
// =============================================================================

std::vector<float> make_rect(float x0, float y0, float x1, float y1)
{
    // Deux triangles, sens horaire (y=0 en haut) :
    //   (x0,y0)─(x1,y0)
    //      │   ╲   │
    //   (x0,y1)─(x1,y1)
    return { x0, y0,  x1, y0,  x1, y1,
             x1, y1,  x0, y1,  x0, y0 };
}


// =============================================================================
// Planisphere — implémentation
// =============================================================================

// ── pixPerDeg ─────────────────────────────────────────────────────────────────
// À zoom=1 : 180° de latitude tiennent dans la HAUTEUR DU PANNEAU pixels.
// La hauteur du panneau = winH − panelTop (winH = fbH total de la fenêtre).
float Planisphere::pixPerDeg(int winH) const
{
    int panelH = winH - panelTop;
    return (static_cast<float>(panelH) / 180.0f) * zoom;
}

// ── halfVisLon ────────────────────────────────────────────────────────────────
// Demi-largeur du panneau en degrés = (panelW/2) / pixPerDeg + marge.
float Planisphere::halfVisLon(int splitX, int winW, int winH, float margin) const
{
    int panelW = winW - splitX;
    return static_cast<float>(panelW) / (2.0f * pixPerDeg(winH)) + margin;
}

// ── halfVisLat ────────────────────────────────────────────────────────────────
// Demi-hauteur visible en latitude : winH/2 / pixPerDeg = 90/zoom.
float Planisphere::halfVisLat(int winH) const
{
    return 90.0f / zoom;
}

// ── project ───────────────────────────────────────────────────────────────────
// Projection équirectangulaire : mappe (lon°, lat°) → pixel écran.
glm::vec2 Planisphere::project(float lon, float lat,
                                int splitX, int winW, int winH) const
{
    int   panelW = winW - splitX;
    int   panelH = winH - panelTop;
    float ppd    = pixPerDeg(winH);   // utilise panelH en interne

    // Abscisse du centre du panneau droit en pixels absolus
    float cx = static_cast<float>(splitX) + static_cast<float>(panelW) * 0.5f;
    // Ordonnée du CENTRE DU PANNEAU en coordonnées écran (y=0 en haut).
    // Le panneau commence à panelTop, donc son centre est à panelTop + panelH/2.
    float cy = static_cast<float>(panelTop) + static_cast<float>(panelH) * 0.5f;

    return {
        cx + (lon - ctrLon) * ppd,   // +lon → droite
        cy - (lat - ctrLat) * ppd    // +lat → haut → y plus petit
    };
}

// ── clampLat ──────────────────────────────────────────────────────────────────
// Garantit que les pôles Nord/Sud restent hors de la fenêtre.
//
// DÉMONSTRATION :
//   y_pôleNord = cy − (90 − ctrLat) × ppd
//   Condition  y_pôleNord ≤ 0 :
//     (90 − ctrLat) × ppd ≥ winH/2
//     ctrLat ≤ 90 − 90/zoom
void Planisphere::clampLat()
{
    float latMax = std::max(0.0f, 90.0f - 90.0f / zoom);
    ctrLat = glm::clamp(ctrLat, -latMax, latMax);
}

// ── drawBackground ────────────────────────────────────────────────────────────
void Planisphere::drawBackground(DynBuf2D& buf, GLint locColor,
                                  int splitX, int winW, int winH) const
{
    // Le fond commence à panelTop (pas à 0) pour ne pas recouvrir le menu.
    auto bg = make_rect(static_cast<float>(splitX),
                        static_cast<float>(panelTop),
                        static_cast<float>(winW),
                        static_cast<float>(winH));
    draw_2d(buf, bg, GL_TRIANGLES, locColor, { 0.04f, 0.07f, 0.13f, 1.0f });
}

// ── drawGraticule ─────────────────────────────────────────────────────────────
//
// STRATÉGIE :
//   Les lignes sont construites dans la plage de degrés VISIBLE,
//   sans se limiter à [-180°, +180°]. project() calcule les pixels
//   en coordonnées absolues → le scissor OpenGL découpe le reste.
//
//   4 tableaux de segments (GL_LINES = paires de sommets) :
//     fine      : 10° lon / 5° lat  sauf multiples de 30° / 15°
//     principal : 30° lon / 15° lat sauf Greenwich + équateur
//     greenwich : 0°, ±360°, … (méridien le plus épais)
//     equateur  : lat=0° (parallèle le plus épais)
void Planisphere::drawGraticule(DynBuf2D& buf, GLint locColor,
                                 int splitX, int winW, int winH) const
{
    // ── Plage visible (avec marge de 1° pour les bords) ──────────────────────
    const float hvl  = halfVisLon(splitX, winW, winH, 1.0f);
    const float hvla = halfVisLat(winH) + 0.5f;    // +0.5° pour ne pas couper aux pôles

    const float lonL = ctrLon - hvl;
    const float lonR = ctrLon + hvl;
    const float latB = ctrLat - hvla;   // bas du panneau
    const float latT = ctrLat + hvla;   // haut du panneau

    // ── Lambdas helpers ───────────────────────────────────────────────────────
    // Ajoute un méridien (segment vertical du bas au haut du panneau)
    auto addMeridian = [&](std::vector<float>& pts, float lon)
    {
        auto p0 = project(lon, latB, splitX, winW, winH);
        auto p1 = project(lon, latT, splitX, winW, winH);
        pts.push_back(p0.x); pts.push_back(p0.y);
        pts.push_back(p1.x); pts.push_back(p1.y);
    };

    // Ajoute un parallèle (segment horizontal du bord gauche au bord droit)
    auto addParallel = [&](std::vector<float>& pts, float lat)
    {
        auto p0 = project(lonL, lat, splitX, winW, winH);
        auto p1 = project(lonR, lat, splitX, winW, winH);
        pts.push_back(p0.x); pts.push_back(p0.y);
        pts.push_back(p1.x); pts.push_back(p1.y);
    };

    // ─────────────────────────────────────────────────────────────────────────
    // NIVEAU 1 — Grille fine : 10° lon, 5° lat
    // ─────────────────────────────────────────────────────────────────────────
    {
        std::vector<float> pts;

        int lo0 = static_cast<int>(std::floor(lonL / 10.0)) * 10;
        int lo1 = static_cast<int>(std::ceil (lonR / 10.0)) * 10;
        for (int lo = lo0; lo <= lo1; lo += 10)
            if (lo % 30 != 0)
                addMeridian(pts, static_cast<float>(lo));

        int la0 = static_cast<int>(std::floor(std::max(latB, -90.0f) / 5.0)) * 5;
        int la1 = static_cast<int>(std::ceil (std::min(latT,  90.0f) / 5.0)) * 5;
        for (int la = la0; la <= la1; la += 5)
            if (la % 15 != 0)
                addParallel(pts, static_cast<float>(la));

        glLineWidth(1.0f);
        draw_2d(buf, pts, GL_LINES, locColor, { 0.08f, 0.17f, 0.30f, 1.0f });
    }

    // ─────────────────────────────────────────────────────────────────────────
    // NIVEAU 2 — Grille principale : 30° lon, 15° lat
    // Exclut Greenwich (lon % 360 == 0) ET l'équateur (la == 0).
    // ─────────────────────────────────────────────────────────────────────────
    {
        std::vector<float> pts;

        int lo0 = static_cast<int>(std::floor(lonL / 30.0)) * 30;
        int lo1 = static_cast<int>(std::ceil (lonR / 30.0)) * 30;
        for (int lo = lo0; lo <= lo1; lo += 30)
            if (lo % 360 != 0)
                addMeridian(pts, static_cast<float>(lo));

        int la0 = static_cast<int>(std::floor(std::max(latB, -90.0f) / 15.0)) * 15;
        int la1 = static_cast<int>(std::ceil (std::min(latT,  90.0f) / 15.0)) * 15;
        for (int la = la0; la <= la1; la += 15)
            if (la != 0)   // équateur dessiné au niveau 4
                addParallel(pts, static_cast<float>(la));

        glLineWidth(1.0f);
        draw_2d(buf, pts, GL_LINES, locColor, { 0.18f, 0.36f, 0.58f, 1.0f });
    }

    // ─────────────────────────────────────────────────────────────────────────
    // NIVEAU 3 — Méridien de Greenwich : lon = 0°, ±360°, …
    // Trait le plus épais, dessiné en dernier (au-dessus des autres).
    // ─────────────────────────────────────────────────────────────────────────
    {
        std::vector<float> pts;
        int lo0 = static_cast<int>(std::floor(lonL / 360.0)) * 360;
        int lo1 = static_cast<int>(std::ceil (lonR / 360.0)) * 360;
        for (int lo = lo0; lo <= lo1; lo += 360)
            addMeridian(pts, static_cast<float>(lo));

        glLineWidth(2.0f);
        draw_2d(buf, pts, GL_LINES, locColor, { 0.40f, 0.66f, 0.92f, 1.0f });
        glLineWidth(1.0f);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // NIVEAU 4 — Équateur : lat = 0°
    // Même couleur et épaisseur que Greenwich — dessiné par-dessus le reste.
    // ─────────────────────────────────────────────────────────────────────────
    {
        std::vector<float> pts;
        // L'équateur est un seul parallèle, mais on vérifie qu'il est visible.
        if (latB <= 0.0f && latT >= 0.0f)
            addParallel(pts, 0.0f);

        if (!pts.empty()) {
            glLineWidth(2.0f);
            draw_2d(buf, pts, GL_LINES, locColor, { 0.40f, 0.66f, 0.92f, 1.0f });
            glLineWidth(1.0f);
        }
    }
}

// ── drawWaypoint ──────────────────────────────────────────────────────────────
//
// LOOP : dessine autant de copies horizontales du marqueur que nécessaire pour
//   couvrir la vue entière (scroll infini). Le nombre de copies est calculé
//   dynamiquement depuis halfVisLon(). Le scissor OpenGL gère le clipping.
//
// FORME :
//   WPShape::Hexagon → hexagone régulier en GL_LINE_STRIP (7 sommets)
//   WPShape::Cross   → croix (+)         en GL_LINES      (4 sommets, 2 segments)
void Planisphere::drawWaypoint(DynBuf2D& buf, GLint locColor,
                                int splitX, int winW, int winH,
                                const WayPoint& wp) const
{
    const float margeDegs = wp.radius2D / pixPerDeg(winH) + 2.0f;
    const float hvl = halfVisLon(splitX, winW, winH, margeDegs);

    // Calcul dynamique du nombre de copies nécessaires pour couvrir la vue entière.
    const int nMin = static_cast<int>(std::floor((ctrLon - hvl - wp.lon) / 360.0f));
    const int nMax = static_cast<int>(std::ceil ((ctrLon + hvl - wp.lon) / 360.0f));

    for (int n = nMin; n <= nMax; ++n)
    {
        float lon = wp.lon + n * 360.0f;

        if (lon < ctrLon - hvl || lon > ctrLon + hvl) continue;

        glm::vec2 c = project(lon, wp.lat, splitX, winW, winH);
        const float r = wp.radius2D;

        if (wp.shape == WPShape::Hexagon)
        {
            // ── Hexagone régulier ─────────────────────────────────────────────
            // angle_i = i × 60°, 7 sommets pour GL_LINE_STRIP (contour fermé)
            std::vector<float> hex;
            hex.reserve(14);
            for (int i = 0; i <= 6; ++i) {
                float a = i * glm::pi<float>() / 3.0f;
                hex.push_back(c.x + r * std::cos(a));
                hex.push_back(c.y + r * std::sin(a));
            }
            draw_2d(buf, hex, GL_LINE_STRIP, locColor, wp.color);
        }
        else // WPShape::Cross
        {
            // ── Croix (+) ─────────────────────────────────────────────────────
            // 2 segments : barre horizontale + barre verticale.
            // GL_LINES = paires de sommets indépendantes.
            std::vector<float> cross = {
                c.x - r, c.y,      c.x + r, c.y,    // barre est-ouest
                c.x,     c.y - r,  c.x,     c.y + r  // barre nord-sud
            };
            draw_2d(buf, cross, GL_LINES, locColor, wp.color);
        }
    }
}
