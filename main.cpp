// =============================================================================
// main.cpp — OrbitalSim  (point d'entrée)
//
// Ce fichier contient UNIQUEMENT :
//   1. Initialisation de la fenêtre et d'OpenGL (GLFW + GLAD)
//   2. Variables d'état globales (caméra, split, carte)
//   3. Callbacks GLFW (clavier, souris, resize)
//   4. Compilation des shaders GLSL
//   5. Boucle de rendu principale
//
// La logique d'affichage (buffers GPU, projection, graticule, marqueurs)
// est dans include/rendering/affichage.h et src/rendering/affichage.cpp.
// =============================================================================

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Toutes les structures de rendu (WayPoint, Planisphere, DynBuf*, SphereGPU…)
#include "rendering/affichage.h"
#include "rendering/menu.h"

#include <iostream>
#include <vector>
#include <cmath>


// =============================================================================
// État global de l'application
// =============================================================================

// ── Dimensions du framebuffer ─────────────────────────────────────────────────
// Mis à jour dans framebuffer_size_callback.
static int WIN_W = 1400, WIN_H = 750;

// ── Split panneau 3D / planisphère ────────────────────────────────────────────
// splitFrac ∈ [0.15, 0.85] : fraction de WIN_W pour le panneau 3D (gauche).
// Par défaut à 0.5 → moitié/moitié.
static float splitFrac = 0.5f;

// ── Split vertical du panneau droit (menu / planisphère) ─────────────────────
// mapTopFrac ∈ [0, 0.85] : fraction de WIN_H à partir du haut où commence
// le planisphère dans le panneau droit.
// 0.0 → planisphère plein, menu entièrement caché derrière.
// Augmenter → on révèle le menu au-dessus du planisphère.
static float mapTopFrac = 0.0f;

// ── Instance du menu ──────────────────────────────────────────────────────────
static Menu gMenu;

// ── Caméra 3D (arcball) ───────────────────────────────────────────────────────
// L'arcball place la caméra sur une sphère centrée sur l'origine.
// camYaw  : rotation autour de l'axe Y (gauche/droite)
// camPitch: rotation autour de l'axe X (haut/bas), clampé à ±89°
static float camYaw    = 30.0f;
static float camPitch  = 20.0f;
static float camRadius =  3.0f;

// ── Planisphère ───────────────────────────────────────────────────────────────
// gMap gère l'état (centre, zoom) et expose les méthodes de dessin.
static Planisphere gMap;

// ── WayPoints (repères géographiques) ────────────────────────────────────────
// Initialisés dans main() avant la boucle de rendu.
static std::vector<WayPoint> gWaypoints;

// ── Souris ────────────────────────────────────────────────────────────────────
static bool  lmbDown    = false;
static float lastMX     = 0.0f, lastMY = 0.0f;
static bool  dragIn3D   = false;  // true → le drag courant a commencé dans le panneau 3D
static bool  dragSplit  = false;  // true → le drag courant déplace le séparateur vertical
static bool  dragMapTop = false;  // true → le drag courant déplace le séparateur horizontal

// ── GLFW ──────────────────────────────────────────────────────────────────────
static GLFWwindow* gWindow     = nullptr;
static GLFWcursor* gCurResize  = nullptr;   // curseur ↔ (séparateur vertical)
static GLFWcursor* gCurVResize = nullptr;   // curseur ↕ (séparateur horizontal)
static GLFWcursor* gCurArrow   = nullptr;   // curseur flèche normal


// =============================================================================
// Helpers
// =============================================================================

// Position du séparateur vertical en pixels (calculée depuis la fraction)
static float splitX_px() { return splitFrac * static_cast<float>(WIN_W); }

// Position du séparateur horizontal (haut du planisphère) en pixels
static float mapTopY_px() { return mapTopFrac * static_cast<float>(WIN_H); }

