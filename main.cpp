#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Modules de simulation
#include "simulation/orbit.h"
#include "simulation/physics.h"
#include "simulation/vehicle.h"
#include "core/constants.h"

#include <iostream>
#include <vector>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <algorithm>

// ── Fenêtre ───────────────────────────────────────────────────────────────────
const int SCR_WIDTH  = 1200;
const int SCR_HEIGHT = 800;

// ── Caméra arcball ────────────────────────────────────────────────────────────
float camYaw    =  30.0f;
float camPitch  =  20.0f;
float camRadius =  4.5f;
bool  mouseDown = false;
float lastX = 0.0f, lastY = 0.0f;

void framebuffer_size_callback(GLFWwindow*, int w, int h)
{
    glViewport(0, 0, w, h);
}
void mouse_button_callback(GLFWwindow*, int button, int action, int)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT)
        mouseDown = (action == GLFW_PRESS);
}
void cursor_pos_callback(GLFWwindow*, double xpos, double ypos)
{
    static bool first = true;
    if (first) { lastX = (float)xpos; lastY = (float)ypos; first = false; }
    if (mouseDown) {
        float dx = (float)xpos - lastX;
        float dy = (float)ypos - lastY;
        camYaw += dx * 0.4f;
        camPitch = glm::clamp(camPitch + dy * 0.4f, -89.0f, 89.0f);
    }
    lastX = (float)xpos; lastY = (float)ypos;
}
void scroll_callback(GLFWwindow*, double, double yoffset)
{
    camRadius = glm::clamp(camRadius - (float)yoffset * 0.2f, 1.2f, 10.0f);
}
void key_callback(GLFWwindow* window, int key, int, int action, int)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

// ── Shaders GLSL ──────────────────────────────────────────────────────────────
const char* VERT_SRC = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    gl_PointSize = 8.0;
}
)";

const char* FRAG_SRC = R"(
#version 330 core
out vec4 FragColor;
uniform vec3 uColor;
void main() {
    FragColor = vec4(uColor, 1.0);
}
)";

static GLuint compile_shader(GLenum type, const char* src)
{
    GLuint id = glCreateShader(type);
    glShaderSource(id, 1, &src, nullptr);
    glCompileShader(id);
    GLint ok;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(id, 512, nullptr, log);
        std::cerr << "Erreur shader : " << log << "\n";
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

// ── Sphère wireframe ──────────────────────────────────────────────────────────
struct SphereMesh {
    std::vector<float>        vertices;
    std::vector<unsigned int> indices;
};

static SphereMesh make_sphere(float radius, int stacks, int slices)
{
    SphereMesh m;
    for (int i = 0; i <= stacks; ++i) {
        float phi = glm::pi<float>() * i / stacks;
        for (int j = 0; j <= slices; ++j) {
            float theta = glm::two_pi<float>() * j / slices;
            m.vertices.push_back(radius * std::sin(phi) * std::cos(theta));
            m.vertices.push_back(radius * std::cos(phi));
            m.vertices.push_back(radius * std::sin(phi) * std::sin(theta));
        }
    }
    // Arêtes horizontales
    for (int i = 0; i <= stacks; ++i)
        for (int j = 0; j < slices; ++j) {
            unsigned a = i*(slices+1)+j;
            m.indices.push_back(a); m.indices.push_back(a+1);
        }
    // Arêtes verticales
    for (int i = 0; i < stacks; ++i)
        for (int j = 0; j <= slices; ++j) {
            unsigned a = i*(slices+1)+j;
            m.indices.push_back(a); m.indices.push_back((i+1)*(slices+1)+j);
        }
    return m;
}

// ── Helpers GPU ───────────────────────────────────────────────────────────────

// Upload un tableau de vec3 en GL_LINE_STRIP (ou GL_POINTS).
// Retourne le VAO ; vbo est rempli.
static GLuint uploadVec3(const std::vector<glm::vec3>& pts, GLuint& vbo, GLenum usage = GL_STATIC_DRAW)
{
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(pts.size() * sizeof(glm::vec3)),
                 pts.empty() ? nullptr : pts.data(), usage);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    return vao;
}

