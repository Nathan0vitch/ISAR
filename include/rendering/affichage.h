#pragma once
// =============================================================================
// affichage.h — Structures et classes de rendu, OrbitalSim
//
// Ce fichier regroupe tout ce qui concerne l'AFFICHAGE :
//   • WPShape     : forme du marqueur d'un WayPoint (hexagone ou croix)
//   • WayPoint    : repère géographique (ville, pôle, site d'atterrissage, etc.)
//   • DynBuf2D/3D : wrappers minces autour des buffers GPU dynamiques
//   • SphereGPU   : sphère UV uploadée sur GPU (positions + normales)
//   • Graticule3DGPU : quadrillage 3D statique (3 niveaux + surbrillances)
//   • Planisphere : état + rendu de la vue carte 2D (panneau droit)
//   • make_rect() : helper géométrique
//
// SÉPARATION DES RESPONSABILITÉS :
//   main.cpp       → fenêtre, OpenGL, callbacks, boucle de rendu
//   affichage.cpp  → logique de projection, construction des vertices, upload GPU
// =============================================================================

#include <glad/glad.h>          // API OpenGL (types GLuint, GLenum, GLint…)
#include <glm/glm.hpp>          // vec2, vec3, vec4, mat4

#include <vector>
#include <string>


// =============================================================================
// WPShape — Forme du marqueur d'un WayPoint
// =============================================================================
//
// Hexagon : hexagone régulier dessiné en GL_LINE_STRIP (7 sommets, contour fermé)
// Cross   : croix (+) dessinée en GL_LINES (2 segments, 4 sommets)
//           → utilisée pour les pôles géographiques
enum class WPShape { Hexagon, Cross };


// =============================================================================
// WayPoint — Repère géographique affiché sur le planisphère ET sur la sphère 3D
// =============================================================================
//
// Un WayPoint est un point nommé à la surface de la Terre.
// Sur le planisphère, il s'affiche comme un hexagone ou une croix centré sur
// ses coordonnées.
// Sur la sphère 3D, il s'affiche dans le plan tangent à la surface.
//
// Usage :
//   WayPoint paris { "Paris", 48.85f, 2.35f, {1.f, 0.85f, 0.2f, 1.f} };
//   WayPoint nord  { "Nord",  90.0f,  0.0f,  {1.f, 0.2f,  0.2f, 1.f},
//                    12.0f, 0.12f, WPShape::Cross };
struct WayPoint
{
    std::string name;           // Nom du lieu (usage futur : tooltip, liste)
    float       lat;            // Latitude  en degrés géographiques  (N > 0)
    float       lon;            // Longitude en degrés géographiques  (E > 0)
    glm::vec4   color;          // Couleur RGBA  [0, 1]
    float       radius2D = 8.0f;    // Rayon du marqueur sur le planisphère, en PIXELS ÉCRAN
                                    // → indépendant du zoom et de la taille du panneau
    float       radius3D = 0.055f;  // Rayon du marqueur sur la sphère 3D
                                    // (fraction du rayon de la sphère)
    WPShape     shape    = WPShape::Hexagon;  // Forme du marqueur

    // -------------------------------------------------------------------------
    // hexSphere() — Génère les 7 sommets (x,y,z) de l'hexagone 3D.
    //
    // L'hexagone est dessiné dans le PLAN TANGENT à la sphère au point (lat, lon).
    // Ce plan est défini par deux vecteurs orthogonaux :
    //   east  = vecteur "Est"  local  (dérivée de la position par rapport à lon)
    //   north = vecteur "Nord" local  (dérivée de la position par rapport à lat)
    //
    // Les 7 points forment un contour fermé pour GL_LINE_STRIP.
    // Ils sont légèrement décalés au-dessus de la surface (× 1.005) pour
    // éviter le z-fighting avec la sphère.
    std::vector<float> hexSphere() const;

    // -------------------------------------------------------------------------
    // crossSphere() — Génère les 4 sommets (x,y,z) de la croix 3D.
    //
    // La croix (+) est composée de 2 segments dans le plan tangent :
    //   barre horizontale : centre − r·east  →  centre + r·east
    //   barre verticale   : centre − r·north →  centre + r·north
    //
    // Retournés dans l'ordre pour GL_LINES (4 sommets = 2 paires).
    // Légèrement décalés au-dessus de la surface (× 1.005) comme pour l'hexagone.
    std::vector<float> crossSphere() const;

    // -------------------------------------------------------------------------
    // markerSphere() — Dispatche vers hexSphere() ou crossSphere() selon `shape`.
    //
    // À utiliser dans la boucle de rendu pour éviter le branchement explicite.
    std::vector<float> markerSphere() const;

    // -------------------------------------------------------------------------
    // glMode() — Mode OpenGL correspondant à la forme (GL_LINE_STRIP ou GL_LINES).
    GLenum glMode() const;
};


