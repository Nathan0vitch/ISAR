// =============================================================================
// main.cpp — OrbitalSim  (point d'entrée)
//
// Ce fichier contient UNIQUEMENT :
//   1. Initialisation de la fenêtre et d'OpenGL (GLFW + GLAD)
//   2. Variables d'état globales (caméra, split, carte, simulation)
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

// Dear ImGui
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "rendering/affichage.h"
#include "rendering/menu.h"
#include "simulation/Satellite.h"

#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <cstdio>   // snprintf
#include <algorithm>


// =============================================================================
// État global de l'application
// =============================================================================

// ── Dimensions du framebuffer ─────────────────────────────────────────────────
static int WIN_W = 1400, WIN_H = 750;

// ── Split panneau 3D / planisphère ────────────────────────────────────────────
static float splitFrac = 0.5f;

// ── Split vertical du panneau droit (menu / planisphère) ─────────────────────
static float mapTopFrac = 0.0f;

// ── Instance du menu ──────────────────────────────────────────────────────────
static Menu gMenu;

// ── Liste des satellites ──────────────────────────────────────────────────────
static std::vector<Satellite> gSatellites;

// ── Caméra 3D (arcball) ───────────────────────────────────────────────────────
static float camYaw    = 30.0f;
static float camPitch  = 20.0f;
static float camRadius =  3.0f;

// ── Planisphère ───────────────────────────────────────────────────────────────
static Planisphere gMap;

// ── WayPoints ─────────────────────────────────────────────────────────────────
static std::vector<WayPoint> gWaypoints;

// ── Souris ────────────────────────────────────────────────────────────────────
static bool  lmbDown    = false;
static float lastMX     = 0.0f, lastMY = 0.0f;
static bool  dragIn3D   = false;
static bool  dragSplit  = false;
static bool  dragMapTop = false;

// ── GLFW ──────────────────────────────────────────────────────────────────────
static GLFWwindow* gWindow     = nullptr;
static GLFWcursor* gCurResize  = nullptr;
static GLFWcursor* gCurVResize = nullptr;
static GLFWcursor* gCurArrow   = nullptr;

// =============================================================================
// État de la simulation temporelle
// =============================================================================
//
// simTime  : temps de simulation courant [s], ∈ [0, SIM_DURATION]
// simSpeed : multiplicateur de vitesse (valeurs : 1, 2, 4, …, 16384)
// simPaused: simulation en pause si true
// wallPrev : dernier glfwGetTime() mesuré (pour le dt)
static double simTime   = 0.0;
static double simSpeed  = 64.0;   // ×64 par défaut (~14s par orbite LEO)
static bool   simPaused = false;
static double wallPrev  = 0.0;

// ── Sélecteur de périodes affichées ──────────────────────────────────────────
// Indices : 0=1T  1=3T  2=5T  3=10T  4=tout (−1 périodes = pas de limite)
static int         gNbPeriodsIdx  = 0;
static const int   kPeriodMult[]  = { 1, 3, 5, 10, -1 };  // -1 = toutes
static const char* kPeriodLabels[]= { "1T", "3T", "5T", "10T", "Tout" };


// =============================================================================
// Helpers
// =============================================================================

static float splitX_px()  { return splitFrac * static_cast<float>(WIN_W); }
static float mapTopY_px() { return mapTopFrac * static_cast<float>(WIN_H); }

static bool nearSplit(double x) {
    return std::abs(static_cast<float>(x) - splitX_px()) < 6.0f;
}
static bool inMapPanel(double x) {
    return static_cast<float>(x) > splitX_px();
}
static bool nearMapTopSplit(double x, double y) {
    if (!inMapPanel(x)) return false;
    return std::abs(static_cast<float>(y) - mapTopY_px()) < 6.0f;
}


// =============================================================================
// Callbacks GLFW
// =============================================================================

static void framebuffer_size_callback(GLFWwindow*, int w, int h)
{
    WIN_W = w;
    WIN_H = h;
    gMap.clampLat();
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;
    if (ImGui::GetIO().WantCaptureMouse && !dragSplit && !dragMapTop) return;

    if (action == GLFW_PRESS)
    {
        double cx, cy;
        glfwGetCursorPos(window, &cx, &cy);
        lmbDown = true;

        if (nearSplit(cx)) {
            dragSplit = true; dragMapTop = false; dragIn3D = false;
            glfwSetCursor(window, gCurResize);
        } else if (nearMapTopSplit(cx, cy)) {
            dragMapTop = true; dragSplit = false; dragIn3D = false;
            glfwSetCursor(window, gCurVResize);
        } else {
            dragSplit = false; dragMapTop = false;
            dragIn3D  = !inMapPanel(cx);
        }
    }
    else
    {
        lmbDown = false; dragSplit = false; dragMapTop = false;
    }
}