// Upload 3 sommets (triangle GL_TRIANGLES).
static GLuint uploadTriangle(const glm::vec3 tri[3], GLuint& vbo)
{
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, 3 * sizeof(glm::vec3), tri, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    return vao;
}

// Met à jour le premier vec3 d'un VBO (satellite animé).
static void updateFirstVec3(GLuint vbo, const glm::vec3& pt)
{
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::vec3), &pt);
}

// ── Point de trajectoire ──────────────────────────────────────────────────────
struct TrajPoint {
    glm::vec3 posGL;   // position en unités GL  (1 unité = R_Terre)
    float     alt_km;  // altitude au-dessus de la surface (km)
    float     time_s;  // temps mission écoulé (s)
};

// ── Création d'un triangle marqueur ──────────────────────────────────────────
// pointUp = true  → apex pointe LOIN de la Terre  (apoastre ↑)
// pointUp = false → apex pointe VERS la Terre     (périastre ↓)
static void makeMarkerTriangle(const glm::vec3& posGL,
                               bool              pointUp,
                               const glm::vec3&  orbitNormal,
                               float             sizeGL,
                               glm::vec3         tri[3])
{
    glm::vec3 radial = glm::normalize(posGL);

    // Tangente : perpendiculaire au radial dans le plan orbital
    glm::vec3 tang = glm::cross(orbitNormal, radial);
    if (glm::length(tang) < 0.01f)
        tang = glm::cross(radial, glm::vec3(1.0f, 0.0f, 0.0f));
    tang = glm::normalize(tang);

    if (pointUp) {
        // Apex vers l'extérieur (apoastre)
        tri[0] = posGL + sizeGL         * radial;                           // sommet
        tri[1] = posGL - sizeGL * 0.5f  * radial + sizeGL * tang;          // base droite
        tri[2] = posGL - sizeGL * 0.5f  * radial - sizeGL * tang;          // base gauche
    } else {
        // Apex vers la Terre (périastre)
        tri[0] = posGL - sizeGL         * radial;                           // sommet
        tri[1] = posGL + sizeGL * 0.5f  * radial + sizeGL * tang;          // base droite
        tri[2] = posGL + sizeGL * 0.5f  * radial - sizeGL * tang;          // base gauche
    }
}