// Retourne true si le curseur (x) est à moins de 6 px du séparateur vertical
static bool nearSplit(double x) {
    return std::abs(static_cast<float>(x) - splitX_px()) < 6.0f;
}

// Retourne true si le curseur est dans le panneau planisphère (droite)
static bool inMapPanel(double x) {
    return static_cast<float>(x) > splitX_px();
}

// Retourne true si le curseur est sur le séparateur horizontal du planisphère
// (dans le panneau droit, à moins de 6 px du bord supérieur du planisphère)
static bool nearMapTopSplit(double x, double y) {
    if (!inMapPanel(x)) return false;
    return std::abs(static_cast<float>(y) - mapTopY_px()) < 6.0f;
}


// =============================================================================
// Callbacks GLFW
// =============================================================================

// ── Redimensionnement ─────────────────────────────────────────────────────────
static void framebuffer_size_callback(GLFWwindow*, int w, int h)
{
    WIN_W = w;
    WIN_H = h;
    // Après resize, re-clamper la latitude (le zoom_min peut avoir changé
    // si la fenêtre est plus haute, mais en pratique WIN_H est fixé ici).
    gMap.clampLat();
}

// ── Bouton souris ─────────────────────────────────────────────────────────────
static void mouse_button_callback(GLFWwindow* window, int button, int action, int)
{
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;

    if (action == GLFW_PRESS)
    {
        double cx, cy;
        glfwGetCursorPos(window, &cx, &cy);
        lmbDown = true;

        if (nearSplit(cx)) {
            // ── Drag du séparateur vertical (3D / planisphère) ─────────────
            dragSplit  = true;
            dragMapTop = false;
            dragIn3D   = false;
            glfwSetCursor(window, gCurResize);
        } else if (nearMapTopSplit(cx, cy)) {
            // ── Drag du séparateur horizontal (menu / planisphère) ──────────
            dragMapTop = true;
            dragSplit  = false;
            dragIn3D   = false;
            glfwSetCursor(window, gCurVResize);
        } else {
            // ── Drag dans l'un des deux panneaux ───────────────────────────
            dragSplit  = false;
            dragMapTop = false;
            dragIn3D   = !inMapPanel(cx);   // true si panneau 3D (gauche)
        }
    }
    else
    {
        lmbDown    = false;
        dragSplit  = false;
        dragMapTop = false;
        // Le curseur sera remis à jour au prochain mousemove
    }
}

// ── Mouvement de la souris ────────────────────────────────────────────────────
static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos)
{
    float dx = static_cast<float>(xpos) - lastMX;
    float dy = static_cast<float>(ypos) - lastMY;

    if (dragSplit)
    {
        // ── Déplace le séparateur vertical ────────────────────────────────
        splitFrac = glm::clamp(static_cast<float>(xpos) / WIN_W, 0.15f, 0.85f);
    }
    else if (dragMapTop)
    {
        // ── Déplace le séparateur horizontal (haut du planisphère) ───────
        // 0 = planisphère en haut (menu caché), 0.85 = planisphère tout en bas.
        mapTopFrac = glm::clamp(static_cast<float>(ypos) / WIN_H, 0.0f, 0.85f);
    }
    else if (lmbDown)
    {
        if (dragIn3D)
        {
            // ── Rotation arcball ─────────────────────────────────────────
            // Axe horizontal INVERSÉ : souris droite → yaw diminue
            // (la sphère tourne dans le sens "naturel" comme si on la tirait)
            camYaw   -= dx * 0.4f;
            camPitch  = glm::clamp(camPitch + dy * 0.4f, -89.0f, 89.0f);
        }
        else
        {
            // ── Pan du planisphère ────────────────────────────────────────
            float ppd = gMap.pixPerDeg(WIN_H);

            // Souris vers la droite (dx > 0) → on avance vers la droite de
            // la carte → les longitudes croissantes arrivent → ctrLon diminue
            gMap.ctrLon -= dx / ppd;

            // Souris vers le bas (dy > 0) → en espace écran y↓, la carte
            // descend → les latitudes décroissantes arrivent → ctrLat augmente
            gMap.ctrLat += dy / ppd;

            // Applique le clamp lat (les pôles ne doivent pas entrer dans
            // la fenêtre). ctrLon n'est pas clampé → loop horizontal libre.
            gMap.clampLat();
        }
    }
    else
    {
        // ── Mise à jour du curseur selon la position ───────────────────────
        if (nearSplit(xpos))
            glfwSetCursor(window, gCurResize);
        else if (nearMapTopSplit(xpos, ypos))
            glfwSetCursor(window, gCurVResize);
        else
            glfwSetCursor(window, gCurArrow);
    }

    lastMX = static_cast<float>(xpos);
    lastMY = static_cast<float>(ypos);
}

