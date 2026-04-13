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
    // Les indices des 4 coins du quad sont :
    //   a = i*(slices+1)+j       b = a + (slices+1)
    //   a+1                      b+1
    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            unsigned a = i * (slices + 1) + j;
            unsigned b = a + (slices + 1);

            // Triangle 1 : a → b → a+1
            idx.push_back(a);   idx.push_back(b);   idx.push_back(a + 1);
            // Triangle 2 : a+1 → b → b+1
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

    // Vertices
    glBindBuffer(GL_ARRAY_BUFFER, g.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(verts.size() * sizeof(float)),
                 verts.data(), GL_STATIC_DRAW);   // GL_STATIC_DRAW : ne changera jamais

    // Indices
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
// À zoom=1 : 180° de latitude tiennent dans winH pixels → 1° = winH/180 px.
// À zoom=2 : les pixels sont deux fois plus grands → 1° = winH/180·2 px.
float Planisphere::pixPerDeg(int winH) const
{
    return (static_cast<float>(winH) / 180.0f) * zoom;
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
    float ppd    = pixPerDeg(winH);

    // Abscisse du centre du panneau droit en pixels absolus
    float cx = static_cast<float>(splitX) + static_cast<float>(panelW) * 0.5f;
    // Ordonnée du centre de la fenêtre (y=0 en haut)
    float cy = static_cast<float>(winH) * 0.5f;

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
//     cy − (90 − ctrLat) × ppd ≤ 0
//     (90 − ctrLat) × ppd ≥ cy = winH/2
//     90 − ctrLat ≥ winH/(2·ppd) = 90/zoom
//     ctrLat ≤ 90 − 90/zoom
//   Idem pour le pôle Sud par symétrie → ctrLat ≥ −(90 − 90/zoom).
void Planisphere::clampLat()
{
    float latMax = std::max(0.0f, 90.0f - 90.0f / zoom);
    ctrLat = glm::clamp(ctrLat, -latMax, latMax);
}

// ── drawBackground ────────────────────────────────────────────────────────────
void Planisphere::drawBackground(DynBuf2D& buf, GLint locColor,
                                  int splitX, int winW, int winH) const
{
    auto bg = make_rect(static_cast<float>(splitX), 0.0f,
                        static_cast<float>(winW),   static_cast<float>(winH));
    draw_2d(buf, bg, GL_TRIANGLES, locColor, { 0.04f, 0.07f, 0.13f, 1.0f });
}

// ── drawGraticule ─────────────────────────────────────────────────────────────
//
// STRATÉGIE :
//   Les lignes sont construites dans la plage de degrés VISIBLE,
//   sans se limiter à [-180°, +180°]. project() calcule les pixels
//   en coordonnées absolues → le scissor OpenGL découpe le reste.
//
//   3 tableaux de segments (GL_LINES = paires de sommets) :
//     fine     : 10° lon / 5° lat  sauf multiples de 30° / 15°
//     principal: 30° lon / 15° lat sauf multiples de 360° (=Greenwich)
//     greenwich: 0°, ±360°, … (méridien le plus épais)
//
//   Les méridiens (verticaux) vont du bord bas au bord haut du panneau.
//   Les parallèles (horizontaux) vont du bord gauche au bord droit.
void Planisphere::drawGraticule(DynBuf2D& buf, GLint locColor,
                                 int splitX, int winW, int winH) const
{
    // ── Plage visible (avec marge de 1° pour les bords) ──────────────────────
    const float hvl  = halfVisLon(splitX, winW, winH, 1.0f);
    const float hvla = halfVisLat(winH) + 0.5f;    // +0.5° pour ne pas couper les traits aux pôles

    const float lonL = ctrLon - hvl;
    const float lonR = ctrLon + hvl;
    const float latB = ctrLat - hvla;   // bas du panneau  (lat la plus petite visible)
    const float latT = ctrLat + hvla;   // haut du panneau (lat la plus grande visible)

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
    // Exclut les multiples de 30° (lon) et 15° (lat) → dessinés au niveau 2.
    // ─────────────────────────────────────────────────────────────────────────
    {
        std::vector<float> pts;

        // Méridiens fins : multiples de 10° dans la plage visible
        // std::floor/ceil pour inclure les lignes juste hors marge
        int lo0 = static_cast<int>(std::floor(lonL / 10.0)) * 10;
        int lo1 = static_cast<int>(std::ceil (lonR / 10.0)) * 10;
        for (int lo = lo0; lo <= lo1; lo += 10) {
            if (lo % 30 != 0)   // skip : sera dessiné au niveau 2
                addMeridian(pts, static_cast<float>(lo));
        }

        // Parallèles fins : multiples de 5° dans la plage visible
        // Les latitudes sont bornées à ±90° (pas de sens au-delà des pôles)
        int la0 = static_cast<int>(std::floor(std::max(latB, -90.0f) / 5.0)) * 5;
        int la1 = static_cast<int>(std::ceil (std::min(latT,  90.0f) / 5.0)) * 5;
        for (int la = la0; la <= la1; la += 5) {
            if (la % 15 != 0)   // skip : sera dessiné au niveau 2
                addParallel(pts, static_cast<float>(la));
        }

        glLineWidth(1.0f);
        draw_2d(buf, pts, GL_LINES, locColor, { 0.08f, 0.17f, 0.30f, 1.0f });
    }

    // ─────────────────────────────────────────────────────────────────────────
    // NIVEAU 2 — Grille principale : 30° lon, 15° lat  (surbrillance)
    // Exclut le méridien de Greenwich (lon = k × 360°) → dessiné au niveau 3.
    // ─────────────────────────────────────────────────────────────────────────
    {
        std::vector<float> pts;

        // Méridiens principaux : multiples de 30°, sauf multiples de 360°
        int lo0 = static_cast<int>(std::floor(lonL / 30.0)) * 30;
        int lo1 = static_cast<int>(std::ceil (lonR / 30.0)) * 30;
        for (int lo = lo0; lo <= lo1; lo += 30) {
            if (lo % 360 != 0)  // skip Greenwich et ses copies ±360°
                addMeridian(pts, static_cast<float>(lo));
        }

        // Parallèles principaux : multiples de 15°
        int la0 = static_cast<int>(std::floor(std::max(latB, -90.0f) / 15.0)) * 15;
        int la1 = static_cast<int>(std::ceil (std::min(latT,  90.0f) / 15.0)) * 15;
        for (int la = la0; la <= la1; la += 15)
            addParallel(pts, static_cast<float>(la));

        // Même épaisseur de trait, couleur plus lumineuse pour la distinction
        glLineWidth(1.0f);
        draw_2d(buf, pts, GL_LINES, locColor, { 0.18f, 0.36f, 0.58f, 1.0f });
    }

    // ─────────────────────────────────────────────────────────────────────────
    // NIVEAU 3 — Méridien de Greenwich : lon = 0°, ±360°, ±720°, …
    // Trait le plus épais, dessiné en dernier (donc au-dessus des autres).
    // Gestion du loop : on cherche tous les multiples de 360° dans la plage.
    // ─────────────────────────────────────────────────────────────────────────
    {
        std::vector<float> pts;

        int lo0 = static_cast<int>(std::floor(lonL / 360.0)) * 360;
        int lo1 = static_cast<int>(std::ceil (lonR / 360.0)) * 360;
        for (int lo = lo0; lo <= lo1; lo += 360)
            addMeridian(pts, static_cast<float>(lo));

        glLineWidth(2.0f);  // glLineWidth > 1.0 est supporté sur la plupart des drivers
        draw_2d(buf, pts, GL_LINES, locColor, { 0.40f, 0.66f, 0.92f, 1.0f });
        glLineWidth(1.0f);  // reset systématique pour ne pas affecter le prochain draw
    }
}

// ── drawWaypoint ──────────────────────────────────────────────────────────────
//
// LOOP : dessine 3 copies horizontales du marqueur (n = -1, 0, +1),
//   soit aux longitudes  lon − 360°, lon, lon + 360°.
//   Les copies hors de la plage visible sont ignorées.
//   Le scissor OpenGL gère de toute façon le clipping.
//
// PROPORTIONS :
//   Le rayon wp.radius2D est en pixels écran.
//   La projection ortho utilisée en 2D mappe 1 unité = 1 pixel dans les
//   deux directions. L'hexagone calculé avec cos/sin en pixels est donc
//   régulier quel que soit le ratio ou le zoom du panneau.
void Planisphere::drawWaypoint(DynBuf2D& buf, GLint locColor,
                                int splitX, int winW, int winH,
                                const WayPoint& wp) const
{
    // Marge : rayon du marqueur converti en degrés (pour test de visibilité)
    const float margeDegs = wp.radius2D / pixPerDeg(winH) + 2.0f;
    const float hvl = halfVisLon(splitX, winW, winH, margeDegs);

    for (int n = -1; n <= 1; ++n)
    {
        float lon = wp.lon + n * 360.0f;

        // Test de visibilité : si le centre est hors plage, on skip
        if (lon < ctrLon - hvl || lon > ctrLon + hvl) continue;

        // Centre du marqueur en pixels écran
        glm::vec2 c = project(lon, wp.lat, splitX, winW, winH);

        // Construction de l'hexagone en pixels :
        //   angle_i = i × 60°   (hexagone régulier si unités x = unités y)
        //   sommet_i = centre + r × (cos(angle_i), sin(angle_i))
        const float r = wp.radius2D;
        std::vector<float> hex;
        hex.reserve(14);    // 7 points × 2 coordonnées
        for (int i = 0; i <= 6; ++i) {
            float a = i * glm::pi<float>() / 3.0f;
            hex.push_back(c.x + r * std::cos(a));
            hex.push_back(c.y + r * std::sin(a));
        }
        // GL_LINE_STRIP relie les 7 points dans l'ordre → hexagone fermé
        draw_2d(buf, hex, GL_LINE_STRIP, locColor, wp.color);
    }
}