// ── main ──────────────────────────────────────────────────────────────────────
int main()
{
    // ═══════════════════════════════════════════════════════════════════════════
    // 1. PRÉ-CALCUL DE LA TRAJECTOIRE (avant l'init OpenGL)
    // ═══════════════════════════════════════════════════════════════════════════

    // Orbite initiale : 200 km, circulaire, i = 28.5°
    OrbitalElements elems0;
    elems0.a     = constants::R_EARTH + 200.0;   // km
    elems0.e     = 0.0;
    elems0.i     = 28.5 * constants::DEG2RAD;
    elems0.raan  = 0.0;
    elems0.omega = 0.0;
    elems0.nu    = 0.0;

    CartesianState cs0 = keplerianToCartesian(elems0);

    StateVec state = {
        cs0.pos.x, cs0.pos.y, cs0.pos.z,
        cs0.vel.x, cs0.vel.y, cs0.vel.z
    };

    VehicleParams vehicle = defaultCapsule();

    // Normale au plan orbital (fixée à t=0)
    glm::vec3 orbitNormal = glm::normalize(
        glm::cross(glm::vec3(cs0.pos), glm::vec3(cs0.vel))
    );

    const double dt_sim  = 60.0;              // pas d'intégration (s)
    const double t_max   = 30.0 * 86400.0;   // 30 jours

    std::vector<TrajPoint> trajectory;
    trajectory.reserve(50000);

    std::cout << "Calcul de la trajectoire (orbite 200 km, 30 jours)...\n";

    for (double t = 0.0; t <= t_max; t += dt_sim)
    {
        double r   = std::sqrt(state[0]*state[0] + state[1]*state[1] + state[2]*state[2]);
        double alt = r - constants::R_EARTH;   // km

        float sc = (float)constants::GL_SCALE;
        TrajPoint tp;
        tp.posGL  = { (float)(state[0]*sc), (float)(state[1]*sc), (float)(state[2]*sc) };
        tp.alt_km = (float)alt;
        tp.time_s = (float)t;
        trajectory.push_back(tp);

        if (alt < 0.0) break;   // impact sol

        state = physics::rk4Step(state, t, dt_sim, vehicle);
    }

    const int N = (int)trajectory.size();
    std::cout << "Terminé : " << N << " points, durée = "
              << std::fixed << std::setprecision(2)
              << trajectory.back().time_s / 86400.0 << " jours\n";
    std::cout << "Altitude finale : " << trajectory.back().alt_km << " km\n";

    // ───────────────────────────────────────────────────────────────────────────
    // 2. RECHERCHE DE L'APOASTRE ET DU PÉRIASTRE
    //    Sur la trajectoire complète : l'apoastre est le point le plus haut,
    //    le périastre le plus bas (qui peut être sous la surface si décroissance complète).
    // ───────────────────────────────────────────────────────────────────────────
    int   apoIdx = 0, periIdx = 0;
    float maxAlt = trajectory[0].alt_km;
    float minAlt = trajectory[0].alt_km;

    for (int i = 1; i < N; ++i) {
        if (trajectory[i].alt_km > maxAlt) { maxAlt = trajectory[i].alt_km; apoIdx  = i; }
        if (trajectory[i].alt_km < minAlt) { minAlt = trajectory[i].alt_km; periIdx = i; }
    }

    // Si apoastre et périastre sont au même endroit (orbite presque circulaire sans
    // décroissance notable), forcer apoastre au début et périastre à la fin.
    if (apoIdx == periIdx) {
        apoIdx  = 0;
        periIdx = N - 1;
    }

    std::cout << "Apoastre  : " << maxAlt << " km  (t = "
              << trajectory[apoIdx].time_s  / 3600.0 << " h)\n";
    std::cout << "Périastre : " << minAlt << " km  (t = "
              << trajectory[periIdx].time_s / 3600.0 << " h)\n";

    // ───────────────────────────────────────────────────────────────────────────
    // 3. SEGMENTATION : au-dessus / en-dessous de 150 km
    //    - pts_high : trajet ≥ 150 km  → dessiné en cyan
    //    - pts_low  : trajet < 150 km  → dessiné en rouge
    //    Si le périastre est sous la surface, pts_low descend jusqu'au sol.
    // ───────────────────────────────────────────────────────────────────────────
    std::vector<glm::vec3> pts_high, pts_low;
    bool wasLow = false;

    for (int i = 0; i < N; ++i) {
        if (trajectory[i].alt_km >= 150.0f) {
            pts_high.push_back(trajectory[i].posGL);
            wasLow = false;
        } else {
            // Connecter la transition haute→basse
            if (!wasLow && !pts_high.empty())
                pts_low.push_back(pts_high.back());
            pts_low.push_back(trajectory[i].posGL);
            wasLow = true;
        }
    }

    // ───────────────────────────────────────────────────────────────────────────
    // 4. TRIANGLES MARQUEURS
    // ───────────────────────────────────────────────────────────────────────────
    const float markerSize = 0.030f;   // ~191 km en unités GL

    glm::vec3 apoTri[3], periTri[3];
    makeMarkerTriangle(trajectory[apoIdx].posGL,  true,  orbitNormal, markerSize, apoTri);
    makeMarkerTriangle(trajectory[periIdx].posGL, false, orbitNormal, markerSize, periTri);

    // ═══════════════════════════════════════════════════════════════════════════
    // 5. INITIALISATION OPENGL / GLFW
    // ═══════════════════════════════════════════════════════════════════════════
    if (!glfwInit()) { std::cerr << "Échec glfwInit\n"; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
        "OrbitalSim — 200 km / 30 j / NRLMSISE-00", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Échec GLAD\n"; return -1;
    }

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback    (window, mouse_button_callback);
    glfwSetCursorPosCallback      (window, cursor_pos_callback);
    glfwSetScrollCallback         (window, scroll_callback);
    glfwSetKeyCallback            (window, key_callback);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_PROGRAM_POINT_SIZE);

    GLuint shader  = create_program(VERT_SRC, FRAG_SRC);
    GLint  locMVP  = glGetUniformLocation(shader, "uMVP");
    GLint  locCol  = glGetUniformLocation(shader, "uColor");

    // ═══════════════════════════════════════════════════════════════════════════
    // 6. UPLOAD GÉOMÉTRIE
    // ═══════════════════════════════════════════════════════════════════════════

    // Sphère Terre
    SphereMesh sphere = make_sphere(1.0f, 24, 36);
    GLuint earthVAO, earthVBO, earthEBO;
    glGenVertexArrays(1, &earthVAO);
    glGenBuffers(1, &earthVBO);
    glGenBuffers(1, &earthEBO);
    glBindVertexArray(earthVAO);
    glBindBuffer(GL_ARRAY_BUFFER, earthVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(sphere.vertices.size() * sizeof(float)),
                 sphere.vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, earthEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 (GLsizeiptr)(sphere.indices.size() * sizeof(unsigned)),
                 sphere.indices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    GLsizei earthIdx = (GLsizei)sphere.indices.size();

    // Trajectoire ≥ 150 km  (cyan)
    GLuint highVBO = 0, highVAO = 0;
    GLsizei highCnt = 0;
    if (!pts_high.empty()) {
        highVAO = uploadVec3(pts_high, highVBO);
        highCnt = (GLsizei)pts_high.size();
    }

    // Trajectoire < 150 km  (rouge)
    GLuint lowVBO = 0, lowVAO = 0;
    GLsizei lowCnt = 0;
    if (!pts_low.empty()) {
        lowVAO = uploadVec3(pts_low, lowVBO);
        lowCnt = (GLsizei)pts_low.size();
    }

    // Marqueur apoastre  (triangle jaune ↑)
    GLuint apoVBO, apoVAO = uploadTriangle(apoTri, apoVBO);

    // Marqueur périastre (triangle orange ↓)
    GLuint periVBO, periVAO = uploadTriangle(periTri, periVBO);

    // Satellite animé (point blanc, VBO dynamique)
    glm::vec3 satPos = trajectory[0].posGL;
    std::vector<glm::vec3> satVec = { satPos };
    GLuint satVBO, satVAO = uploadVec3(satVec, satVBO, GL_DYNAMIC_DRAW);

    // ═══════════════════════════════════════════════════════════════════════════
    // 7. BOUCLE DE RENDU
    // ═══════════════════════════════════════════════════════════════════════════
    double simTime      = 0.0;
    double timeScale    = 3600.0;          // 1 s réel = 1 h simulée
    double lastRealTime = glfwGetTime();
    int    satIdx       = 0;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // ── Avance du temps simulé ──
        double now     = glfwGetTime();
        double dt_real = now - lastRealTime;
        lastRealTime   = now;
        simTime       += dt_real * timeScale;

        float tEnd = trajectory.back().time_s;
        if (simTime > tEnd) simTime = 0.0;   // reboucle

        // Recherche de l'index correspondant dans la trajectoire
        while (satIdx + 1 < N && trajectory[satIdx + 1].time_s <= (float)simTime)
            ++satIdx;
        if (satIdx >= N) satIdx = 0;

        satPos = trajectory[satIdx].posGL;
        updateFirstVec3(satVBO, satPos);

        // ── Titre de fenêtre (télémétrie) ──
        {
            int days  = (int)(simTime / 86400.0);
            int hours = (int)((simTime - days * 86400.0) / 3600.0);
            int mins  = (int)((simTime - days * 86400.0 - hours * 3600.0) / 60.0);
            std::ostringstream ss;
            ss << "OrbitalSim | J+" << days
               << " " << std::setw(2) << std::setfill('0') << hours
               << "h" << std::setw(2) << std::setfill('0') << mins
               << "  |  Alt: " << std::fixed << std::setprecision(1)
               << trajectory[satIdx].alt_km << " km";
            glfwSetWindowTitle(window, ss.str().c_str());
        }

        // ── Effacement ──
        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ── Matrice MVP ──
        int W, H;
        glfwGetFramebufferSize(window, &W, &H);
        float aspect = (H > 0) ? (float)W / (float)H : 1.0f;

        float yRad = glm::radians(camYaw);
        float pRad = glm::radians(camPitch);
        glm::vec3 camPos = {
            camRadius * std::cos(pRad) * std::sin(yRad),
            camRadius * std::sin(pRad),
            camRadius * std::cos(pRad) * std::cos(yRad)
        };
        glm::mat4 proj  = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
        glm::mat4 view  = glm::lookAt(camPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 mvp   = proj * view;   // model = identité

        glUseProgram(shader);
        glUniformMatrix4fv(locMVP, 1, GL_FALSE, glm::value_ptr(mvp));

        // ── Terre (bleu foncé) ──
        glUniform3f(locCol, 0.15f, 0.45f, 0.75f);
        glBindVertexArray(earthVAO);
        glDrawElements(GL_LINES, earthIdx, GL_UNSIGNED_INT, nullptr);

        // ── Trajectoire ≥ 150 km (cyan clair) ──
        if (highVAO && highCnt > 1) {
            glUniform3f(locCol, 0.75f, 0.92f, 1.00f);
            glBindVertexArray(highVAO);
            glDrawArrays(GL_LINE_STRIP, 0, highCnt);
        }

        // ── Trajectoire < 150 km (rouge vif) ──
        if (lowVAO && lowCnt > 1) {
            glUniform3f(locCol, 1.00f, 0.15f, 0.10f);
            glBindVertexArray(lowVAO);
            glDrawArrays(GL_LINE_STRIP, 0, lowCnt);
        }

        // ── Marqueur apoastre : triangle jaune, apex LOIN de la Terre ──
        glUniform3f(locCol, 1.00f, 0.90f, 0.00f);
        glBindVertexArray(apoVAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // ── Marqueur périastre : triangle orange, apex VERS la Terre ──
        glUniform3f(locCol, 1.00f, 0.45f, 0.00f);
        glBindVertexArray(periVAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // ── Satellite (point blanc) ──
        glUniform3f(locCol, 1.00f, 1.00f, 1.00f);
        glBindVertexArray(satVAO);
        glDrawArrays(GL_POINTS, 0, 1);

        glBindVertexArray(0);
        glfwSwapBuffers(window);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // 8. NETTOYAGE
    // ═══════════════════════════════════════════════════════════════════════════
    glDeleteVertexArrays(1, &earthVAO);
    glDeleteBuffers(1, &earthVBO);
    glDeleteBuffers(1, &earthEBO);

    if (highVAO) { glDeleteVertexArrays(1, &highVAO); glDeleteBuffers(1, &highVBO); }
    if (lowVAO)  { glDeleteVertexArrays(1, &lowVAO);  glDeleteBuffers(1, &lowVBO);  }

    glDeleteVertexArrays(1, &apoVAO);  glDeleteBuffers(1, &apoVBO);
    glDeleteVertexArrays(1, &periVAO); glDeleteBuffers(1, &periVBO);
    glDeleteVertexArrays(1, &satVAO);  glDeleteBuffers(1, &satVBO);

    glDeleteProgram(shader);
    glfwTerminate();
    return 0;
}