static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos)
{
    ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);
    float dx = static_cast<float>(xpos) - lastMX;
    float dy = static_cast<float>(ypos) - lastMY;

    if (dragSplit) {
        splitFrac = glm::clamp(static_cast<float>(xpos) / WIN_W, 0.15f, 0.85f);
    } else if (dragMapTop) {
        mapTopFrac = glm::clamp(static_cast<float>(ypos) / WIN_H, 0.0f, 0.85f);
    } else if (lmbDown) {
        if (dragIn3D) {
            camYaw   -= dx * 0.4f;
            camPitch  = glm::clamp(camPitch + dy * 0.4f, -89.0f, 89.0f);
        } else {
            float ppd = gMap.pixPerDeg(WIN_H);
            gMap.ctrLon -= dx / ppd;
            gMap.ctrLat += dy / ppd;
            gMap.clampLat();
        }
    } else {
        if      (nearSplit(xpos))           glfwSetCursor(window, gCurResize);
        else if (nearMapTopSplit(xpos,ypos)) glfwSetCursor(window, gCurVResize);
        else                                glfwSetCursor(window, gCurArrow);
    }

    lastMX = static_cast<float>(xpos);
    lastMY = static_cast<float>(ypos);
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);
    if (ImGui::GetIO().WantCaptureMouse) return;

    double cx, cy;
    glfwGetCursorPos(window, &cx, &cy);

    if (!inMapPanel(cx)) {
        camRadius = glm::clamp(camRadius - static_cast<float>(yoffset) * 0.25f,
                               1.2f, 10.0f);
    } else if (!nearSplit(cx)) {
        float factor = (yoffset > 0) ? 1.15f : (1.0f / 1.15f);
        gMap.zoom = glm::clamp(gMap.zoom * factor,
                               Planisphere::ZOOM_MIN, Planisphere::ZOOM_MAX);
        gMap.clampLat();
    }
}

// ── Clavier ───────────────────────────────────────────────────────────────────
//   ESC / Q  : quitter
//   Espace   : pause / reprendre
//   + / =    : vitesse ×2
//   - / _    : vitesse ÷2
//   R        : réinitialiser le temps
static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    // Forward vers ImGui en premier — il met à jour WantCaptureKeyboard.
    ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);

    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

    // ESC ferme toujours, même si un champ ImGui est focalisé.
    if (key == GLFW_KEY_ESCAPE) {
        glfwSetWindowShouldClose(window, true);
        return;
    }

    // Les raccourcis de simulation ne s'activent QUE si ImGui n'a pas le focus
    // clavier, pour ne pas interférer avec la saisie dans les formulaires.
    if (ImGui::GetIO().WantCaptureKeyboard) return;

    switch (key)
    {
    case GLFW_KEY_SPACE:
        if (action == GLFW_PRESS)
            simPaused = !simPaused;
        break;

    // Accélérer (+ pavé num ou = azerty)
    case GLFW_KEY_KP_ADD:
    case GLFW_KEY_EQUAL:
        simSpeed = std::min(simSpeed * 2.0, 16384.0);
        break;

    // Ralentir (- pavé num ou - azerty)
    case GLFW_KEY_KP_SUBTRACT:
    case GLFW_KEY_MINUS:
        simSpeed = std::max(simSpeed / 2.0, 1.0);
        break;

    // Réinitialiser le temps de simulation
    case GLFW_KEY_R:
        if (action == GLFW_PRESS)
            simTime = 0.0;
        break;

    default:
        break;
    }
}


// =============================================================================
// Sources GLSL
// =============================================================================

static const char* VERT3D = R"glsl(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uMVP;
uniform mat4 uModel;
out vec3 vNormal;
out vec3 vFragPos;
void main()
{
    vFragPos = vec3(uModel * vec4(aPos, 1.0));
    vNormal  = mat3(transpose(inverse(uModel))) * aNormal;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)glsl";