// =============================================================================
// DynBuf2D — Buffer GPU pour des sommets 2D (vec2), rechargé chaque frame
// =============================================================================
//
// Utilisé pour le planisphère : fond, graticule, hexagones, séparateur.
//
// FONCTIONNEMENT OPENGL :
//   • glBufferData(..., GL_DYNAMIC_DRAW) indique au driver que le buffer
//     est souvent mis à jour → il peut l'optimiser en RAM rapide (VRAM).
//   • Le VAO mémorise le format des attributs (attrib 0 = vec2).
struct DynBuf2D { GLuint vao = 0, vbo = 0; };

// Crée et configure le VAO / VBO.
DynBuf2D make_dyn2d();

// Upload `pts` (liste de vec2 en pixels écran) dans le buffer et dessine.
// pts format : [x0,y0,  x1,y1,  x2,y2, ...]
void draw_2d(DynBuf2D& d, const std::vector<float>& pts,
             GLenum mode, GLint locColor, glm::vec4 col);


// =============================================================================
// DynBuf3D — Buffer GPU pour des sommets 3D (vec3), rechargé chaque frame
// =============================================================================
//
// Utilisé pour les éléments 3D dynamiques (marqueurs de WayPoints).
struct DynBuf3D { GLuint vao = 0, vbo = 0; };

DynBuf3D make_dyn3d();

// Upload `pts` (liste de vec3) dans le buffer et dessine.
// pts format : [x0,y0,z0,  x1,y1,z1, ...]
void draw_3d(DynBuf3D& d, const std::vector<float>& pts,
             GLenum mode, GLint locColor, glm::vec4 col);


// =============================================================================
// SphereGPU — Sphère UV uploadée sur GPU (positions + normales)
// =============================================================================
//
// CONVENTION D'AXES :
//   Y = axe de rotation terrestre (pôle Nord → Y > 0)
//   Plan XZ = plan équatorial
//
// FORMAT DES VERTICES (stride = 6 floats) :
//   [x, y, z, nx, ny, nz]    (position + normale — identiques pour une sphère centrée)
//
// Attrib 0 (location=0) : position  (3 floats, offset 0)
// Attrib 1 (location=1) : normale   (3 floats, offset 12 octets)
struct SphereGPU { GLuint vao = 0, vbo = 0, ebo = 0; GLsizei count = 0; };

// Génère et uploade une sphère UV de rayon `r`.
// stacks : subdivisions en latitude  (suggestion : 48)
// slices : subdivisions en longitude (suggestion : 64)
SphereGPU build_sphere(float r, int stacks, int slices);


// =============================================================================
// Graticule3DGPU — Quadrillage 3D statique (géométrie uploadée une seule fois)
// =============================================================================
//
// Le quadrillage reprend exactement les 3 niveaux du planisphère + équateur :
//   fine      : méridiens tous les 10° (sauf mult. 30°), parallèles tous les 5°
//               (sauf mult. 15°)  — couleur sombre
//   major     : méridiens tous les 30° (sauf Greenwich), parallèles tous les 15°
//               (sauf équateur)   — couleur moyenne
//   highlight : méridien de Greenwich (lon=0°) + équateur (lat=0°)
//               — trait épais, couleur vive
//
// Les lignes sont stockées en GL_LINES (paires de sommets) dans des VBOs
// séparés, uploadés avec GL_STATIC_DRAW (géométrie fixe).
//
// Chaque arc est découpé en `seg` segments pour paraître lisse.
struct Graticule3DGPU
{
    GLuint vaoFine = 0, vboFine = 0;   GLsizei cntFine = 0;
    GLuint vaoMaj  = 0, vboMaj  = 0;   GLsizei cntMaj  = 0;
    GLuint vaoHl   = 0, vboHl   = 0;   GLsizei cntHl   = 0;
};

// Génère et uploade le quadrillage 3D.
// r   : rayon des lignes (légèrement > rayon de la sphère pour éviter le z-fighting)
// seg : subdivisions par arc (suggestion : 72)
Graticule3DGPU build_graticule3D(float r, int seg);

// Dessine le quadrillage avec le shader Flat3D actif (uMVP doit déjà être envoyé).
// locColor : location de l'uniforme vec4 uColor dans le shader actif
void draw_graticule3D(const Graticule3DGPU& g, GLint locColor);


// =============================================================================
// Helpers géométrie
// =============================================================================

// Rectangle plein [x0,y0] → [x1,y1], retourné comme 2 triangles (6 sommets 2D).
// Ordre : sens horaire en espace écran (y=0 en haut).
std::vector<float> make_rect(float x0, float y0, float x1, float y1);