// ── Molette ───────────────────────────────────────────────────────────────────
static void scroll_callback(GLFWwindow* window, double, double yoffset)
{
    double cx, cy;
    glfwGetCursorPos(window, &cx, &cy);

    if (!inMapPanel(cx))
    {
        // ── Zoom caméra 3D ────────────────────────────────────────────────
        // yoffset > 0 = scroll vers le haut = on s'approche
        camRadius = glm::clamp(camRadius - static_cast<float>(yoffset) * 0.25f,
                               1.2f, 10.0f);
    }
    else if (!nearSplit(cx))
    {
        // ── Zoom planisphère ──────────────────────────────────────────────
        float factor = (yoffset > 0) ? 1.15f : (1.0f / 1.15f);
        gMap.zoom = glm::clamp(gMap.zoom * factor,
                               Planisphere::ZOOM_MIN,
                               Planisphere::ZOOM_MAX);
        // Re-clamp lat après changement de zoom :
        // le "latMax autorisé" dépend du zoom.
        gMap.clampLat();
    }
}

// ── Clavier ───────────────────────────────────────────────────────────────────
static void key_callback(GLFWwindow* window, int key, int, int action, int)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}


// =============================================================================
// Sources GLSL
// =============================================================================

// ── Shader 3D : éclairage Phong (sphère terrestre) ────────────────────────────
//
// Vertex shader : transforme les positions et calcule les normales en
//   espace monde (pour l'éclairage).
// Fragment shader : modèle de Phong simplifié avec lumière directionnelle
//   (soleil) = rayons parallèles, pas d'atténuation.
//
// MODÈLE DE PHONG :
//   Lumière = ambient + diffuse + specular
//   ambient  = k_a · couleur                  (éclairage minimum, côté nuit)
//   diffuse  = k_d · max(N·L, 0) · couleur    (dépend de l'angle N/lumière)
//   specular = k_s · max(R·V, 0)^alpha        (brillance)
//
static const char* VERT3D = R"glsl(
#version 330 core

layout(location = 0) in vec3 aPos;     // Position locale du sommet
layout(location = 1) in vec3 aNormal;  // Normale locale du sommet

uniform mat4 uMVP;    // Model-View-Projection (transforme en clip-space)
uniform mat4 uModel;  // Matrice modèle seule (transforme en espace monde)

out vec3 vNormal;     // Normale en espace monde (pour le fragment shader)
out vec3 vFragPos;    // Position en espace monde (pour le calcul specular)

void main()
{
    // Position en espace monde (pour les calculs d'éclairage)
    vFragPos = vec3(uModel * vec4(aPos, 1.0));

    // Normale en espace monde.
    // On utilise la transposée de l'inverse du modèle pour gérer
    // correctement les normales en cas de mise à l'échelle non uniforme.
    vNormal = mat3(transpose(inverse(uModel))) * aNormal;

    // Position finale en clip-space (sortie obligatoire du vertex shader)
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)glsl";

static const char* FRAG3D = R"glsl(
#version 330 core