static const char* FRAG3D = R"glsl(
#version 330 core
in  vec3 vNormal;
in  vec3 vFragPos;
uniform vec3 uSunDir;
uniform vec3 uColor;
out vec4 FragColor;
void main()
{
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uSunDir);
    float diff = max(dot(N, L), 0.0);
    vec3 ambient  = 0.07 * uColor;
    vec3 diffuse  = diff * uColor;
    vec3 viewDir  = normalize(-vFragPos);
    vec3 halfDir  = normalize(L + viewDir);
    float spec    = pow(max(dot(N, halfDir), 0.0), 64.0);
    vec3 specular = 0.18 * vec3(1.0) * spec;
    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
)glsl";

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
        std::cerr << "Erreur shader : " << buf << "\n";
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
    if (!glfwInit()) { std::cerr << "Échec glfwInit\n"; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    gWindow = glfwCreateWindow(WIN_W, WIN_H, "OrbitalSim", nullptr, nullptr);
    if (!gWindow) { std::cerr << "Échec glfwCreateWindow\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(gWindow);
    glfwSwapInterval(1);

    gCurResize  = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
    gCurVResize = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
    gCurArrow   = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::cerr << "Échec GLAD\n"; return -1;
    }

    // ── Dear ImGui ────────────────────────────────────────────────────────────
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding    = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.GrabRounding      = 2.0f;
    style.Colors[ImGuiCol_Text] = ImVec4(0.88f, 0.92f, 0.98f, 1.0f);

    ImFont* font = io.Fonts->AddFontFromFileTTF(
        "C:\\Windows\\Fonts\\segoeui.ttf", 14.0f);
    if (!font) io.Fonts->AddFontDefault();

    ImGui_ImplGlfw_InitForOpenGL(gWindow, /*install_callbacks=*/false);
    ImGui_ImplOpenGL3_Init("#version 330");

    // ── Callbacks ─────────────────────────────────────────────────────────────
    glfwSetFramebufferSizeCallback(gWindow, framebuffer_size_callback);
    glfwSetMouseButtonCallback    (gWindow, mouse_button_callback);
    glfwSetCursorPosCallback      (gWindow, cursor_pos_callback);
    glfwSetScrollCallback         (gWindow, scroll_callback);
    glfwSetKeyCallback            (gWindow, key_callback);

    // ── États OpenGL ──────────────────────────────────────────────────────────
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // ── Shaders ───────────────────────────────────────────────────────────────
    GLuint shader3D     = create_program(VERT3D,      FRAG3D);
    GLuint shaderFlat3D = create_program(VERT_FLAT3D, FRAG_FLAT3D);
    GLuint shader2D     = create_program(VERT2D,      FRAG2D);

    GLint loc3D_MVP   = glGetUniformLocation(shader3D,     "uMVP");
    GLint loc3D_Model = glGetUniformLocation(shader3D,     "uModel");
    GLint loc3D_Sun   = glGetUniformLocation(shader3D,     "uSunDir");
    GLint loc3D_Color = glGetUniformLocation(shader3D,     "uColor");
    GLint locF_MVP    = glGetUniformLocation(shaderFlat3D, "uMVP");
    GLint locF_Color  = glGetUniformLocation(shaderFlat3D, "uColor");
    GLint loc2D_Proj  = glGetUniformLocation(shader2D,     "uProj");
    GLint loc2D_Color = glGetUniformLocation(shader2D,     "uColor");

    // ── Géométries ────────────────────────────────────────────────────────────
    SphereGPU     sphere = build_sphere(1.0f, 48, 64);
    DynBuf2D      dyn2   = make_dyn2d();
    DynBuf3D      dyn3   = make_dyn3d();
    Graticule3DGPU grat3D = build_graticule3D(1.005f, 72);

    const glm::vec3 sunDir = glm::normalize(glm::vec3(1.6f, 0.8f, 0.7f));

    // ── WayPoints ─────────────────────────────────────────────────────────────
    gWaypoints.push_back({ "Paris",    48.85f,  2.35f, { 1.0f, 0.85f, 0.2f,  1.0f }, 8.0f,  0.055f, WPShape::Hexagon });
    gWaypoints.push_back({ "Pole Nord", 90.0f,  0.0f,  { 1.0f, 0.22f, 0.18f, 1.0f }, 10.0f, 0.12f,  WPShape::Cross   });
    gWaypoints.push_back({ "Pole Sud", -90.0f,  0.0f,  { 0.22f,0.55f, 1.0f,  1.0f }, 10.0f, 0.12f,  WPShape::Cross   });

    std::vector<std::vector<float>> waypointMesh3D;
    for (const auto& wp : gWaypoints)
        waypointMesh3D.push_back(wp.markerSphere());

    // ── Palette de couleurs satellites ────────────────────────────────────────
    static const glm::vec4 kPalette[] = {
        { 1.0f, 0.85f, 0.20f, 1.0f },
        { 0.20f, 1.0f, 0.55f, 1.0f },
        { 0.35f, 0.70f, 1.0f, 1.0f },
        { 1.0f, 0.45f, 0.10f, 1.0f },
        { 0.80f, 0.25f, 1.0f, 1.0f },
        { 1.0f, 0.30f, 0.45f, 1.0f },
    };

    // ── Initialise le timer de simulation ─────────────────────────────────────
    wallPrev = glfwGetTime();

    // ── Helper : croix 3D 6 sommets ──────────────────────────────────────────
    auto makeCross3 = [](glm::vec3 p, float r) -> std::vector<float> {
        return {
            p.x-r, p.y,   p.z,    p.x+r, p.y,   p.z,
            p.x,   p.y-r, p.z,    p.x,   p.y+r, p.z,
            p.x,   p.y,   p.z-r,  p.x,   p.y,   p.z+r,
        };
    };


    // =========================================================================
    // Boucle de rendu
    // =========================================================================
    while (!glfwWindowShouldClose(gWindow))
    {
        glfwPollEvents();

        // ── Avancer le temps de simulation ───────────────────────────────────
        {
            double wallNow = glfwGetTime();
            double wallDt  = wallNow - wallPrev;
            wallPrev       = wallNow;
            // Clamp du dt pour éviter les sauts après lag / focus perdu
            wallDt = std::min(wallDt, 0.1);

            if (!simPaused)
                simTime = std::fmod(simTime + wallDt * simSpeed, SIM_DURATION);
        }

        // ── Nouvelle frame ImGui ──────────────────────────────────────────────
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        int fbW, fbH;
        glfwGetFramebufferSize(gWindow, &fbW, &fbH);

        int mapTopY = static_cast<int>(mapTopFrac * fbH);
        gMap.panelTop = mapTopY;
        int splitX    = static_cast<int>(splitFrac * fbW);

        // ── Clear global ──────────────────────────────────────────────────────
        glScissor(0, 0, fbW, fbH);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // =====================================================================
        // PANNEAU GAUCHE — Vue 3D
        // =====================================================================
        //
        // La hauteur utile exclut la barre de temps (BAR_H pixels en bas).
        const float BAR_H = 54.0f;

        glViewport(0, 0, splitX, fbH);
        glScissor (0, 0, splitX, fbH);
        glEnable(GL_DEPTH_TEST);

        glClearColor(0.04f, 0.05f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ── Matrices 3D ───────────────────────────────────────────────────────
        float yRad = glm::radians(camYaw);
        float pRad = glm::radians(camPitch);
        glm::vec3 camPos = {
            camRadius * std::cos(pRad) * std::sin(yRad),
            camRadius * std::sin(pRad),
            camRadius * std::cos(pRad) * std::cos(yRad)
        };

        float     aspect3D = static_cast<float>(splitX) / static_cast<float>(fbH);
        glm::mat4 proj3D   = glm::perspective(glm::radians(45.0f), aspect3D, 0.1f, 100.0f);
        glm::mat4 view3D   = glm::lookAt(camPos, glm::vec3(0.0f), glm::vec3(0,1,0));

        // ── Matrice de rotation de la Terre ───────────────────────────────────
        //   ω_E × simTime [rad] — autour de Y (pôle Nord)
        const float earthAngle = static_cast<float>(simTime * OMEGA_EARTH);
        glm::mat4 modelEarth   = glm::rotate(glm::mat4(1.0f),
                                              earthAngle,
                                              glm::vec3(0.0f, 1.0f, 0.0f));

        // MVP pour les éléments ECEF (sphère, graticule, WayPoints) → tournent
        glm::mat4 mvpEarth = proj3D * view3D * modelEarth;
        // MVP pour les éléments ECI (orbites, trajectoires) → fixes dans l'espace
        glm::mat4 mvpECI   = proj3D * view3D;   // model = identité

        // ── Sphère terrestre (shader Phong, ECEF) ────────────────────────────
        glUseProgram(shader3D);
        glUniformMatrix4fv(loc3D_MVP,   1, GL_FALSE, glm::value_ptr(mvpEarth));
        glUniformMatrix4fv(loc3D_Model, 1, GL_FALSE, glm::value_ptr(modelEarth));
        glUniform3fv(loc3D_Sun,   1, glm::value_ptr(sunDir));
        glUniform3f (loc3D_Color, 0.14f, 0.40f, 0.80f);

        glBindVertexArray(sphere.vao);
        glDrawElements(GL_TRIANGLES, sphere.count, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);

        // ── Éléments ECEF (quadrillage + WayPoints) — tournent avec la Terre ──
        glUseProgram(shaderFlat3D);
        glUniformMatrix4fv(locF_MVP, 1, GL_FALSE, glm::value_ptr(mvpEarth));

        draw_graticule3D(grat3D, locF_Color);

        for (int i = 0; i < static_cast<int>(gWaypoints.size()); ++i)
            draw_3d(dyn3, waypointMesh3D[i], gWaypoints[i].glMode(),
                    locF_Color, gWaypoints[i].color);

        // ── Éléments ECI (orbites + trajectoires propagées) — fixes ──────────
        glUniformMatrix4fv(locF_MVP, 1, GL_FALSE, glm::value_ptr(mvpECI));

        for (const auto& sat : gSatellites)
        {
            // Ellipse képlérienne (grise, tirets)
            draw_3d(dyn3, sat.orbitVerts, GL_LINE_LOOP, locF_Color,
                    { sat.color.r, sat.color.g, sat.color.b, 0.30f });

            // Ligne des nœuds
            std::vector<float> nodeLine = {
                sat.anECI.x, sat.anECI.y, sat.anECI.z,
                sat.dnECI.x, sat.dnECI.y, sat.dnECI.z,
            };
            draw_3d(dyn3, nodeLine, GL_LINES, locF_Color,
                    { 0.15f, 0.90f, 0.35f, 0.40f });

            // Apogée / Périgée / nœuds
            draw_3d(dyn3, makeCross3(sat.apECI, 0.030f), GL_LINES, locF_Color,
                    { 0.35f, 0.72f, 1.0f, 0.80f });
            draw_3d(dyn3, makeCross3(sat.peECI, 0.030f), GL_LINES, locF_Color,
                    { 1.0f, 0.55f, 0.15f, 0.80f });
            draw_3d(dyn3, makeCross3(sat.anECI, 0.025f), GL_LINES, locF_Color,
                    { 0.15f, 1.0f, 0.40f, 0.70f });
            draw_3d(dyn3, makeCross3(sat.dnECI, 0.025f), GL_LINES, locF_Color,
                    { 1.0f, 0.28f, 0.28f, 0.70f });

            // ── Trajectoire 3D : ±1 période orbitale autour du temps courant ──
            // Seule une fenêtre temporelle [t−T, t+T] est affichée pour rester
            // lisible. Passé = sombre, futur = couleur vive.
            if (!sat.trackVerts.empty())
            {
                const int    nPts    = static_cast<int>(sat.track.size());
                const double dtTrack = (nPts > 1) ? (sat.track[1].t - sat.track[0].t) : 30.0;
                const double T       = static_cast<double>(sat.orbital.period_s);

                // Index courant
                const int iCur = std::min(static_cast<int>(simTime / dtTrack), nPts - 1);

                // Bornes ±1T clampées à [0, nPts−1]
                const int iPast = std::max(0,       static_cast<int>((simTime - T) / dtTrack));
                const int iFut  = std::min(nPts - 1,static_cast<int>((simTime + T) / dtTrack));

                // Passé [iPast … iCur]
                if (iCur > iPast)
                {
                    std::vector<float> past(sat.trackVerts.begin() + iPast * 3,
                                            sat.trackVerts.begin() + (iCur + 1) * 3);
                    draw_3d(dyn3, past, GL_LINE_STRIP, locF_Color,
                            { sat.color.r * 0.45f, sat.color.g * 0.45f,
                              sat.color.b * 0.45f, 0.60f });
                }

                // Futur [iCur … iFut]
                if (iFut > iCur)
                {
                    std::vector<float> future(sat.trackVerts.begin() + iCur * 3,
                                              sat.trackVerts.begin() + (iFut + 1) * 3);
                    draw_3d(dyn3, future, GL_LINE_STRIP, locF_Color,
                            { sat.color.r, sat.color.g, sat.color.b, 0.80f });
                }
            }

            // ── Position courante (grande croix brillante) ──────────────────
            glm::vec3 curPos = sat.posAtTime(simTime);
            draw_3d(dyn3, makeCross3(curPos, 0.060f), GL_LINES, locF_Color,
                    sat.color);
        }

        // ── Projection ortho pleine fenêtre ───────────────────────────────────
        glm::mat4 proj2D_full = glm::ortho(0.0f, static_cast<float>(fbW),
                                            static_cast<float>(fbH), 0.0f,
                                            -1.0f, 1.0f);
        glDisable(GL_DEPTH_TEST);

        // =====================================================================
        // MENU
        // =====================================================================
        if (mapTopY > 0)
        {
            glViewport(0, 0, fbW, fbH);
            glScissor(splitX, fbH - mapTopY, fbW - splitX, mapTopY);
            glUseProgram(shader2D);
            glUniformMatrix4fv(loc2D_Proj, 1, GL_FALSE, glm::value_ptr(proj2D_full));
            gMenu.draw(dyn2, loc2D_Color, splitX, fbW, fbH, mapTopY);

            if (gMenu.drawImGui(splitX, fbW, mapTopY))
            {
                Satellite sat;
                sat.orbital  = gMenu.pendingOrbit;
                sat.physical = gMenu.pendingPhysics;
                sat.name     = "SAT-" + std::to_string(gSatellites.size() + 1);
                sat.color    = kPalette[gSatellites.size() % 6];
                sat.buildGeometry();
                sat.propagate();   // ← calcul RK4+J2 sur 3 jours
                gSatellites.push_back(std::move(sat));
            }
        }

        // =====================================================================
        // PANNEAU DROIT — Planisphère
        // =====================================================================
        {
            int panelH = fbH - mapTopY;
            glViewport(splitX, 0, fbW - splitX, panelH);
            glScissor (splitX, 0, fbW - splitX, panelH);

            glm::mat4 proj2D_panel = glm::ortho(
                static_cast<float>(splitX), static_cast<float>(fbW),
                static_cast<float>(fbH),    static_cast<float>(mapTopY),
                -1.0f, 1.0f);

            glUseProgram(shader2D);
            glUniformMatrix4fv(loc2D_Proj, 1, GL_FALSE, glm::value_ptr(proj2D_panel));

            gMap.drawBackground(dyn2, loc2D_Color, splitX, fbW, fbH);
            gMap.drawGraticule (dyn2, loc2D_Color, splitX, fbW, fbH);

            // ── Ground tracks des satellites ──────────────────────────────────
            //
            // Nombre de périodes affiché : contrôlé par gNbPeriodsIdx.
            // La fenêtre temporelle est [simTime − n×T, simTime + n×T], ou
            // [0, SIM_DURATION] si "Tout" est sélectionné.
            //
            // Coupure antiméridien : on détecte un saut |Δlon| > 90° entre deux
            // points consécutifs et on démarre un nouveau segment (les orbites LEO
            // avancent de ~1°/s max, donc 90° signifie bien un wrap ±180°).
            for (const auto& sat : gSatellites)
            {
                if (sat.track.empty()) continue;

                const int    nPts    = static_cast<int>(sat.track.size());
                const double dtTrack = (nPts > 1) ? (sat.track[1].t - sat.track[0].t) : 30.0;
                const int    iCur    = std::min(static_cast<int>(simTime / dtTrack), nPts - 1);
                const double T       = static_cast<double>(sat.orbital.period_s);
                const int    mult    = kPeriodMult[gNbPeriodsIdx];  // -1 = tout

                // Bornes de la fenêtre en indices
                int iPastStart, iFutEnd;
                if (mult < 0) {
                    iPastStart = 0;
                    iFutEnd    = nPts - 1;
                } else {
                    iPastStart = std::max(0,       static_cast<int>((simTime - mult * T) / dtTrack));
                    iFutEnd    = std::min(nPts - 1,static_cast<int>((simTime + mult * T) / dtTrack));
                }

                // Sous-échantillonnage : 1 point sur 2 (~4320 pts max)
                const int stride = 2;

                // Lambda : pousse le segment accumulé vers le GPU et le vide
                auto flushSeg = [&](std::vector<float>& seg, glm::vec4 col) {
                    if (seg.size() >= 4)
                        draw_2d(dyn2, seg, GL_LINE_STRIP, loc2D_Color, col);
                    seg.clear();
                };

                // Lambda : parcourt [iFrom, iTo] par stride et dessine les
                // segments en coupant à l'antiméridien.
                // lonOffset : −360, 0 ou +360 pour les 3 copies horizontales.
                auto drawTrackRange = [&](int iFrom, int iTo,
                                          glm::vec4 col, float lonOffset)
                {
                    std::vector<float> seg;
                    float prevLon = -9999.0f;
                    for (int i = iFrom; i <= iTo; i += stride)
                    {
                        float lat, lon;
                        sat.latLonAtTime(sat.track[i].t, lat, lon);
                        lon += lonOffset;
                        // Coupure antiméridien : saut > 90° entre deux points
                        // (l'offset est uniforme → la détection reste correcte)
                        if (prevLon > -9000.0f && std::abs(lon - prevLon) > 90.0f)
                            flushSeg(seg, col);
                        glm::vec2 px = gMap.project(lon, lat, splitX, fbW, fbH);
                        seg.push_back(px.x);
                        seg.push_back(px.y);
                        prevLon = lon;
                    }
                    flushSeg(seg, col);
                };

                // Couleurs passé / futur
                const glm::vec4 colPast = { sat.color.r * 0.55f,
                                             sat.color.g * 0.55f,
                                             sat.color.b * 0.55f, 0.70f };
                const glm::vec4 colFut  = { sat.color.r, sat.color.g,
                                             sat.color.b, 0.85f };

                // ── 3 copies horizontales (−360°, 0°, +360°) ──────────────────
                // Le scissor OpenGL découpe ce qui dépasse du panneau.
                for (int n = -1; n <= 1; ++n)
                {
                    const float off = static_cast<float>(n) * 360.0f;

                    // Passé [iPastStart … iCur]
                    drawTrackRange(iPastStart, iCur, colPast, off);

                    // Futur [iCur … iFutEnd]
                    drawTrackRange(iCur, iFutEnd, colFut, off);
                }

                // ── Position courante : croix sur les 3 copies ────────────────
                {
                    float lat, lon0;
                    sat.latLonAtTime(simTime, lat, lon0);

                    for (int n = -1; n <= 1; ++n)
                    {
                        const float lon = lon0 + static_cast<float>(n) * 360.0f;
                        glm::vec2 c = gMap.project(lon, lat, splitX, fbW, fbH);
                        const float r = 7.0f;
                        std::vector<float> cross = {
                            c.x - r, c.y,     c.x + r, c.y,
                            c.x,     c.y - r, c.x,     c.y + r
                        };
                        glLineWidth(2.0f);
                        draw_2d(dyn2, cross, GL_LINES, loc2D_Color, sat.color);
                        glLineWidth(1.0f);
                    }
                }
            }

            // WayPoints géographiques
            for (const auto& wp : gWaypoints)
                gMap.drawWaypoint(dyn2, loc2D_Color, splitX, fbW, fbH, wp);
        }

        // =====================================================================
        // SÉPARATEURS + POIGNÉES
        // =====================================================================
        glViewport(0, 0, fbW, fbH);
        glScissor (0, 0, fbW, fbH);
        glUseProgram(shader2D);
        glUniformMatrix4fv(loc2D_Proj, 1, GL_FALSE, glm::value_ptr(proj2D_full));
        {
            float sx  = static_cast<float>(splitX);
            float mty = static_cast<float>(mapTopY);
            float fw  = static_cast<float>(fbW);
            float fh  = static_cast<float>(fbH);

            draw_2d(dyn2, make_rect(sx - 1.0f, 0.0f, sx + 1.0f, fh),
                    GL_TRIANGLES, loc2D_Color, { 0.50f, 0.72f, 1.0f, 1.0f });
            float midV = fh * 0.5f;
            draw_2d(dyn2, make_rect(sx - 4.0f, midV - 20.0f, sx + 4.0f, midV + 20.0f),
                    GL_TRIANGLES, loc2D_Color, { 0.70f, 0.85f, 1.0f, 1.0f });

            if (mapTopY > 0) {
                draw_2d(dyn2, make_rect(sx, mty - 1.0f, fw, mty + 1.0f),
                        GL_TRIANGLES, loc2D_Color, { 0.50f, 0.72f, 1.0f, 1.0f });
                float midH = sx + (fw - sx) * 0.5f;
                draw_2d(dyn2, make_rect(midH - 20.0f, mty - 4.0f, midH + 20.0f, mty + 4.0f),
                        GL_TRIANGLES, loc2D_Color, { 0.70f, 0.85f, 1.0f, 1.0f });
            }
        }

        // =====================================================================
        // BARRE DE SIMULATION (bas du panneau 3D)
        // =====================================================================
        //
        // Fenêtre ImGui collée en bas du panneau 3D.
        // Contient : ▶/⏸ + slider + T+JJ HH:MM:SS + boutons -/+ vitesse
        // Touches : Espace = pause, +/- = vitesse, R = reset
        {
            ImGui::SetNextWindowPos (ImVec2(0.0f, static_cast<float>(fbH) - BAR_H),
                                     ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(static_cast<float>(splitX), BAR_H),
                                     ImGuiCond_Always);

            ImGui::PushStyleColor(ImGuiCol_WindowBg,    ImVec4(0.04f, 0.06f, 0.12f, 0.92f));
            ImGui::PushStyleColor(ImGuiCol_Border,      ImVec4(0.25f, 0.45f, 0.70f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg,     ImVec4(0.10f, 0.18f, 0.32f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.15f, 0.28f, 0.48f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_SliderGrab,  ImVec4(0.40f, 0.70f, 1.00f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.55f, 0.85f, 1.00f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Button,       ImVec4(0.12f, 0.22f, 0.38f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(0.20f, 0.38f, 0.62f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.30f, 0.55f, 0.90f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(8.0f, 5.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,      ImVec2(5.0f, 4.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize,      10.0f);

            const ImGuiWindowFlags kBarFlags =
                ImGuiWindowFlags_NoTitleBar      |
                ImGuiWindowFlags_NoResize        |
                ImGuiWindowFlags_NoMove          |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoScrollbar;

            if (ImGui::Begin("##timeline", nullptr, kBarFlags))
            {
                // ── Ligne 1 : bouton ▶/⏸  +  slider de temps ────────────────
                const char* playLbl = simPaused ? " \xe2\x96\xb6 " : "\xe2\x8f\xb8\xe2\x8f\xb8";  // ▶ / ⏸⏸ UTF-8
                if (ImGui::Button(playLbl, ImVec2(32.0f, 20.0f)))
                    simPaused = !simPaused;
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Espace");

                ImGui::SameLine(0.0f, 6.0f);

                float t_f   = static_cast<float>(simTime);
                float dur_f = static_cast<float>(SIM_DURATION);
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                if (ImGui::SliderFloat("##simslider", &t_f, 0.0f, dur_f, ""))
                    simTime = static_cast<double>(t_f);
                // Si l'utilisateur tire le slider, la valeur est déjà appliquée.
                if (ImGui::IsItemActive())
                    simTime = static_cast<double>(t_f);

                // ── Ligne 2 : T+ JJ HH:MM:SS  |  vitesse  |  - x +  |  hint ──
                const int total_s = static_cast<int>(simTime);
                const int days    = total_s / 86400;
                const int hh      = (total_s % 86400) / 3600;
                const int mm      = (total_s % 3600)  / 60;
                const int ss      = total_s % 60;
                char tbuf[40];
                std::snprintf(tbuf, sizeof(tbuf), "J+%d  %02d:%02d:%02d", days, hh, mm, ss);
                ImGui::TextColored(ImVec4(0.50f, 0.88f, 1.0f, 1.0f), "%s", tbuf);

                ImGui::SameLine(0.0f, 18.0f);

                // Bouton réduire vitesse
                if (ImGui::Button("- ##spd", ImVec2(22.0f, 18.0f)))
                    simSpeed = std::max(simSpeed / 2.0, 1.0);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Réduire vitesse (-)");

                ImGui::SameLine(0.0f, 4.0f);
                char sbuf[20];
                // Affiche "×N" avec N formaté proprement
                if (simSpeed < 1000.0)
                    std::snprintf(sbuf, sizeof(sbuf), "\xc3\x97%.0f   ", simSpeed);
                else
                    std::snprintf(sbuf, sizeof(sbuf), "\xc3\x97%.0fk  ", simSpeed / 1000.0);
                ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.20f, 1.0f), "%s", sbuf);

                ImGui::SameLine(0.0f, 4.0f);
                // Bouton augmenter vitesse
                if (ImGui::Button("+ ##spd", ImVec2(22.0f, 18.0f)))
                    simSpeed = std::min(simSpeed * 2.0, 16384.0);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Augmenter vitesse (+)");

                ImGui::SameLine(0.0f, 16.0f);
                // Sélecteur du nombre de périodes affichées sur la carte
                ImGui::TextDisabled("Trace:");
                ImGui::SameLine(0.0f, 4.0f);
                ImGui::SetNextItemWidth(58.0f);
                ImGui::Combo("##nbT", &gNbPeriodsIdx, kPeriodLabels, 5);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Nombre de périodes affichées\nsur la trace au sol (planisphère)");

                ImGui::SameLine(0.0f, 12.0f);
                ImGui::TextDisabled("[Espace] [+/-] [R]");
            }
            ImGui::End();

            ImGui::PopStyleVar  (4);
            ImGui::PopStyleColor(9);
        }

        // ── Rendu ImGui ───────────────────────────────────────────────────────
        glViewport(0, 0, fbW, fbH);
        glScissor (0, 0, fbW, fbH);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(gWindow);
    }

    // ── Nettoyage ─────────────────────────────────────────────────────────────
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

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