// =============================================================================
// Planisphere — Gestion de l'état et du rendu de la vue carte 2D (panneau droit)
// =============================================================================
//
// ── PROJECTION ÉQUIRECTANGULAIRE ────────────────────────────────────────────
//
//   La projection équirectangulaire (ou "plate-carrée") est la plus simple :
//   elle mappe linéairement les degrés de longitude et latitude en pixels.
//
//   x_écran = splitX + panelW/2 + (lon − ctrLon) × pixPerDeg
//   y_écran = winH/2            − (lat − ctrLat) × pixPerDeg
//
//   où  pixPerDeg = (winH / 180°) × zoom
//                 = nombre de pixels par degré (même valeur en x et en y)
//
//   CONSÉQUENCE : 1° lon = 1° lat = même taille en pixels.
//   L'hexagone dessiné avec un rayon en pixels sera toujours régulier.
//
// ── LOOP HORIZONTAL ─────────────────────────────────────────────────────────
//
//   ctrLon N'EST PAS borné à [-180°, 180°].
//   En faisant défiler vers la droite, on accumule des degrés (ex : ctrLon = 450°).
//   Les éléments sont dessinés pour plusieurs copies (n = −1, 0, +1) :
//     lon_copie = lon_réelle + n × 360°
//   Le scissor OpenGL découpe automatiquement ce qui dépasse du panneau.
//
// ── CONTRAINTE VERTICALE ────────────────────────────────────────────────────
//
//   zoom ≥ ZOOM_MIN = 1  →  la carte fait au moins winH pixels de haut.
//                            Il n'y a pas de zone vide en haut ou en bas.
//
//   clampLat() impose  |ctrLat| ≤ 90° − 90°/zoom
//   Preuve : le pôle Nord (lat = 90°) se trouve en y_écran = winH/2 − (90−ctrLat)×ppd
//            Pour y_écran ≤ 0 (pôle hors fenêtre), il faut ctrLat ≤ 90 − 90/zoom.
//
class Planisphere
{
public:
    float ctrLon = 0.0f;    // Longitude centre de vue (°, NON borné — gère le loop)
    float ctrLat = 0.0f;    // Latitude  centre de vue (°)
    float zoom   = 1.0f;    // Facteur de zoom

    static constexpr float ZOOM_MIN = 1.0f;    // Carte ≥ hauteur fenêtre
    static constexpr float ZOOM_MAX = 25.0f;   // Zoom maximum

    // ── Maths projection ──────────────────────────────────────────────────────

    // Pixels par degré = (winH / 180) × zoom.
    // Identique en x et en y → garantit des proportions constantes.
    float pixPerDeg(int winH) const;

    // Demi-largeur visible en longitude (°).
    // margin (°) est ajoutée des deux côtés pour que les éléments en bord
    // de panneau soient toujours dessinés même partiellement visibles.
    float halfVisLon(int splitX, int winW, int winH, float margin = 0.0f) const;

    // Demi-hauteur visible en latitude (°) = 90 / zoom.
    float halfVisLat(int winH) const;

    // Convertit (lon°, lat°) → coordonnées PIXEL ÉCRAN absolues.
    //   splitX : position x du séparateur 3D/planisphère (début du panneau droit)
    //   winW, winH : dimensions du framebuffer
    glm::vec2 project(float lon, float lat, int splitX, int winW, int winH) const;

    // ── Contraintes ───────────────────────────────────────────────────────────

    // Clamp ctrLat pour que les pôles restent hors de la fenêtre.
    // À appeler après chaque modification de ctrLat ou zoom.
    void clampLat();

    // ── Dessin ────────────────────────────────────────────────────────────────

    // Fond uni de la zone planisphère (couleur océan sombre).
    void drawBackground(DynBuf2D&, GLint locColor,
                        int splitX, int winW, int winH) const;

    // Graticule en 4 niveaux :
    //   1. Fin      : toutes les 10° lon / 5° lat  — lignes fines, couleur sombre
    //   2. Principal: toutes les 30° lon / 15° lat — lignes moyennes, couleur claire
    //               (sauf méridien 0° et équateur, dessinés en niveaux 3/4)
    //   3. Greenwich: méridien 0°+copies (±360°…)  — trait épais, couleur vive
    //   4. Équateur : parallèle 0°                 — trait épais, couleur vive
    //
    // Les méridiens s'étendent sur TOUTE la hauteur du panneau.
    // Les parallèles s'étendent sur TOUTE la largeur du panneau.
    // Le loop est géré : les méridiens sont tracés dans la plage visible
    // sans se limiter à [-180°, +180°].
    void drawGraticule(DynBuf2D&, GLint locColor,
                       int splitX, int winW, int winH) const;

    // Marqueur d'un WayPoint avec loop (3 copies horizontales).
    //   • Hexagone pour WPShape::Hexagon
    //   • Croix (+) pour WPShape::Cross
    //
    // PROPORTIONS CONSTANTES :
    //   wp.radius2D est en pixels écran.
    //   La projection ortho (1 unité = 1 pixel en x et en y) garantit
    //   que le marqueur reste régulier quel que soit le zoom ou la
    //   taille du panneau.
    void drawWaypoint(DynBuf2D&, GLint locColor,
                      int splitX, int winW, int winH,
                      const WayPoint& wp) const;
};