in  vec3 vNormal;    // Normale en espace monde
in  vec3 vFragPos;   // Position en espace monde

uniform vec3 uSunDir;  // Direction VERS le soleil (normalisée)
uniform vec3 uColor;   // Couleur de la surface

out vec4 FragColor;

void main()
{
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uSunDir);

    // Composante diffuse : Lambert — max(N·L, 0)
    // Vaut 0 sur le côté nuit, 1 face au soleil.
    float diff = max(dot(N, L), 0.0);

    // Composante ambiante (lumière indirecte, côté nuit non complètement noir)
    vec3 ambient  = 0.07 * uColor;

    // Composante diffuse
    vec3 diffuse  = diff * uColor;

    // Composante spéculaire (brillance, modèle de Blinn-Phong)
    // viewDir : direction vers la caméra depuis le fragment
    // halfDir : bissectrice entre la lumière et la vue
    vec3 viewDir  = normalize(-vFragPos);
    vec3 halfDir  = normalize(L + viewDir);
    float spec    = pow(max(dot(N, halfDir), 0.0), 64.0);
    vec3 specular = 0.18 * vec3(1.0) * spec;

    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
)glsl";

// ── Shader 3D Flat : couleur uniforme sans éclairage ─────────────────────────
// Utilisé pour les traits des pôles et l'hexagone 3D de Paris.
static const char* VERT_FLAT3D = R"glsl(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
void main() { gl_Position = uMVP * vec4(aPos, 1.0); }
)glsl";

static const char* FRAG_FLAT3D = R"glsl(
#version 330 core
uniform vec4 uColor;
out vec4 FragColor;
void main() { FragColor = uColor; }
)glsl";

// ── Shader 2D : couleur uniforme, espace pixel (planisphère, UI) ─────────────
// La projection ortho (passée via uProj) mappe les pixels écran → NDC.
static const char* VERT2D = R"glsl(
#version 330 core
layout(location = 0) in vec2 aPos;
uniform mat4 uProj;
void main() { gl_Position = uProj * vec4(aPos, 0.0, 1.0); }
)glsl";

static const char* FRAG2D = R"glsl(
#version 330 core
uniform vec4 uColor;
out vec4 FragColor;
void main() { FragColor = uColor; }
)glsl";


// =============================================================================
// Utilitaires shaders
// =============================================================================

static GLuint compile_shader(GLenum type, const char* src)
{
    GLuint id = glCreateShader(type);
    glShaderSource(id, 1, &src, nullptr);
    glCompileShader(id);

    GLint ok;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetShaderInfoLog(id, 512, nullptr, buf);
        std::cerr << "Erreur compilation shader : " << buf << "\n";
    }
    return id;
}

static GLuint create_program(const char* vert, const char* frag)
{
    GLuint vs  = compile_shader(GL_VERTEX_SHADER,   vert);
    GLuint fs  = compile_shader(GL_FRAGMENT_SHADER, frag);
    GLuint pgm = glCreateProgram();
    glAttachShader(pgm, vs);
    glAttachShader(pgm, fs);
    glLinkProgram(pgm);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return pgm;
}


// =============================================================================
// main()
// =============================================================================

int main()
{
    // ── Initialisation GLFW ───────────────────────────────────────────────────
    if (!glfwInit()) {
        std::cerr << "Échec glfwInit\n";
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);   // multisampling anti-aliasing (4x)

    gWindow = glfwCreateWindow(WIN_W, WIN_H, "OrbitalSim", nullptr, nullptr);
    if (!gWindow) {
        std::cerr << "Échec glfwCreateWindow\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(gWindow);
    glfwSwapInterval(1);   // VSync : limite le rendu à la fréquence de l'écran

    // Curseurs personnalisés
    gCurResize  = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);   // ↔ séparateur vertical
    gCurVResize = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);   // ↕ séparateur horizontal
    gCurArrow   = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);

    // ── Initialisation GLAD (charge les pointeurs de fonctions OpenGL) ────────
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::cerr << "Échec GLAD\n";
        return -1;
    }

    // ── Enregistrement des callbacks ──────────────────────────────────────────
    glfwSetFramebufferSizeCallback(gWindow, framebuffer_size_callback);
    glfwSetMouseButtonCallback    (gWindow, mouse_button_callback);
    glfwSetCursorPosCallback      (gWindow, cursor_pos_callback);
    glfwSetScrollCallback         (gWindow, scroll_callback);
    glfwSetKeyCallback            (gWindow, key_callback);

    // ── États OpenGL globaux ──────────────────────────────────────────────────
    glEnable(GL_DEPTH_TEST);    // Test de profondeur (objets cachés derrière)
    glEnable(GL_MULTISAMPLE);   // Active l'AA MSAA (requiert GLFW_SAMPLES > 0)
    glEnable(GL_SCISSOR_TEST);  // Permet de restreindre le rendu à une zone

    // ── Compilation des programmes shaders ───────────────────────────────────
    GLuint shader3D     = create_program(VERT3D,      FRAG3D);
    GLuint shaderFlat3D = create_program(VERT_FLAT3D, FRAG_FLAT3D);
    GLuint shader2D     = create_program(VERT2D,      FRAG2D);

    // Uniforms du shader 3D Phong
    GLint loc3D_MVP   = glGetUniformLocation(shader3D, "uMVP");
    GLint loc3D_Model = glGetUniformLocation(shader3D, "uModel");
    GLint loc3D_Sun   = glGetUniformLocation(shader3D, "uSunDir");
    GLint loc3D_Color = glGetUniformLocation(shader3D, "uColor");

    // Uniforms du shader Flat 3D (pôles, hexagone Paris)
    GLint locF_MVP    = glGetUniformLocation(shaderFlat3D, "uMVP");
    GLint locF_Color  = glGetUniformLocation(shaderFlat3D, "uColor");

    // Uniforms du shader 2D (planisphère)
    GLint loc2D_Proj  = glGetUniformLocation(shader2D, "uProj");
    GLint loc2D_Color = glGetUniformLocation(shader2D, "uColor");

    // ── Géométries ────────────────────────────────────────────────────────────
    // Sphère UV (48 stacks × 64 slices = bon compromis qualité/perf)
    SphereGPU sphere = build_sphere(1.0f, 48, 64);

    // Buffers dynamiques
    DynBuf2D dyn2 = make_dyn2d();  // pour le planisphère (2D)
    DynBuf3D dyn3 = make_dyn3d();  // pour les marqueurs WayPoint 3D non éclairés

    // Quadrillage 3D statique (uploadé une seule fois sur GPU)
    // r = 1.005 : légèrement au-dessus de la sphère pour éviter le z-fighting
    // 72 segments par arc → arcs lisses (~5° par sous-segment)
    Graticule3DGPU grat3D = build_graticule3D(1.005f, 72);

    // ── Direction du soleil ───────────────────────────────────────────────────
    // Le soleil est une lumière directionnelle (rayons parallèles).
    // La direction est fixe ici — elle pourra être animée plus tard (orbite).
    // normalize() garantit un vecteur unitaire (requis pour le dot product).
    const glm::vec3 sunDir = glm::normalize(glm::vec3(1.6f, 0.8f, 0.7f));

    // ── WayPoints ─────────────────────────────────────────────────────────────
    // Paris : hexagone or
    gWaypoints.push_back({
        "Paris",
        48.85f, 2.35f,
        { 1.0f, 0.85f, 0.2f, 1.0f },   // or
        8.0f, 0.055f,
        WPShape::Hexagon
    });

    // Pôle Nord : croix rouge (lat=90°, lon=0° — lon arbitraire aux pôles)
    gWaypoints.push_back({
        "Pole Nord",
        90.0f, 0.0f,
        { 1.0f, 0.22f, 0.18f, 1.0f },  // rouge
        10.0f, 0.12f,
        WPShape::Cross
    });

    // Pôle Sud : croix bleue
    gWaypoints.push_back({
        "Pole Sud",
        -90.0f, 0.0f,
        { 0.22f, 0.55f, 1.0f, 1.0f },  // bleu
        10.0f, 0.12f,
        WPShape::Cross
    });

    // Pré-calculer les marqueurs 3D (géométrie fixe, calculée une seule fois)
    std::vector<std::vector<float>> waypointMesh3D;
    for (const auto& wp : gWaypoints)
        waypointMesh3D.push_back(wp.markerSphere());

    // ── Boucle de rendu ───────────────────────────────────────────────────────
    while (!glfwWindowShouldClose(gWindow))
    {
        glfwPollEvents();   // Traite les événements GLFW (clavier, souris…)

        // Dimensions actuelles du framebuffer
        int fbW, fbH;
        glfwGetFramebufferSize(gWindow, &fbW, &fbH);

        // Position du séparateur horizontal (haut du planisphère) en pixels entiers
        int mapTopY = static_cast<int>(mapTopFrac * fbH);

        // Synchronise le panelTop du planisphère pour les calculs de projection
        gMap.panelTop = mapTopY;

        // Position du séparateur en pixels entiers
        int splitX = static_cast<int>(splitFrac * fbW);

        // ── Clear global ──────────────────────────────────────────────────────
        // Le scissor couvre toute la fenêtre pour le clear initial.
        glScissor(0, 0, fbW, fbH);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ══════════════════════════════════════════════════════════════════════
        // PANNEAU GAUCHE — Vue 3D
        // ══════════════════════════════════════════════════════════════════════
        // On restreint le viewport ET le scissor au panneau gauche.
        // glViewport : où OpenGL projette le clip-space (NDC → pixels)
        // glScissor  : zone où glClear et les draw calls peuvent écrire
        glViewport(0, 0, splitX, fbH);
        glScissor (0, 0, splitX, fbH);
        glEnable(GL_DEPTH_TEST);

        glClearColor(0.04f, 0.05f, 0.10f, 1.0f);  // fond bleu nuit (espace)
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ── Matrices 3D ───────────────────────────────────────────────────────
        // Caméra arcball : la caméra orbite autour de l'origine.
        float yRad = glm::radians(camYaw);
        float pRad = glm::radians(camPitch);
        glm::vec3 camPos = {
            camRadius * std::cos(pRad) * std::sin(yRad),
            camRadius * std::sin(pRad),
            camRadius * std::cos(pRad) * std::cos(yRad)
        };

        float     aspect3D = static_cast<float>(splitX) / static_cast<float>(fbH);
        glm::mat4 proj3D   = glm::perspective(glm::radians(45.0f), aspect3D, 0.1f, 100.0f);
        glm::mat4 view3D   = glm::lookAt(camPos, glm::vec3(0.0f), glm::vec3(0, 1, 0));
        glm::mat4 model3D  = glm::mat4(1.0f);   // identité : sphère centrée à l'origine
        glm::mat4 mvp3D    = proj3D * view3D * model3D;

        // ── Sphère terrestre (shader Phong) ───────────────────────────────────
        glUseProgram(shader3D);
        glUniformMatrix4fv(loc3D_MVP,   1, GL_FALSE, glm::value_ptr(mvp3D));
        glUniformMatrix4fv(loc3D_Model, 1, GL_FALSE, glm::value_ptr(model3D));
        glUniform3fv(loc3D_Sun,   1, glm::value_ptr(sunDir));
        glUniform3f (loc3D_Color, 0.14f, 0.40f, 0.80f);  // bleu Terre

        glBindVertexArray(sphere.vao);
        glDrawElements(GL_TRIANGLES, sphere.count, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);

        // ── Éléments 3D non éclairés (shader Flat) ────────────────────────────
        // Dessinés APRÈS la sphère pour apparaître au-dessus d'elle.
        glUseProgram(shaderFlat3D);
        glUniformMatrix4fv(locF_MVP, 1, GL_FALSE, glm::value_ptr(mvp3D));

        // Quadrillage 3D : 3 niveaux (fin / principal / surbrillance)
        // L'équateur et le méridien de Greenwich sont inclus dans la surbrillance.
        draw_graticule3D(grat3D, locF_Color);

        // Marqueurs WayPoint (Paris = hexagone, pôles = croix)
        for (int i = 0; i < static_cast<int>(gWaypoints.size()); ++i)
            draw_3d(dyn3, waypointMesh3D[i], gWaypoints[i].glMode(),
                    locF_Color, gWaypoints[i].color);

        // ── Projection ortho pleine fenêtre (partagée par menu et séparateurs) ──
        // Coordonnées écran : x ∈ [0, fbW], y ∈ [0, fbH] (y=0 en haut)
        glm::mat4 proj2D_full = glm::ortho(0.0f, static_cast<float>(fbW),
                                            static_cast<float>(fbH), 0.0f,
                                            -1.0f, 1.0f);

        glDisable(GL_DEPTH_TEST);   // tout le reste est 2D

        // ══════════════════════════════════════════════════════════════════════
        // MENU — Zone en haut à droite, au-dessus du planisphère
        // ══════════════════════════════════════════════════════════════════════
        // Rendu en premier (en arrière-plan) : le planisphère le recouvrira
        // si mapTopY < Menu::HEIGHT.
        // Scissor OpenGL (y depuis le bas du framebuffer) :
        //   zone menu en écran = [mapTopY .. 0] depuis le haut
        //            en OpenGL = [fbH - mapTopY .. fbH]
        if (mapTopY > 0)
        {
            glViewport(0, 0, fbW, fbH);
            glScissor(splitX, fbH - mapTopY, fbW - splitX, mapTopY);
            glUseProgram(shader2D);
            glUniformMatrix4fv(loc2D_Proj, 1, GL_FALSE, glm::value_ptr(proj2D_full));
            gMenu.draw(dyn2, loc2D_Color, splitX, fbW, fbH, mapTopY);
        }

        // ══════════════════════════════════════════════════════════════════════
        // PANNEAU DROIT — Planisphère
        // ══════════════════════════════════════════════════════════════════════
        // Le panneau planisphère occupe la zone [splitX, mapTopY] → [fbW, fbH]
        // en coordonnées écran (y depuis le haut).
        // En OpenGL (y depuis le bas) : viewport y ∈ [0, fbH - mapTopY].
        {
            int panelH = fbH - mapTopY;   // hauteur du panneau planisphère
            glViewport(splitX, 0, fbW - splitX, panelH);
            glScissor (splitX, 0, fbW - splitX, panelH);

            // Projection ortho calée sur le panneau planisphère :
            //   x : [splitX, fbW]       → NDC [−1, +1]
            //   y : [mapTopY, fbH]      → NDC [+1, −1]  (y écran croît vers le bas)
            // Avec ce mapping, project() retourne des coords pixels absolus dans
            // [splitX..fbW] × [mapTopY..fbH] et ils se projettent correctement.
            glm::mat4 proj2D_panel = glm::ortho(
                static_cast<float>(splitX),
                static_cast<float>(fbW),
                static_cast<float>(fbH),        // bottom (screen y max → NDC −1)
                static_cast<float>(mapTopY),    // top    (screen y min → NDC +1)
                -1.0f, 1.0f);

            glUseProgram(shader2D);
            glUniformMatrix4fv(loc2D_Proj, 1, GL_FALSE, glm::value_ptr(proj2D_panel));

            // Fond, graticule, marqueurs (via la classe Planisphere)
            gMap.drawBackground(dyn2, loc2D_Color, splitX, fbW, fbH);
            gMap.drawGraticule (dyn2, loc2D_Color, splitX, fbW, fbH);
            for (const auto& wp : gWaypoints)
                gMap.drawWaypoint(dyn2, loc2D_Color, splitX, fbW, fbH, wp);
        }

        // ══════════════════════════════════════════════════════════════════════
        // SÉPARATEURS + POIGNÉES
        // ══════════════════════════════════════════════════════════════════════
        // On repasse au viewport/scissor global pour dessiner par-dessus tout.
        glViewport(0, 0, fbW, fbH);
        glScissor (0, 0, fbW, fbH);
        glUseProgram(shader2D);
        glUniformMatrix4fv(loc2D_Proj, 1, GL_FALSE, glm::value_ptr(proj2D_full));

        {
            float sx  = static_cast<float>(splitX);
            float mty = static_cast<float>(mapTopY);
            float fw  = static_cast<float>(fbW);
            float fh  = static_cast<float>(fbH);

            // ── Séparateur VERTICAL (3D / planisphère) ─────────────────────
            // Barre fine 2 px, sur toute la hauteur
            draw_2d(dyn2, make_rect(sx - 1.0f, 0.0f, sx + 1.0f, fh),
                    GL_TRIANGLES, loc2D_Color, { 0.50f, 0.72f, 1.0f, 1.0f });
            // Poignée (8 × 40 px, centrée sur la hauteur de la fenêtre)
            float midV = fh * 0.5f;
            draw_2d(dyn2, make_rect(sx - 4.0f, midV - 20.0f, sx + 4.0f, midV + 20.0f),
                    GL_TRIANGLES, loc2D_Color, { 0.70f, 0.85f, 1.0f, 1.0f });

            // ── Séparateur HORIZONTAL (menu / planisphère, panneau droit) ──
            if (mapTopY > 0)
            {
                // Barre fine 2 px, sur toute la largeur du panneau droit
                draw_2d(dyn2, make_rect(sx, mty - 1.0f, fw, mty + 1.0f),
                        GL_TRIANGLES, loc2D_Color, { 0.50f, 0.72f, 1.0f, 1.0f });
                // Poignée (40 × 8 px, centrée horizontalement dans le panneau droit)
                float midH = sx + (fw - sx) * 0.5f;
                draw_2d(dyn2, make_rect(midH - 20.0f, mty - 4.0f, midH + 20.0f, mty + 4.0f),
                        GL_TRIANGLES, loc2D_Color, { 0.70f, 0.85f, 1.0f, 1.0f });
            }
        }

        // Échange les buffers front/back (double buffering)
        glfwSwapBuffers(gWindow);
    }

    // ── Nettoyage ─────────────────────────────────────────────────────────────
    glDeleteVertexArrays(1, &sphere.vao);
    glDeleteBuffers(1, &sphere.vbo);
    glDeleteBuffers(1, &sphere.ebo);
    glDeleteVertexArrays(1, &dyn2.vao); glDeleteBuffers(1, &dyn2.vbo);
    glDeleteVertexArrays(1, &dyn3.vao); glDeleteBuffers(1, &dyn3.vbo);
    glDeleteVertexArrays(1, &grat3D.vaoFine); glDeleteBuffers(1, &grat3D.vboFine);
    glDeleteVertexArrays(1, &grat3D.vaoMaj);  glDeleteBuffers(1, &grat3D.vboMaj);
    glDeleteVertexArrays(1, &grat3D.vaoHl);   glDeleteBuffers(1, &grat3D.vboHl);
    glDeleteProgram(shader3D);
    glDeleteProgram(shaderFlat3D);
    glDeleteProgram(shader2D);
    glfwDestroyCursor(gCurResize);
    glfwDestroyCursor(gCurVResize);
    glfwDestroyCursor(gCurArrow);
    glfwTerminate();
    return 0;
}
